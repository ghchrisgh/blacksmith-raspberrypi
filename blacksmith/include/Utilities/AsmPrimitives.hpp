/*
 * Copyright (c) 2021 by ETH Zurich.
 * Licensed under the MIT License, see LICENSE file for more details.
 */

#ifndef UTILS
#define UTILS

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_map>

#include "GlobalDefines.hpp"

/*====
 *Architecture check
====*/
#if defined(__aarch64__)
  #define arch_arm
#elif defined(__i386__) || defined(__x86_64__)
  #define arch_x86
#else
  #error "Unsupported architecture"
#endif

[[gnu::unused]] static inline __attribute__((always_inline)) void clflush(volatile void *p) {
#if defined(arch_arm)
  asm volatile("dc civac, %0" :: "r"(p) : "memory");
  asm volatile("dsb ish" ::: "memory");
  asm volatile("isb" ::: "memory");
#elif  defined(arch_x86)
  asm volatile("clflush (%0)\n" :: "r"(p) : "memory");
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) void clflushopt(volatile void *p) {
#if defined(arch_arm)
  asm volatile("dc civac, %0" :: "r"(p) : "memory");
  asm volatile("dsb ish" ::: "memory");
  asm volatile("isb" ::: "memory");
#else
#ifdef DDR3
  asm volatile("clflush (%0)\n" :: "r"(p) : "memory");
#else
  asm volatile("clflushopt (%0)\n" :: "r"(p) : "memory");
#endif
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) void cpuid() {
#if defined(arch_x86) 
  asm volatile("cpuid"::: "rax", "rbx", "rcx", "rdx");
#elif defined(arch_arm)
  // im notfall wieder weg
  asm volatile("isb" ::: "memory");
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) void mfence() {
#if defined(arch_arm) 
  asm volatile("dmb ish" ::: "memory");
#elif defined(arch_x86)
  asm volatile("mfence"::: "memory");
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) void sfence() {
#if defined(arch_arm)
  asm volatile("dmb ishst" ::: "memory");
#elif defined(arch_x86)
  asm volatile("sfence"::: "memory");
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) void lfence() {
#if defined(arch_arm)  
  asm volatile("isb" ::: "memory");
#elif defined(arch_x86)
  asm volatile("lfence"::: "memory");
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) uint64_t rdtscp() {
#if defined(arch_arm)
  uint64_t val;
  asm volatile("isb");
  asm volatile("mrs %0, pmccntr_el0" : "=r"(val));
  asm volatile("isb");
  return val;
#elif defined(arch_x86)
  uint64_t lo, hi;
  asm volatile("rdtscp\n" : "=a"(lo), "=d"(hi)::"%rcx");
  return (hi << 32UL) | lo;
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) uint64_t rdtsc() {
#if defined(arch_arm) 
  return rdtscp();
#elif defined(arch_x86)
  uint64_t lo, hi;
  asm volatile("rdtsc\n" : "=a"(lo), "=d"(hi)::"%rcx");
  return (hi << 32UL) | lo;
#endif
}

[[gnu::unused]] static inline __attribute__((always_inline)) uint64_t realtime_now() {
  struct timespec now_ts{};
  clock_gettime(CLOCK_MONOTONIC, &now_ts);
  return static_cast<uint64_t>(
      (static_cast<double>(now_ts.tv_sec)*1e9 + static_cast<double>(now_ts.tv_nsec)));
}

#endif /* UTILS */
