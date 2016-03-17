# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

add_library(DAE_prof_ST STATIC profiler.cpp)
add_library(DAE_prof_OMP STATIC profiler.cpp)
add_library(DAE_prof_PT STATIC profiler.cpp)

target_compile_options(DAE_prof_ST PRIVATE -std=c++11 -D PROFILING_MODE=1 -finline-functions -fPIC)
target_link_libraries(DAE_prof_ST PRIVATE -lcpufreq)

target_compile_options(DAE_prof_OMP PRIVATE -std=c++11 -D PROFILING_MODE=2 -finline-functions -fopenmp -fPIC)
target_link_libraries(DAE_prof_OMP PRIVATE -lcpufreq)

target_compile_options(DAE_prof_PT PRIVATE -std=c++11 -D PROFILING_MODE=3 -finline-functions -fPIC)
target_link_libraries(DAE_prof_PT PRIVATE -lcpufreq -lpthread)