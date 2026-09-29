#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <stdlib.h>



static inline void pin_to_cpu0(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
        exit(1);
    }
}

static inline uint64_t read_pmccntr_without_isb(void) {
    uint64_t val;
    asm volatile("mrs %0, pmccntr_el0" : "=r"(val) :: "memory");
    return val;
}
static inline uint64_t read_pmccntr_with_isb(void) {
    uint64_t val;
    asm volatile("isb\n mrs %0, pmccntr_el0\n isb" : "=r"(val) :: "memory");
    return val;
}

int main(void) {
    pin_to_cpu0();

    const int N = 1000000;
    uint64_t start, end, min_cycles = 0xFFFFFFFFFFFFFFFF;

    // Warm-up
    for(int i=0; i<1000; i++) read_pmccntr_with_isb();
    start = read_pmccntr_with_isb();
    for(int i=0; i<N; i++) {
        uint64_t t0 = read_pmccntr_with_isb();
        uint64_t t1 = read_pmccntr_with_isb();
        uint64_t delta = t1 - t0;
        if(delta < min_cycles) min_cycles = delta;
    }
    end = read_pmccntr_with_isb();
    printf("PMCCNTR_EL0: min cycles per read ~ %lu cycles\n", min_cycles);
    printf("Total elapsed cycles for %d reads: %lu\n", N, end-start);



    // Warum-up
    for(int i=0; i<1000; i++) read_pmccntr_without_isb();
    start = read_pmccntr_without_isb();
    for(int i=0; i<N; i++) {
        uint64_t t0 = read_pmccntr_without_isb();
        uint64_t t1 = read_pmccntr_without_isb();
        uint64_t delta = t1 - t0;
        if(delta < min_cycles) min_cycles = delta;
    }
    end = read_pmccntr_without_isb();
    printf("PMCCNTR_EL0 without ISB: min cycles per read ~ %lu cycles\n", min_cycles);
    printf("Total elapsed cycles for %d reads: %lu\n", N, end-start);
    

    return 0;
}
