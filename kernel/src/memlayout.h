#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

//kernel_image fixed by linker.ld (check file in linker script folder),top -2GiB of address space
#define KERNEL_VMA      0xffffffff80000000UL

// HHDM(physmap) — fixed by Limine, given at boot via hhdm_request
// base only;size = however much physical RAM exists (grows with hardware)
#define HHDM_BASE       0xffff800000000000UL

// Kernel heap — reserved range for future kmalloc (Day 24-25)
#define KHEAP_BASE      0xffff900000000000UL
#define KHEAP_SIZE      (512UL * 1024 * 1024)   // 512MB

// MMIO — reserved range for mapping device registers (framebuffer, later NIC)
#define MMIO_BASE       0xffffa00000000000UL
#define MMIO_SIZE       (16UL * 1024 * 1024)    // 16MB

#endif
