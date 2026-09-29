#pragma once

// Manual instructions for asmjit:
// https://developer.arm.com/documentation/ddi0487/latest
// asmjit: asmjit.com
// blacksmith asmjit part: https://github.com/comsec-group/blacksmith/blob/public/src/Fuzzer/CodeJitter.cpp

#include <stdint.h>
#include <asmjit/a64.h>
#include "config.h"

using namespace asmjit;

// ---------------------------------------------------------------------------
// see instruction manual arm: https://developer.arm.com/documentation/ddi0487/latest
// DC CIVAC, Xt = SYS #3, C7, C14, #1, Xt  -> 0xD50B7E00 | Rt
// MRS Xt, PMCCNTR_EL0                      -> 0xD53B9D00 | Rt
// ---------------------------------------------------------------------------
static constexpr uint32_t DC_CIVAC_X0    = 0xD50B7E20u; // dc civac, x0
static constexpr uint32_t DC_CIVAC_X1    = 0xD50B7E21u; // dc civac, x1
static constexpr uint32_t DSB_ISH        = 0xD5033B9Fu; // dsb ish
static constexpr uint32_t ISB_SY         = 0xD5033FDFu; // isb sy
static constexpr uint32_t MRS_PMCCNTR_X4 = 0xD53B9D04u; // mrs x4, pmccntr_el0
static constexpr uint32_t MRS_PMCCNTR_X5 = 0xD53B9D05u; // mrs x5, pmccntr_el0

// ---------------------------------------------------------------------------
// idea: rdtsc = start; for(0 bis INNER){*a, *b, flush a, flush b, lfence()} rdtsc = end; return end-start;
// asmjit part aus blacksmith: https://github.com/comsec-group/blacksmith/blob/public/src/Fuzzer/CodeJitter.cpp
// ---------------------------------------------------------------------------
typedef uint64_t (*Fn)(volatile uint8_t* a, volatile uint8_t* b);

Fn build_kernel();
void release_kernel(Fn fn);

/*
Note:

Output Register:

printf "dc civac, x0\ndc civac, x1\ndsb ish\nisb\nmrs x4, pmccntr_el0\nmrs x5, pmccntr_el0\n" \
  > /tmp/t.s && aarch64-linux-gnu-as /tmp/t.s -o /tmp/t.o && \
  aarch64-linux-gnu-objdump -d /tmp/t.o
*/
