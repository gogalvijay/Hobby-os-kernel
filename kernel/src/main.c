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

#include "pic.h"
#include "pit.h"
#include "timer.h"
#include "lapic.h"

#include "context_switch.h"

#include "scheduler.h"


#define CONTEXT_TEST_ITERS 5

#define PGSIZE 4096
#define USER_STACK_TOP 0x0000000006000000ULL


static volatile int task_a_yield_count = 0;
static volatile int task_b_yield_count = 0;
static struct task *context_test_task_a_ref;
static struct task *context_test_task_b_ref;
static struct task *context_test_current;

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

// Finds a Limine module whose path ENDS WITH the given suffix (e.g.
// "fork_test.elf"), so we never depend on a hardcoded module index that
// shifts every time a module is added/removed/reordered in limine.conf.
static struct limine_file *find_module(const char *suffix) {
    if (module_request.response == NULL) {
        return NULL;
    }

    size_t suffix_len = 0;
    while (suffix[suffix_len] != '\0') {
        suffix_len++;
    }

    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file *mod = module_request.response->modules[i];
        const char *path = mod->path;

        size_t path_len = 0;
        while (path[path_len] != '\0') {
            path_len++;
        }

        if (path_len < suffix_len) {
            continue;
        }

        const char *tail = path + (path_len - suffix_len);
        size_t j;
        for (j = 0; j < suffix_len; j++) {
            if (tail[j] != suffix[j]) {
                break;
            }
        }
        if (j == suffix_len) {
            return mod;
        }
    }

    return NULL;
}



static void context_test_task_a(void) {
    for (int i = 0; i < CONTEXT_TEST_ITERS; i++) {
        task_a_yield_count++;
        kprintf("[task A] running, yield_count=%d\n", task_a_yield_count);
        context_switch(context_test_task_a_ref, context_test_task_b_ref);
    }
    kprintf("[task A] finished all iterations, halting\n");
    for (;;) { __asm__ volatile ("hlt"); }
}

static void context_test_task_b(void) {
    for (int i = 0; i < CONTEXT_TEST_ITERS; i++) {
        task_b_yield_count++;
        kprintf("[task B] running, yield_count=%d\n", task_b_yield_count);
        context_switch(context_test_task_b_ref, context_test_task_a_ref);
    }
    kprintf("[task B] finished all iterations, halting\n");
    for (;;) { __asm__ volatile ("hlt"); }
}


static volatile uint64_t preempt_a_prints = 0;
static volatile uint64_t preempt_b_prints = 0;

static void preempt_test_task_a(void) {
    for (;;) {
        preempt_a_prints++;
        if (preempt_a_prints <= 5 || preempt_a_prints % 200000 == 0) {
            kprintf("[task A] tick=%ld prints=%ld\n",
                    (int64_t)timer_get_ticks(), (int64_t)preempt_a_prints);
        }
    }
}

static void preempt_test_task_b(void) {
    for (;;) {
        preempt_b_prints++;
        if (preempt_b_prints <= 5 || preempt_b_prints % 200000 == 0) {
            kprintf("[task B] tick=%ld prints=%ld\n",
                    (int64_t)timer_get_ticks(), (int64_t)preempt_b_prints);
        }
    }
}

static volatile uint64_t sched_prints[4] = {0, 0, 0, 0};

#define DEFINE_SCHED_TASK(NAME, IDX) \
static void NAME(void) { \
    for (;;) { \
        sched_prints[IDX]++; \
        if (sched_prints[IDX] <= 5 || sched_prints[IDX] % 200000 == 0) { \
            kprintf("[sched task %d] tick=%ld prints=%ld\n", \
                    IDX, (int64_t)timer_get_ticks(), (int64_t)sched_prints[IDX]); \
        } \
    } \
}

