/// \file profiler.h
///
/// \brief Interfacing DVFS support for DAE
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.
#include <cpufreq.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __PROFILER_H__
#define __PROFILER_H__

#define CACHE_LINE 64

#define MODE_SINGLE_THREADED 1
#define MODE_OMP 2
#define MODE_PTHREAD 3

#ifndef PROFILING_MODE
#define PROFILING_MODE MODE_SINGLE_THREADED
#endif

#if defined(__i386__)
static __inline__ uint64_t rdtsc(void) {
  uint64_t val;
  __asm__ __volatile__("rdtsc " : "=A"(val));
  return (val);
}
#elif defined(__x86_64__)
static __inline__ uint64_t rdtsc(void) {
  uint64_t hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)lo) | (((uint64_t)hi) << 32);
}
#elif defined(__arm__)
static __inline__ uint64_t rdtsc(void) {
  uint32_t r;
  asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(r));
  return (uint64_t)r;
}
#else
#error "*** Incompatible Architecture ***"
#endif

extern "C" {

extern volatile void *profiler_get_counters(uint64_t tid);
extern uint64_t profiler_get_thread_id(void);

extern void profiler_start_access(volatile void *arg);
extern void profiler_end_access(volatile void *arg);
extern void profiler_start_execute(volatile void *arg);
extern void profiler_end_execute(volatile void *arg);

extern void profiler_print_stats(void);
}

#endif /* __PROFILER_H__ */
