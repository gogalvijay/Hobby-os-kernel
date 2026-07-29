#include "hhdm.h"

uint64_t g_hhdm_offset = 0;

void hhdm_init(uint64_t offset) {
    g_hhdm_offset = offset;
}
