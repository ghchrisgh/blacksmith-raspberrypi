// efn_main.cpp: Bank, Row, and Column
//
// Output Format: dram_map.json = { "bank N": { "row M": [phys, etc...] } }

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <vector>
#include <algorithm>

#include "helper.h"
#include "config.h"
#include "jit_kernel.h"
#include "hugepage.h"
#include "pagemap.h"

#ifndef __aarch64__
#error "Only supports aarch64"
#endif

static FILE* g_meas = nullptr;   // measurement log (when EFN_LOG_MEASUREMENTS true in config.h)

// Median over EFN_SAMPLES measurements. Loggs (Phase, off_a, off_b, median).. set in config.h
static uint64_t measure(Fn fn, uint8_t* mem, size_t off_a, size_t off_b, int phase)
{
    uint64_t s[EFN_SAMPLES];
    for (int i = 0; i < EFN_SAMPLES; i++) s[i] = fn(&mem[off_a], &mem[off_b]);
    std::sort(s, s + EFN_SAMPLES);
    uint64_t m = s[EFN_SAMPLES / 2];
    if (g_meas) fprintf(g_meas, "%d,%zu,%zu,%lu\n", phase, off_a, off_b, m);
    return m;
}

static inline bool is_conflict(uint64_t cycles)
{
    return cycles >= HIT_CONFLICT_THRESHOLD;
}

// Calibration with random addresses (set in config). Samples in the set dense (2mb) region.
static void calibrate(Fn fn, uint8_t* mem, size_t dense_n)
{
    fprintf(stderr, "[ CALIB ] measuring %d random pairs ...\n", EFN_CALIB_SAMPLES);
    srand(1234);
    std::vector<uint64_t> vals; vals.reserve(EFN_CALIB_SAMPLES);
    uint64_t mn = UINT64_MAX, mx = 0;
    FILE* cf = fopen("efn_calib.csv", "w");
    if (cf) fprintf(cf, "cycles\n");
    for (int i = 0; i < EFN_CALIB_SAMPLES; i++) {
        size_t a = (size_t)(rand() % dense_n), b = (size_t)(rand() % dense_n);
        if (a == b) continue;
        sched_yield();
        uint64_t c = fn(&mem[a * EFN_STRIDE], &mem[b * EFN_STRIDE]);
        vals.push_back(c);
        if (c < mn) mn = c; if (c > mx) mx = c;
        if (cf) fprintf(cf, "%lu\n", c);
    }
    if (cf) fclose(cf);
    std::sort(vals.begin(), vals.end());
    fprintf(stderr, "[ CALIB ] min=%lu median=%lu p90=%lu max=%lu\n",
            mn, vals[vals.size()/2], vals[(vals.size()*9)/10], mx);
    const int B = 40; uint64_t bw = (mx > mn) ? (mx - mn)/B : 1; if (!bw) bw = 1;
    std::vector<int> h(B, 0);
    for (uint64_t v : vals) { int b=(int)((v-mn)/bw); if(b>=B)b=B-1; h[b]++; }
    int mc = 1; for (int c : h) if (c > mc) mc = c;
    fprintf(stderr, "[ CALIB ] cycles distribution:\n");
    for (int b = 0; b < B; b++) { if(!h[b])continue;
        fprintf(stderr, "  %5lu | ", mn + (uint64_t)b*bw);
        for (int k=0;k<h[b]*50/mc;k++) fputc('#',stderr);
        fprintf(stderr, " %d\n", h[b]); }
    fprintf(stderr, "[ CALIB ] Threshold = %lu\n", HIT_CONFLICT_THRESHOLD);
}

