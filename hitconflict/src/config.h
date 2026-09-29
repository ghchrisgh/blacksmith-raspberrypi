#pragma once

#include <stdint.h>
#include <stddef.h>

// config.h – parameters

// ---------------------------------------------------------------------------
// CPU: pin cpu_core
// ---------------------------------------------------------------------------
static constexpr int CPU_CORE = 0;

// ---------------------------------------------------------------------------
// 1. Tool hist_main:
// Loops: INNER, OUTER  (for hist_main)
// INNER: higher = more stable values
// OUTER: number of histogram measurement points
// ---------------------------------------------------------------------------
static constexpr int INNER = 1000;
static constexpr int OUTER = 1000;

// ---------------------------------------------------------------------------
// Filter: enable/disable, min and max for hist_main
// if there is a 3rd spike, it gets filtered
// ---------------------------------------------------------------------------
static constexpr bool     FILTER_ENABLED = false;
static constexpr uint64_t FILTER_MIN     = 300;
static constexpr uint64_t FILTER_MAX     = 450;

// ---------------------------------------------------------------------------
// 2. Tool efn_main:
// ---------------------------------------------------------------------------

// Set threshold between hit and conflict
static constexpr uint64_t HIT_CONFLICT_THRESHOLD = 430;

// Steps Cacheline 64B
static constexpr size_t EFN_STRIDE = 64;

// Dense region: in the dense region every cacheline (64B) is measured
// 2 MB = 2^21 -> covers address bits 0..20 (Col + low row bits)
static constexpr size_t EFN_SIZE = 2ULL * 1024 * 1024;   // 2 MB dense region
//2ULL * 1024 * 1024;

// ---------------------------------------------------------------------------
// Coarse probes: in addition to the dense 2 MB block, measure ONE SINGLE
// address every EFN_COARSE_STEP across the entire hugepage range
// (i.e. at 2MB, 4MB, 6MB, ... up to EFN_COARSE_SPAN).
//
// Purpose: the coarse probes vary the HIGH address bits (>= bit 21)
// ---------------------------------------------------------------------------
static constexpr bool   EFN_COARSE_ENABLED = true;
static constexpr size_t EFN_COARSE_STEP    = 2ULL * 1024 * 1024;          // every 2 MB
static constexpr size_t EFN_COARSE_SPAN    = 1ULL * 1024 * 1024 * 1024;   // up to 1 GB

// Median over N measurements per pair
static constexpr int EFN_SAMPLES = 4;

// calibration test for efn_main.cpp
// Tests random addresses before the start and builds a histogram
// Shows that hits and conflicts are correct
// true = calibration only. false = calibration and full run of the tool
static constexpr bool EFN_CALIBRATE_ONLY = false;
static constexpr int  EFN_CALIB_SAMPLES  = 4000;  // number of samples

// log to create a png for the 3 phases with cycles
static constexpr bool EFN_LOG_MEASUREMENTS = true;
