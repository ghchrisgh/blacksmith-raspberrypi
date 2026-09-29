// hugepage.cpp – alloc/free 1-GB-Hugepage

#include "hugepage.h"

#include <stdio.h>
#include <sys/mman.h>

uint8_t* alloc_hugepage(size_t size)
{
    uint8_t* mem = (uint8_t*)mmap(
        NULL, size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB,
        -1, 0);

    if (mem == MAP_FAILED) {
        perror("[ ERR ] - mmap(MAP_HUGE_1GB). Try 'make setup' before running");
        return nullptr;
    }
    printf("[ LOG ] - mmap 1GB hugepage OK\n");

    // taskset 0x2, forces physical Pages to allocate
    for (size_t i = 0; i < size; i += 4096)
        mem[i] = 1;

    return mem;
}

void free_hugepage(uint8_t* mem, size_t size)
{
    munmap(mem, size);
}
