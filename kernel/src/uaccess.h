#ifndef UACCESS_H
#define UACCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "paging.h"

bool uva_check_range(page_table_t *pml4, uint64_t uva, uint64_t len, bool need_write);

#endif
