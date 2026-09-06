#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

//from gdt.c layout: entry 3 = user data (DPL=3), entry 4 = user code (DPL=3)
// Selector = index * 8, OR'd with 3 for RPL=3

#define USER_CS 0x23   // (4 * 8)|3
#define USER_DS 0x1B   // (3 * 8)|3  each entry is 8 byte



// fakes an interrupt(iretq) return frame and drops from ring 0 to ring 3. execution resumes at `entry` in ring 3.
void enter_usermode(uint64_t entry, uint64_t user_stack_top);



#endif
