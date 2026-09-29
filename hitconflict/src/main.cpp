// Manual instructions für asmjit teil:
// https://developer.arm.com/documentation/ddi0487/latest
// asmjit: asmjit.com
// blacksmith asmjit part: https://github.com/comsec-group/blacksmith/blob/public/src/Fuzzer/CodeJitter.cpp

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "helper.h"
#include "config.h"
#include "jit_kernel.h"

#ifndef __aarch64__
#error "Only supports aarch64"
#endif

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    // checks root access to pagemap
    if (geteuid() != 0) {
        fprintf(stderr, "[ ERR ] - root required: /proc/self/pagemap\n");
        return 1;
    }
    printf("[ LOG ] - Root access OK (/proc/self/pagemap)\n");

    // cpu pin, log
    pin_cpu(CPU_CORE); // wie taskset (ansonsten taskset -c 0 bei make run)
    printf("[ LOG ] - Pinned to CPU core %d\n", CPU_CORE);

    // LOG values INNER, OUTER
    printf("[ LOG ] - INNER=%d,  OUTER=%d\n", INNER, OUTER);

    // LOG filter enabled or disabled
    if (FILTER_ENABLED)
        printf("[ LOG ] - Filter enabled (min=%lu, max=%lu cycles)\n", FILTER_MIN, FILTER_MAX);
    else
        printf("[ LOG ] - Filter disabled\n");


    const size_t SIZE = 512ULL * 2ULL * 1024 * 1024;

    // mmap
        uint8_t* mem = (uint8_t*)mmap(
        NULL, SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB,
        -1, 0);

        if (mem == MAP_FAILED) {
            perror("[ ERR ] - mmap(MAP_HUGE_1GB). Try 'make setup' before running");
            return 1;
        } else {
        printf("[ LOG ] - mmap 1GB hugepage OK\n");
        //printf("[ LOG ] - mmap 1GB hugepage OK (virt_base=%p)\n", (void*)mem);
        }
    //if (mem == MAP_FAILED) { perror("[ ERROR ] - mmap(MAP_HUGE_1GB). Try 'make setup' before running"); return 1; }

    // zwingt den Kernel physische Pages zu allozieren
    // Hugepages lazy: ohne Zugriff gibts keine physische Seite
    for (size_t i = 0; i < SIZE; i += 4096)
        mem[i] = 1;
        
    /*
    uint64_t phys_base = get_phys_base(mem);
    printf("Hugepage virt_base: %p\n", (void*)mem);
    printf("Hugepage phys_base: 0x%lx\n", phys_base);
    */

    Fn fn = build_kernel();

    FILE* csv = fopen("accesses.csv", "w");
    if (!csv) { perror("fopen"); return 1; }
    // fprintf(csv, "virt_a,virt_b,phys_a,phys_b,cycles\n");
    fprintf(csv, "cycles\n");

    srand(42);

    for (int i = 0; i < OUTER; i++)
    {
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();
        sched_yield();

        // random aligned cache-line pair
        size_t off_a = ((size_t)rand() % (SIZE / 64)) * 64;
        size_t off_b = ((size_t)rand() % (SIZE / 64)) * 64;
        if (off_a == off_b) { i--; continue; }

        volatile uint8_t* a = &mem[off_a];
        volatile uint8_t* b = &mem[off_b];
        
        // uint64_t phys_a = phys_base + off_a;
        // uint64_t phys_b = phys_base + off_b;

        uint64_t cycles = fn(a, b);

        // Filter unplausible values and repeat measurement
        if (FILTER_ENABLED && (cycles < FILTER_MIN || cycles > FILTER_MAX)) { i--; continue; }

        // fprintf(csv, "%p,%p,0x%lx,0x%lx,%lu\n",
        //        (void*)a, (void*)b, phys_a, phys_b, cycles);
        fprintf(csv, "%lu\n", cycles); // erstmal nur cycles
    }

    fclose(csv);
    printf("Done. %d measurements in accesses.csv\n", OUTER);

    release_kernel(fn);
    munmap(mem, SIZE);
    return 0;
}