DEFINE_SCHED_TASK(sched_task_0, 0)
DEFINE_SCHED_TASK(sched_task_1, 1)
DEFINE_SCHED_TASK(sched_task_2, 2)
DEFINE_SCHED_TASK(sched_task_3, 3)


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

    if (module_request.response != NULL) {
        kprintf("module_count=%ld\n", (int64_t)module_request.response->module_count);
        for (uint64_t i = 0; i < module_request.response->module_count; i++) {
            kprintf("module[%ld] path=%s\n", (int64_t)i,
                    module_request.response->modules[i]->path);
        }
    } else {
        kprintf("module_request.response is NULL\n");
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

	{
		kheap_init();
		void *a = kmalloc(32);
		void *b = kmalloc(4096);
		kprintf("kmalloc a=%lx b=%lx\n", (uint64_t)a, (uint64_t)b);
		kfree(a);
		void *c = kmalloc(16);
		kprintf("kmalloc c=%lx\n", (uint64_t)c);
	}
	
	task_table_init();
	
	{
		kprintf("\n--- task table test (day 38-39) ---\n");

		struct task *t1 = task_create();
		struct task *t2 = task_create();
		struct task *t3 = task_create();
		kprintf("created t1_id=%ld t2_id=%ld t3_id=%ld\n",
			(int64_t)t1->task_id, (int64_t)t2->task_id, (int64_t)t3->task_id);
		task_dump();

		uint64_t freed_id = t2->task_id;
		task_destroy(t2);
		task_dump();

		struct task *t4 = task_create();
		if (t4 != NULL && t4->task_id == freed_id) {
			kprintf("PASS: free-list reuse works, new task reused id=%ld\n",
				(int64_t)t4->task_id);
		} else {
			kprintf("FAIL: expected reused id=%ld, got %ld\n",
				(int64_t)freed_id, t4 ? (int64_t)t4->task_id : -1);
		}
		task_dump();

		static struct task *stress_tasks[MAX_TASKS];
		int stress_count = 0;
		for (int i = 0; i < MAX_TASKS; i++) {
			struct task *t = task_create();
			if (t == NULL) {
				break;
			}
			stress_tasks[stress_count++] = t;
		}
		kprintf("filled %d more slots (table should be full now)\n", stress_count);

		struct task *overflow = task_create();
		if (overflow == NULL) {
			kprintf("PASS: task_create returned NULL when table was full\n");
		} else {
			kprintf("FAIL: task_create should have failed, got task_id=%ld\n",
				(int64_t)overflow->task_id);
		}

		for (int i = 0; i < stress_count; i++) {
			task_destroy(stress_tasks[i]);
		}
		task_destroy(t1);
		task_destroy(t3);
		task_destroy(t4);

		kprintf("after cleanup, dump should show nothing:\n");
		task_dump();
		kprintf("--- task table test done ---\n\n");
	}

	{
		uint16_t tr;
		__asm__ volatile ("str %0" : "=r"(tr));
		kprintf("TR=%x (expect 28)\n", tr);
	}



	/*
	{
		kprintf("\n--- round-robin scheduler test (day 45-46) ---\n");

		pic_remap();
		lapic_enable();
		pit_init(100);
		pic_clear_mask(0);

		scheduler_init();

		struct task *t0 = task_create_kernel(sched_task_0);
		struct task *t1 = task_create_kernel(sched_task_1);
		struct task *t2 = task_create_kernel(sched_task_2);
		struct task *t3 = task_create_kernel(sched_task_3);

		if (t0 == NULL || t1 == NULL || t2 == NULL || t3 == NULL) {
			kprintf("FAIL: could not create scheduler test tasks\n");
		} else {
			t0->state = TASK_RUNNING;
			current_task = t0;

			kprintf("enabling interrupts, starting task 0 "
				"(scheduler will pick who runs next via the real task table)...\n");

			__asm__ volatile ("sti");

			context_switch(NULL, t0);

			kprintf("ERROR: should never reach this line\n");
		}
	}
	*/

	/*
	{
		kprintf("\n--- preemptive timer test (day 43-44) ---\n");

		pic_remap();
		lapic_enable();
		pit_init(100);
		pic_clear_mask(0);

		uint8_t pic1_mask;
		__asm__ volatile ("inb $0x21, %%al" : "=a"(pic1_mask));
		kprintf("PIC1 mask after setup=0x%x (bit0 should be 0)\n", pic1_mask);

		struct task *pa = task_create_kernel(preempt_test_task_a);
		struct task *pb = task_create_kernel(preempt_test_task_b);

		if (pa == NULL || pb == NULL) {
			kprintf("FAIL: could not create preemption test tasks\n");
		} else {
			timer_set_round_robin_tasks(pa, pb);
			current_task = pa;

			kprintf("tick before manual int=%ld\n", (int64_t)timer_get_ticks());
			__asm__ volatile ("int $0x20");
			kprintf("tick after manual int=%ld (should be +1 if timer_isr works at all)\n",
				(int64_t)timer_get_ticks());

			kprintf("enabling interrupts, starting task A "
				"(neither task will ever call context_switch itself)...\n");

			__asm__ volatile ("sti");

			uint64_t rflags;
			__asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
			kprintf("rflags=%lx (bit9/IF should be 1)\n", rflags);

			uint8_t pic1_mask_after_sti;
			__asm__ volatile ("inb $0x21, %%al" : "=a"(pic1_mask_after_sti));
			kprintf("PIC1 mask after sti=0x%x (bit0 should still be 0)\n", pic1_mask_after_sti);

			context_switch(NULL, pa);

			kprintf("ERROR: should never reach this line\n");
		}
	}
	*/

	/*
	{
		kprintf("\n--- context switch test (day 40-42) ---\n");

		task_a_yield_count = 0;
		task_b_yield_count = 0;

		struct task *ta = task_create_kernel(context_test_task_a);
		struct task *tb = task_create_kernel(context_test_task_b);

		if (ta == NULL || tb == NULL) {
			kprintf("FAIL: could not create context-switch test tasks\n");
		} else {
			context_test_task_a_ref = ta;
			context_test_task_b_ref = tb;

			kprintf("switching into task A for the first time (old=NULL)...\n");
			context_switch(NULL, ta);

			kprintf("ERROR: should never reach this line\n");
		}
	}
	*/


/*	{
		kprintf("\n--- fork test (day 47-49) ---\n");

		pic_remap();
		lapic_enable();
		pit_init(100);
		pic_clear_mask(0);
		scheduler_init();

		struct task *t = task_create();
		if (t == NULL) {
			kprintf("fork test: task_create failed\n");
		} else {
			struct limine_file *mod = find_module("fork_test.elf");

			if (mod == NULL) {
				kprintf("fork_test.elf module not found (check limine.conf + build)\n");
			} else {
				uint64_t entry = 0;

				if (!elf_load(t->pml4, mod->address, &entry)) {
					kprintf("fork test: elf_load failed\n");
				} else {
					struct PageInfo *stack_pp = page_alloc(1);
					uint64_t stack_phys = page2pa(stack_pp);
					uint64_t stack_flags = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);

					vmm_map(t->pml4, USER_STACK_TOP - PGSIZE, stack_phys, stack_flags);

					current_task = t;
					t->state = TASK_RUNNING;

					__asm__ volatile ("sti");

					kprintf("fork test: entering ring 3, entry=%lx\n", entry);

					vmm_switch_address_space(t->pml4);
					enter_usermode(entry, USER_STACK_TOP);
				}
			}
		}
	}*/

	{
		kprintf("\n--- IPC test (day 50-51) ---\n");

		pic_remap();
		lapic_enable();
		pit_init(100);
		pic_clear_mask(0);
		scheduler_init();

		struct task *receiver = task_create();
		struct task *sender = task_create();

		struct limine_file *recv_mod = find_module("ipc_receiver.elf");
		struct limine_file *send_mod = find_module("ipc_sender.elf");

		if (receiver == NULL || sender == NULL || recv_mod == NULL || send_mod == NULL) {
			kprintf("IPC test: setup failed\n");
		} else {
			uint64_t recv_entry = 0, send_entry = 0;
			elf_load(receiver->pml4, recv_mod->address, &recv_entry);
			elf_load(sender->pml4, send_mod->address, &send_entry);

			struct PageInfo *recv_stack = page_alloc(1);
			struct PageInfo *send_stack = page_alloc(1);
			uint64_t stack_flags = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);

			vmm_map(receiver->pml4, USER_STACK_TOP - PGSIZE, page2pa(recv_stack), stack_flags);
			vmm_map(sender->pml4, USER_STACK_TOP - PGSIZE, page2pa(send_stack), stack_flags);

			kprintf("receiver task_id=%ld sender task_id=%ld\n",
				(int64_t)receiver->task_id, (int64_t)sender->task_id);

			//receiver->state = TASK_RUNNING;
	                task_prepare_user_entry(receiver, recv_entry, USER_STACK_TOP);
			task_prepare_user_entry(sender, send_entry, USER_STACK_TOP);

			kprintf("receiver task_id=%ld sender task_id=%ld\n",
				(int64_t)receiver->task_id, (int64_t)sender->task_id);

			current_task = receiver;
			receiver->state = TASK_RUNNING;

			__asm__ volatile ("sti");

			//context_switch(NULL, receiver);
			scheduler_switch_to(NULL, receiver);

			kprintf("ERROR: should never reach this line\n");
		}
	}

	{
		// day 31-36
		page_table_t *original_pml4 = get_current_pml4();

		struct task *t = task_create();
		if (t == NULL) {
			kprintf("task_create failed\n");
		} else {
			kprintf("task_id=%ld new_pml4=%lx\n", (int64_t)t->task_id, (uint64_t)t->pml4);

			struct limine_file *mod = find_module("user_test.elf");

			if (mod == NULL) {
				kprintf("user_test.elf module not found\n");
			} else {
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
    }

    hcf();
}