int main()
{
    // Start: same part as hist_main.cpp
    if (geteuid() != 0) { fprintf(stderr, "root required\n"); return 1; }
    pin_cpu(CPU_CORE);

    const size_t HUGE = 1ULL * 1024 * 1024 * 1024;

    uint8_t* mem = alloc_hugepage(HUGE);
    if (!mem) return 1;

    uint64_t phys_base = get_phys_base(mem);

    Fn fn = build_kernel();

    // measurement offset: dense block + optional coarse probes after dense block ----
    std::vector<size_t> offs;
    for (size_t o = 0; o < EFN_SIZE; o += EFN_STRIDE) offs.push_back(o);
    const size_t dense_n = offs.size();
    if (EFN_COARSE_ENABLED) {
        // first multiple of STEP that is >= EFN_SIZE
        size_t start = ((EFN_SIZE + EFN_COARSE_STEP - 1) / EFN_COARSE_STEP) * EFN_COARSE_STEP;
        for (size_t o = start; o < EFN_COARSE_SPAN && o < HUGE; o += EFN_COARSE_STEP)
            offs.push_back(o);
    }
    const size_t N = offs.size();
    const size_t coarse_n = N - dense_n;

    fprintf(stderr, "[ LOG ] dense=%zu offsets (%zu MB @ %zu B), "
            "coarse=%zu offsets (all %zu MB up to %zu MB), total=%zu, threshold=%lu\n",
            dense_n, EFN_SIZE/(1024*1024), EFN_STRIDE,
            coarse_n, EFN_COARSE_STEP/(1024*1024), EFN_COARSE_SPAN/(1024*1024),
            N, HIT_CONFLICT_THRESHOLD);

    calibrate(fn, mem, dense_n);
    if (EFN_CALIBRATE_ONLY) {
        fprintf(stderr, "[ LOG ] CALIBRATE_ONLY=true -> End.\n");
        release_kernel(fn); free_hugepage(mem, HUGE); return 0;
    }

    if (EFN_LOG_MEASUREMENTS) {
        g_meas = fopen("efn_meas.csv", "w");
        if (g_meas) fprintf(g_meas, "phase,off_a,off_b,cycles\n");
        fprintf(stderr, "[ LOG ] Measurement logging on -> efn_meas.csv\n");
    }

    std::vector<bool> grouped(N, false);   // now index-based over offs[]
    FILE* js = fopen("dram_map.json", "w");
    if (!js) { perror("fopen dram_map.json"); return 1; }
    fprintf(js, "{\n");

    int bank_id = 0; bool first_bank = true; size_t cursor = 0;

    while (true)
    {
        while (cursor < N && grouped[cursor]) cursor++;
        if (cursor >= N) break;
        size_t A_idx = cursor, A = offs[A_idx];
        fprintf(stderr, "[ LOG ] Bank %d: Reference-offset 0x%lx%s\n",
                bank_id, A, A_idx >= dense_n ? " (coarse)" : "");

        // Step 1: A against all ungrouped -> conflicts = same bank, different row
        std::vector<size_t> conf_idx, hit_idx;
        for (size_t i = 0; i < N; i++) {
            if (grouped[i] || i == A_idx) continue;
            sched_yield();
            uint64_t c = measure(fn, mem, A, offs[i], 1);
            if (is_conflict(c)) conf_idx.push_back(i);
            else                hit_idx.push_back(i);
        }
        std::vector<size_t> member_idx;       // Indices in offs[]
        member_idx.push_back(A_idx);
        for (size_t i : conf_idx) member_idx.push_back(i);

        // Step 2: first conflict B against the hits -> finds A's own row
        if (!conf_idx.empty()) {
            size_t Boff = offs[conf_idx[0]];
            for (size_t i : hit_idx) {
                sched_yield();
                uint64_t c = measure(fn, mem, Boff, offs[i], 2);
                if (is_conflict(c)) member_idx.push_back(i);
            }
        }
        for (size_t i : member_idx) grouped[i] = true;

        // Step 3: group bank_members into rows (hit = same row)
        if (!first_bank) fprintf(js, ",\n");
        first_bank = false;
        fprintf(js, "  \"bank %d\": {\n", bank_id);
        std::vector<bool> used(member_idx.size(), false);
        int row_id = 0; bool first_row = true;
        for (size_t yi = 0; yi < member_idx.size(); yi++) {
            if (used[yi]) continue;
            size_t Y = offs[member_idx[yi]]; used[yi] = true;
            std::vector<size_t> same_row; same_row.push_back(Y);
            for (size_t xi = yi + 1; xi < member_idx.size(); xi++) {
                if (used[xi]) continue;
                size_t X = offs[member_idx[xi]];
                sched_yield();
                uint64_t c = measure(fn, mem, Y, X, 3);
                if (!is_conflict(c)) { same_row.push_back(X); used[xi] = true; }
            }
            if (!first_row) fprintf(js, ",\n");
            first_row = false;
            fprintf(js, "    \"row %d\": [", row_id);
            for (size_t k = 0; k < same_row.size(); k++) {
                if (k) fprintf(js, ", ");
                fprintf(js, "%lu", phys_base + same_row[k]);
            }
            fprintf(js, "]"); row_id++;
        }
        fprintf(js, "\n  }");
        fprintf(stderr, "[ LOG ] Bank %d done: %zu addresses, %d Rows\n",
                bank_id, member_idx.size(), row_id);
        bank_id++;
    }

    fprintf(js, "\n}\n"); fclose(js);
    if (g_meas) fclose(g_meas);
    fprintf(stderr, "[ LOG ] Done. %d Banks -> dram_map.json\n", bank_id);
    release_kernel(fn); free_hugepage(mem, HUGE);
    return 0;
}

