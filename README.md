# Hobby OS Kernel

A 64-bit hobby operating system kernel, built from scratch on top of the [Limine bootloader](https://github.com/limine-bootloader/limine), using MIT 6.828 (2018) labs 2-6 as a conceptual design reference (not literal JOS — this is a fresh 64-bit kernel built my own way).

Just building an OS kernel because I'm bored.

## Roadmap

Following a structured ~90-day roadmap, organized into phases:

- **Phase 0** — Toolchain, Limine hello world, GDT/IDT
- **Phase 1** — Physical & virtual memory management
- **Phase 2** — Kernel heap, ELF loading, user mode, syscalls
- **Phase 3** — Multiple processes, scheduling, fork(), IPC
- **Phase 4** — Storage, filesystem, spawn, shell
- **Phase 5** — Networking (stretch goal)


## Build & run

This repo is built on the [limine-c-template](https://github.com/limine-bootloader/limine-c-template) — see that project for full build dependency details (`xorriso`, `sgdisk`, `mtools`, a cross-capable Clang/LLVM or GCC toolchain).

```
./kernel/get-deps
make run
```

Boots via OVMF/QEMU with serial output routed to stdout for kernel-level debug logging.

## Why

Learning OS internals hands-on: bootloaders, paging, interrupts, processes, filesystems, and networking, by building each piece myself instead of just reading about it.
