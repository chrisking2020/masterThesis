#!/bin/bash

# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

## Warning ##
# This script does not contain any parameter failsafes and should therfore
# only be called by the scripts it is designed for.
#############

PARAMS="\
-DSPEC_CPU \
-DSPEC_CPU_LP64 \
-DSPEC_CPU_LINUX \
-DNDEBUG \
-DPERL_CORE \
-fno-strict-aliasing \
-static \
-I. \
-DFN \
-DFAST \
-DCONGRAD_TMP_VECTORS \
-DDSLASH_TMP_LINKS \
-DSPEC_CPU_LITTLE_ENDIAN \
"

## Arguments ##
# $1: Benchmark name
#
# Prints, based on the benchmark name, the command to compile
# that benchmark. To be clear, this includes the version of clang
# (clang or clang++) and required parameters. Other options such
# as optimization options, input and output files, etc. must be added
# afterwards.
###############

BENCH="$1"

if [[ "$BENCH" == "429.mcf" ]] ; then
    echo "clang $PARAMS"
elif [[ "$BENCH" == "433.milc" ]] ; then
    echo "clang $PARAMS"
elif [[ "$BENCH" == "450.soplex" ]] ; then
    echo "clang++ -I/usr/include/x86_64-linux-gnu/c++/4.7 $PARAMS"
elif [[ "$BENCH" == "456.hmmer" ]] ; then
    echo "clang $PARAMS"
elif [[ "$BENCH" == "462.libquantum" ]] ; then
    echo "clang $PARAMS"
elif [[ "$BENCH" == "470.lbm" ]] ; then
    echo "clang $PARAMS"
elif [[ "$BENCH" == "473.astar" ]] ; then
    echo "clang++ $PARAMS"
else
    echo "error: no command for $BENCH" >&2
    exit 1
fi
