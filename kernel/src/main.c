#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "kprintf.h"
#include "gdt.h"
#include "idt.h"
#include "physical_page_management.h"

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

    gdt_dump();

    idt_init();
    idt_dump();

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
    }

    hcf();
}
