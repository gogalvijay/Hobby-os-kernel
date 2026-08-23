#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "kprintf.h"
#include "gdt.h"
#include "idt.h"
#include "physical_page_management.h"
#include "hhdm.h"
#include "page_alloc.h"
#include "paging.h"
#include "stress_test.h"
#include "kheap.h"
#include "elf.h"
#include "task.h"
#include "usermode.h"

#define PGSIZE 4096
#define USER_STACK_TOP 0x0000000006000000ULL  

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;




static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    volatile uint32_t *fb_ptr = framebuffer->address;
    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            uint32_t nX = x * 255 / framebuffer->width;
            uint32_t nY = y * 255 / framebuffer->height;
            fb_ptr[y * (framebuffer->pitch / 4) + x] = (nY << 8) | nX;
        }
    }

    kprintf("hello\n");
    kprintf("str=%s char=%c\n", "test", 'A');
    kprintf("dec=%d hex=%x\n", 255, 255);
    kprintf("neg=%d\n", -42);
    kprintf("zero=%d zerohex=%x literal%%\n", 0, 0);

    gdt_init();
    gdt_dump();

    idt_init();
    idt_dump();

    if (hhdm_request.response != NULL) {
        hhdm_init(hhdm_request.response->offset);
        kprintf("hhdm offset=%lx\n", g_hhdm_offset);
    }

    if (memmap_request.response != NULL) {
        pmm_init(memmap_request.response);
        pmm_dump();

        void *p1 = boot_alloc(64);
        void *p2 = boot_alloc(4096);
        void *p3 = boot_alloc(1);
	void *p4 = boot_alloc(0x87000);

        kprintf("boot_alloc p1=%lx\n", (uint64_t)p1);
        kprintf("boot_alloc p2=%lx\n", (uint64_t)p2);
        kprintf("boot_alloc p3=%lx\n", (uint64_t)p3);
    	kprintf("boot_alloc p4=%lx\n", (uint64_t)p4);
    	
	frame_alloc_init(memmap_request.response);
	kprintf("free before=%ld\n", (int64_t)count_free());

	struct PageInfo *pp1 = page_alloc(0);
	struct PageInfo *pp2 = page_alloc(0);
	struct PageInfo *pp3 = page_alloc(0);

	kprintf("alloc pa1=%lx\n", page2pa(pp1));
	kprintf("alloc pa2=%lx\n", page2pa(pp2));
	kprintf("alloc pa3=%lx\n", page2pa(pp3));
	kprintf("free after 3 allocs=%ld\n", (int64_t)count_free());

	pp1->pp_ref = 0;
	pp2->pp_ref = 0;
	pp3->pp_ref = 0;
	page_free(pp1);
	page_free(pp2);
	page_free(pp3);

	kprintf("free after 3 frees=%ld\n", (int64_t)count_free());


	 
	//day-16 test
        uint64_t test_vaddr = 0xFFFF800000001000ULL;

        kprintf("pml4_index=%x\n", pml4_index(test_vaddr));
        kprintf("pdpt_index=%x\n", pdpt_index(test_vaddr));
        kprintf("pd_index=%x\n", pd_index(test_vaddr));
        kprintf("pt_index=%x\n", pt_index(test_vaddr));
        kprintf("is_canonical=%d\n", is_canonical(test_vaddr));

        uint64_t test_phys = 0x1234000ULL;
        uint64_t flags = 0x3;
        page_table_entry pte = pte_make(test_phys, flags);
        kprintf("pte=%lx\n", pte);
        kprintf("pte_get_addr=%lx\n", pte_get_addr(pte));
        kprintf("present=%d writable=%d\n", present(pte), writable(pte));

    
        //day17-19 tests
    	{	
        page_table_t *pml4 = get_current_pml4();
        kprintf("pml4=%lx\n", (uint64_t)pml4);

        uint64_t test_vaddr = 0x2000000ULL;

        struct PageInfo *pp = page_alloc(1);
        uint64_t test_phys = page2pa(pp);
        kprintf("test_phys=%lx\n", test_phys);

        uint64_t flags = 0;
        flags = flags | (1ULL << 0);
        flags = flags | (1ULL << 1);

        vmm_map(pml4, test_vaddr, test_phys, flags);

        page_table_entry *pte_before = vmm_walk(pml4, test_vaddr, false);
        if (pte_before == NULL) {
            kprintf("map failed: walk returned NULL\n");
        } else {
            kprintf("mapped entry=%lx\n", *pte_before);
            kprintf("mapped addr=%lx\n", pte_get_addr(*pte_before));
            kprintf("mapped present=%d writable=%d\n", present(*pte_before), writable(*pte_before));
        }

        vmm_unmap(pml4, test_vaddr);

        page_table_entry *pte_after = vmm_walk(pml4, test_vaddr, false);
        if (pte_after == NULL) {
            kprintf("after unmap: walk returned NULL\n");
        } else {
            kprintf("after unmap: entry=%lx present=%d\n", *pte_after, present(*pte_after));
        }
    	
    	}
    
	//day-23
        //phase1_stress_test();
	//
	{
		kheap_init();
		void *a = kmalloc(32);
		void *b = kmalloc(4096);
		kprintf("kmalloc a=%lx b=%lx\n", (uint64_t)a, (uint64_t)b);
		kfree(a);
		void *c = kmalloc(16);
		kprintf("kmalloc c=%lx\n", (uint64_t)c);
	}

	{
		// day 31-36
		page_table_t *original_pml4 = get_current_pml4();

		struct task *t = task_create();
		if (t == NULL) {
			kprintf("task_create failed\n");
		} else {
			kprintf("task_id=%ld new_pml4=%lx\n", (int64_t)t->task_id, (uint64_t)t->pml4);

			if (module_request.response == NULL || module_request.response->module_count < 2) {
				kprintf("user_test.elf module not found\n");
			} else {
				struct limine_file *mod = module_request.response->modules[1];
				uint64_t entry = 0;

				if (!elf_load(t->pml4, mod->address, &entry)) {
					kprintf("elf_load into task pml4 failed\n");
				} else {
					kprintf("elf_load ok, entry=%lx\n", entry);

					struct PageInfo *stack_pp = page_alloc(1);
					if (stack_pp == NULL) {
						kprintf("failed to allocate user stack page\n");
					} else {
						uint64_t stack_phys = page2pa(stack_pp);
						uint64_t stack_flags = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);

						vmm_map(t->pml4, USER_STACK_TOP - PGSIZE, stack_phys, stack_flags);

						current_task = t; 

						kprintf("entering ring 3 at entry=%lx stack=%lx\n",
							entry, (uint64_t)USER_STACK_TOP);

						vmm_switch_address_space(t->pml4);
						enter_usermode(entry, USER_STACK_TOP);

					}
				}
			}
		}

		(void)original_pml4;
	}

	{
		uint16_t tr;
		__asm__ volatile ("str %0" : "=r"(tr));
		kprintf("TR=%x (expect 28)\n", tr);
	}
    
    }

    hcf();
}
