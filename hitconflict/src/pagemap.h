#pragma once

// pagemap.h
// Needs Root: without Root Linux returns PFN=0

#include <stdint.h>

uint64_t get_phys_base(void* virt_base);
