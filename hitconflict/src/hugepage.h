#pragma once

// hugepage.h – 1-GB-Hugepage

#include <stdint.h>
#include <stddef.h>

// mmap 1-GB-Hugepage
uint8_t* alloc_hugepage(size_t size);

// munmap 1-GB-Hugepage
void free_hugepage(uint8_t* mem, size_t size);
