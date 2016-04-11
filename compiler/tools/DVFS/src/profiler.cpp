/// \file profiler.cpp
///
/// \brief Interfacing DVFS support for DAE
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
#include <cpufreq.h>

#include "profiler.h"
#include <cpufreq.h>

struct Statistics {
  uint64_t access_phase_time;
  uint64_t execute_phase_time;
  uint64_t access_phases;
  uint64_t execute_phases;

  uint64_t access_t_start;
  uint64_t execute_t_start;
  uint64_t padding[2];
} __attribute__((aligned(CACHE_LINE)));

unsigned long minCPU_Freq;
unsigned long maxCPU_Freq;
unsigned long curCPU_Freq;

#if (PROFILING_MODE == MODE_SINGLE_THREADED)
uint64_t profiler_get_thread_id(void) { return (0); }
#elif (PROFILING_MODE == MODE_OMP)
#include <omp.h>
#define EXECUTION_STR "OpenMP"
uint64_t profiler_get_thread_id(void) { return (uint64_t)omp_get_thread_num(); }
#elif (PROFILING_MODE == MODE_PTHREAD)
#include <pthread.h>
#define EXECUTION_STR "Pthread"
uint64_t profiler_get_thread_id(void) { return (uint64_t)pthread_self(); }
#else
#error "*** ERROR : Unsupported Profiling Mode. ***"
#endif

void profiler_start_access(volatile void *arg) {
  volatile struct Statistics *s = (volatile struct Statistics *)arg;
  s->access_t_start = rdtsc();
}

void profiler_end_access(volatile void *arg) {
  volatile struct Statistics *s = (volatile struct Statistics *)arg;
  s->access_phase_time += (rdtsc() - s->access_t_start);
  ++s->access_phases;
}

void profiler_start_execute(volatile void *arg) {
  volatile struct Statistics *s = (volatile struct Statistics *)arg;
  s->execute_t_start = rdtsc();
}

void profiler_end_execute(volatile void *arg) {
  volatile struct Statistics *s = (volatile struct Statistics *)arg;
  s->execute_phase_time += (rdtsc() - s->execute_t_start);
  ++s->execute_phases;
}

#if (PROFILING_MODE == MODE_SINGLE_THREADED)
volatile struct Statistics stat __attribute__((aligned(CACHE_LINE)));

volatile void *profiler_get_counters(uint64_t tid) { return &stat; }

void profiler_stats_normalize(void) {
  cpufreq_get_hardware_limits(0, &minCPU_Freq, &maxCPU_Freq);
  curCPU_Freq = cpufreq_get_freq_kernel(0);

  double ratioFL = (double)curCPU_Freq / (double)maxCPU_Freq;
  double ratioFH = (double)curCPU_Freq / (double)maxCPU_Freq;

  stat.access_phase_time =
      (uint64_t)((long double)stat.access_phase_time * ratioFL);
  stat.execute_phase_time =
      (uint64_t)((long double)stat.execute_phase_time * ratioFH);
}

void profiler_print_stats(void) {
  profiler_stats_normalize();

  double wallTimePrefetch =
      (double)stat.access_phase_time / (double)curCPU_Freq / 1000.0;
  double wallTimeTask =
      (double)stat.execute_phase_time / (double)curCPU_Freq / 1000.0;

  double total_time = wallTimeTask + wallTimePrefetch;

  double p_task = 100.0 * (wallTimeTask / total_time);
  double p_pfetch = 100.0 * (wallTimePrefetch / total_time);

  printf("        CPU Frequency (Min)     (kHz): %lu\n", minCPU_Freq);
  printf("        CPU Frequency (Max)     (kHz): %lu\n", maxCPU_Freq);
  printf("        CPU Frequency (Current) (kHz): %lu\n\n", curCPU_Freq);

  printf("        Total time        (s): %.9lf \n", total_time);
  printf("        Compute time      (s): %.9lf \n", wallTimeTask);
  printf("        PreFetch time     (s): %.9lf \n\n", wallTimePrefetch);

  printf("        %% Compute Time   : %.2lf\n", p_task);
  printf("        %% PreFetch Time  : %.2lf\n\n", p_pfetch);

  printf("        Total Tasks       : %lu \n",
         stat.execute_phases + stat.access_phases);
  printf("        Compute tasks     : %lu \n", stat.execute_phases);
  printf("        PreFetch tasks    : %lu \n\n", stat.access_phases);

  printf("        Compute Ticks / Task  : %lu\n",
         stat.execute_phase_time / stat.execute_phases);

  if (stat.access_phases) {
    printf("        PreFetch Ticks / Task : %lu\n",
           stat.access_phase_time / stat.access_phases);
  } else {
    printf("        PreFetch Ticks / Task : %lu\n", 0l);
  }
}
#elif ((PROFILING_MODE == MODE_OMP) || (PROFILING_MODE == MODE_PTHREAD))
#include <map>

