#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "kprintf.h"
#include "gdt.h"
#include "idt.h"

// Set the base revision to 6, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used,section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision=0
};


// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
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

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.

static const char *memmap_type_str(uint64_t type) {
    switch (type) {
        case 0: return "USABLE";
        case 1: return "RESERVED";
        case 2: return "ACPI_RECLAIMABLE";
        case 3: return "ACPI_NVS";
        case 4: return "BAD_MEMORY";
        case 5: return "BOOTLOADER_RECLAIMABLE";
        case 6: return "KERNEL_AND_MODULES";
        case 7: return "FRAMEBUFFER";
        case 8: return "EFI_RECLAIMABLE";
        default: return "UNKNOWN";
    }
}

void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Print a nice pattern to screen as an example.
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    volatile uint32_t *fb_ptr = framebuffer->address;
    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            uint32_t nX = x * 255 / framebuffer->width;
            uint32_t nY = y * 255 / framebuffer->height;
            fb_ptr[y * (framebuffer->pitch / 4) + x] = (nY << 8) | nX;
        }
    }

    

    //day-4 
    //testing serial alone
    //serial_init();
    //serial_write_string("hello from kernel\n");
    
    //testing kprintf
    kprintf("hello\n");
    kprintf("str=%s char=%c\n", "test", 'A');
    kprintf("dec=%d hex=%x\n", 255, 255);
    kprintf("neg=%d\n", -42);
    kprintf("zero=%d zerohex=%x literal%%\n", 0, 0);


    //day 5
    gdt_dump();

    //day-6
    idt_init();
    idt_dump();

    //day-8 test
    //volatile int *bad_ptr = (volatile int *)0x0;
    //*bad_ptr = 42;

    //day-9 
    if(memmap_request.response != NULL){
	size_t cnt=memmap_request.response->entry_count;
	for(size_t i = 0;i<cnt;i++){
		struct limine_memmap_entry *memmap_entry = memmap_request.response->entries[i];
		kprintf("region=%ld base=%lx length=%lx type=%d type=%s  \n",(int64_t)i, memmap_entry->base, memmap_entry->length, memmap_entry->type,  memmap_type_str(memmap_entry->type));

	}

    }

    // We're done, just hang...
    hcf();
}
