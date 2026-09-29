// jit_kernel.cpp
// build_kernel asmjit loop

#include "jit_kernel.h"

// ---------------------------------------------------------------------------
// asmjit guide asmjit.com
// idea measurement: rdtsc = start; for(0 bis 1000){*a, *b, flush a, flush b, lfence()} rdtsc = end; return end-start;
// asmjit part Blacksmith: https://github.com/comsec-group/blacksmith/blob/public/src/Fuzzer/CodeJitter.cpp
// ---------------------------------------------------------------------------

static JitRuntime rt;

Fn build_kernel()
{
    CodeHolder code;
    code.init(rt.environment());
    a64::Assembler as(&code);

    // Register:
    const a64::Gp x0 = a64::x0; // address a
    const a64::Gp x1 = a64::x1; // address b
    const a64::Gp x3 = a64::x3; // scratch for load ziel
    const a64::Gp x4 = a64::x4; // t0 pmccntr_el0: cycles start
    const a64::Gp x5 = a64::x5; // t1 pmccntr_el0: cycles end

    // 1. Warmup (same as in Blacksmith):
        as.ldr(x3, a64::ptr(x0)); // load address a
    as.ldr(x3, a64::ptr(x1)); // load address b

    // Skipped sync part in Blacksmith

    // 2. Measure:
    //as.embed_uint32(DSB_ISH);
    //as.embed_uint32(ISB_SY);
    as.embed_uint32(MRS_PMCCNTR_X4);   // t0: cycles start

    for (int i = 0; i < INNER; i++) {
        as.ldr(x3, a64::ptr(x0));      // load a
        as.ldr(x3, a64::ptr(x1));      // load b
        as.embed_uint32(DC_CIVAC_X0);  // a dc civac (clflush)
        as.embed_uint32(DC_CIVAC_X1);  // b dc civac (clflush)
        //as.embed_uint32(DSB_ISH);
        as.embed_uint32(ISB_SY);       // isb sy (lfence)
    }

    //as.embed_uint32(ISB_SY);
    as.embed_uint32(MRS_PMCCNTR_X5);   // t1: cycles end

    as.sub(x0, x5, x4);          // x0 = x5 - x4, return end - start
    as.mov(x1, (uint64_t)INNER); // writes value into register
    as.udiv(x0, x0, x1);         // x0 = x0 / x1, return
    as.ret(a64::x30);            // return address

    Fn fn = nullptr;
    rt.add(&fn, &code); // Return
    return fn;
}

void release_kernel(Fn fn)
{
    rt.release(fn);
}