volatile unsigned _p_lock = 0;

std::map<uint64_t, volatile struct Statistics *> stat;

static __inline__ void profiler_lock(volatile unsigned *lock) {
  while (__sync_lock_test_and_set(lock, 1))
    ;
}

static __inline__ void profiler_unlock(volatile unsigned *lock) {
  __sync_lock_release(lock);
}

volatile void *profiler_get_counters(uint64_t tid) {
  std::map<uint64_t, volatile struct Statistics *>::iterator s_it;

  s_it = stat.find(tid);
  if (s_it != stat.end()) {
    return s_it->second;
  } else {
    volatile struct Statistics *ts = new struct Statistics();
    profiler_lock(&_p_lock);
    stat[tid] = ts;
    profiler_unlock(&_p_lock);
    return ts;
  }
}

void profiler_stats_normalize(void) {
  cpufreq_get_hardware_limits(0, &minCPU_Freq, &maxCPU_Freq);
  curCPU_Freq = cpufreq_get_freq_kernel(0);

  double ratioFL = (double)curCPU_Freq / (double)maxCPU_Freq;
  double ratioFH = (double)curCPU_Freq / (double)maxCPU_Freq;

  for (auto it = stat.cbegin(); it != stat.cend(); ++it) {
    (*it).second->access_phase_time =
        (uint64_t)((long double)(*it).second->access_phase_time * ratioFL);
    (*it).second->execute_phase_time =
        (uint64_t)((long double)(*it).second->execute_phase_time * ratioFH);
  }
}

void profiler_print_stats(void) {
  if (!stat.size()) {
    printf("No statistics gathered.\n");
    return;
  }

  unsigned num_threads = stat.size();

  profiler_stats_normalize();

  printf("        %s parallel execution with : %u threads\n", EXECUTION_STR,
         num_threads);

  printf("        CPU Frequency (Min)     (kHz): %lu\n", minCPU_Freq);
  printf("        CPU Frequency (Max)     (kHz): %lu\n", maxCPU_Freq);
  printf("        CPU Frequency (Current) (kHz): %lu\n\n", curCPU_Freq);

  uint64_t access_phase_time = 0;
  uint64_t execute_phase_time = 0;
  uint64_t access_phases = 0;
  uint64_t execute_phases = 0;

  for (auto it = stat.cbegin(); it != stat.cend(); ++it) {
    access_phase_time += (*it).second->access_phase_time;
    execute_phase_time += (*it).second->execute_phase_time;
    access_phases += (*it).second->access_phases;
    execute_phases += (*it).second->execute_phases;
  }

  double wallTimePrefetch = (double)access_phase_time / (double)curCPU_Freq /
                            1000.0 / (double)num_threads;
  double wallTimeTask = (double)execute_phase_time / (double)curCPU_Freq /
                        1000.0 / (double)num_threads;

  double total_time = wallTimeTask + wallTimePrefetch;

  double p_task = 100.0 * (wallTimeTask / total_time);
  double p_pfetch = 100.0 * (wallTimePrefetch / total_time);

  printf("        Total time        (s): %.9lf \n", total_time);
  printf("        Compute time      (s): %.9lf \n", wallTimeTask);
  printf("        PreFetch time     (s): %.9lf \n\n", wallTimePrefetch);

  printf("        %% Compute Time   : %.2lf\n", p_task);
  printf("        %% PreFetch Time  : %.2lf\n\n", p_pfetch);

  printf("        Total Tasks       : %lu \n", execute_phases + access_phases);
  printf("        Compute tasks     : %lu \n", execute_phases);
  printf("        PreFetch tasks    : %lu \n\n", access_phases);

  printf("        Compute Ticks / Task  : %lu\n",
         execute_phase_time / execute_phases);

  if (access_phases) {
    printf("        PreFetch Ticks / Task : %lu\n",
           access_phase_time / access_phases);
  } else {
    printf("        PreFetch Ticks / Task : %lu\n", 0l);
  }
}
#else
#error "*** ERROR : Unsupported Profiling Mode. ***"
#endif
