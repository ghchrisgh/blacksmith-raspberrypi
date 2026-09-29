#pragma once

#include <stdint.h>
#include <sched.h>

static inline uint64_t read_pmccntr()
{
    uint64_t v;
    asm volatile("mrs %0, pmccntr_el0" : "=r"(v));
    return v;
}

static inline void fence()
{
    asm volatile("dsb sy" ::: "memory");
}

// pin cpu
void pin_cpu(int core);
