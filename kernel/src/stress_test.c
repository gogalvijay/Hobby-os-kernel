#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stress_test.h"
#include "kprintf.h"
#include "page_alloc.h"
#include "paging.h"
#include "hhdm.h"

static void stress_panic(const char *msg) {
    kprintf("STRESS FAIL: %s\n", msg);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

/* ---------- Test 1: allocate until OOM, then free everything ---------- */

/* We don't have a heap to store a list of allocated pages in, so we build
 * a linked list using the first 8 bytes of each allocated page itself. */
static struct PageInfo *stress_alloc_all(void) {
    struct PageInfo *head = NULL;
    size_t count = 0;

    for (;;) {
        struct PageInfo *pp = page_alloc(0); /* no zero, we're touching all of RAM */
        if (pp == NULL) {
            break;
        }

        struct PageInfo **link_slot = (struct PageInfo **)phys_to_virt(page2pa(pp));
        *link_slot = head;
        head = pp;
        count++;
    }

    kprintf("stress: allocated %ld pages until OOM\n", (int64_t)count);
    return head;
}

static void stress_free_all(struct PageInfo *head) {
    size_t count = 0;

    while (head != NULL) {
        struct PageInfo **link_slot = (struct PageInfo **)phys_to_virt(page2pa(head));
        struct PageInfo *next = *link_slot;

        head->pp_ref = 0;
        page_free(head);

        head = next;
        count++;
    }

    kprintf("stress: freed %ld pages\n", (int64_t)count);
}

/* ---------- Test 2: random map/unmap with corruption checking ---------- */

#define STRESS_TEST_PAGES 64
/* Lower-half scratch region, far from anything else in use. */
#define STRESS_TEST_VBASE  0x0000700000000000ULL

static bool g_stress_mapped[STRESS_TEST_PAGES];

static uint32_t g_rng_state = 88172645u; /* fixed seed -> reproducible run */

static uint32_t stress_rand(void) {
    /* xorshift32 */
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

static void stress_unmap_idx(page_table_t *pml4, uint32_t idx) {
    uint64_t vaddr = STRESS_TEST_VBASE + (uint64_t)idx * 4096;

    page_table_entry *pte = vmm_walk(pml4, vaddr, false);
    if (pte == NULL || !present(*pte)) {
        stress_panic("expected mapping missing during unmap");
    }

    uint64_t phys = pte_get_addr(*pte);
    struct PageInfo *pp = pa2page(phys);

    vmm_unmap(pml4, vaddr);

    pp->pp_ref = 0;
    page_free(pp);

    g_stress_mapped[idx] = false;
}

static void stress_random_map_unmap(page_table_t *pml4, int iterations) {
    for (int i = 0; i < STRESS_TEST_PAGES; i++) {
        g_stress_mapped[i] = false;
    }

    for (int iter = 0; iter < iterations; iter++) {
        uint32_t idx = stress_rand() % STRESS_TEST_PAGES;
        uint64_t vaddr = STRESS_TEST_VBASE + (uint64_t)idx * 4096;

        if (!g_stress_mapped[idx]) {
            struct PageInfo *pp = page_alloc(1); /* zeroed */
            if (pp == NULL) {
                kprintf("stress_random: OOM on map, skipping iter=%d\n", iter);
                continue;
            }

            uint64_t phys = page2pa(pp);
            uint64_t flags = 0x3; /* present + writable */
            vmm_map(pml4, vaddr, phys, flags);

            uint32_t pattern = 0xA5A50000u | idx;
            *(volatile uint32_t *)vaddr = pattern;

            g_stress_mapped[idx] = true;
        } else {
            uint32_t expected = 0xA5A50000u | idx;
            uint32_t actual = *(volatile uint32_t *)vaddr;

            if (actual != expected) {
                kprintf("CORRUPTION idx=%d vaddr=%lx got=%x expected=%x\n",
                        idx, vaddr, actual, expected);
                stress_panic("data corruption detected in mapped page");
            }

            stress_unmap_idx(pml4, idx);
        }
    }

    /* Clean up anything still mapped at the end of the run. */
    for (uint32_t i = 0; i < STRESS_TEST_PAGES; i++) {
        if (g_stress_mapped[i]) {
            stress_unmap_idx(pml4, i);
        }
    }
}

/* ---------- Orchestration ---------- */

void phase1_stress_test(void) {
    kprintf("\n=== PHASE 1 STRESS TEST START ===\n");

    size_t free_start = count_free();
    kprintf("stress: free pages at start=%ld\n", (int64_t)free_start);

    /* --- Test 1: allocate-to-OOM / free-all round trip --- */
    struct PageInfo *all = stress_alloc_all();

    size_t free_at_oom = count_free();
    kprintf("stress: free pages at OOM=%ld (expect 0)\n", (int64_t)free_at_oom);
    if (free_at_oom != 0) {
        stress_panic("free list not empty at OOM");
    }

    stress_free_all(all);

    size_t free_after_round_trip = count_free();
    kprintf("stress: free pages after freeing all=%ld (expect %ld)\n",
            (int64_t)free_after_round_trip, (int64_t)free_start);
    if (free_after_round_trip != free_start) {
        stress_panic("leak detected in allocate-to-OOM round trip");
    }

    /* --- Test 2: random map/unmap with corruption checking --- */
    page_table_t *pml4 = get_current_pml4();
    stress_random_map_unmap(pml4, 5000);

    size_t free_after_random = count_free();
    int64_t consumed = (int64_t)free_start - (int64_t)free_after_random;

    kprintf("stress: free pages after random map/unmap=%ld (start=%ld, consumed=%ld)\n",
            (int64_t)free_after_random, (int64_t)free_start, consumed);

    /* consumed pages should only be the new PDPT/PD/PT page-table pages
     * needed to cover the scratch VA range (small, bounded number, e.g. ~3).
     * Anything beyond ~8 pages is a real leak, not page-table overhead. */
    if (consumed < 0 || consumed > 8) {
        stress_panic("unexpected free-page delta after random map/unmap (possible leak)");
    }

    kprintf("=== PHASE 1 STRESS TEST PASSED ===\n\n");
}
