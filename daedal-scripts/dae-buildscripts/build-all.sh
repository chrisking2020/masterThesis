#!/bin/bash

# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

echoerr() { echo "$@" >&2; }

## Arguments ##
# $1: Buildlist file
# $2: Output location
# $3: Log directory
#
# The bildlist file is supposed to be formated as:
#
# BENCHMARK\n
# VERSION\n
# GRANULARITY \n
# INDIRECTIONS...\n
#
# This pattern is repeated for more benchmarks.
# VERSION is an identifier used to differentiate between diferent builds.
#
# At the output location (which must exist) the benchmark is compiled to
# the directory '[BENCHMARK]/[VERSION]' (taken from the buildlist)
# (which will be created or cleared before compilarion).
###############

## Environment ##
# This scripts expects following environment variables to be set:
# SPEC_BENCH
# BUILD_SCRIPTS
# LLVM_BIN
# COMPILER_LIB
#
# SPEC_BENCH and BUILD_SCRIPTS are optional under the right conditions.
#################

BUILDLIST="$1"
OUT_BASE="${2%/}"
LOG_DIR="${3%/}"

# establish locations
if ! [ -d "$SPEC_BENCH" ] ; then
    export SPEC_BENCH="$PWD"
fi
if ! [ -d "$BUILD_SCRIPTS" ] ; then
    export BUILD_SCRIPTS="$PWD"
fi
if ! [ -d "$LLVM_BIN" ] ; then
    echoerr "error: cannot find LLVM binaries"
    exit 1;
fi
if ! [ -d "$COMPILER_LIB" ] ; then
    echoerr "error: cannot find compiler library"
    exit 1;
fi

# check for dependencies
if ! [ -x "$BUILD_SCRIPTS/build.sh" ] ; then
    echoerr "error: cannot find runnable 'build.sh'"
    exit 1;
fi
if ! [ -x "$BUILD_SCRIPTS/compileSPEC.sh" ] ; then
    echoerr "error: cannot find runnable 'compileSPEC.sh'"
    exit 1;
fi
if ! [ -x "$BUILD_SCRIPTS/compile-bench.sh" ] ; then
    echoerr "error: cannot find runnable 'compile-bench.sh'"
    exit 1;
fi

# check for correct arguments
if [ $# -ne 3 ]; then
    echoerr "error: wrong number of arguments"
    exit 1
fi

# check files and directories
if [ -e "$BUILDLIST" ]; then
    BUILDLIST=$(readlink -m "$BUILDLIST")
else
    echoerr "error: cannot find the buildlist file"
    exit 1
fi
if [ -d "$LOG_DIR" ]; then
    LOG_DIR=$(readlink -m "$LOG_DIR")
else
    echoerr "error: cannot find the log directory"
    exit 1
fi
if [ -d "$OUT_BASE" ]; then
    OUT_BASE=$(readlink -m "$OUT_BASE")
else
    echoerr "error: output directory $OUT_BASE is not a directory"
    exit 1
fi

while read bench; do
    read vers
    read gran
    read indirs
    "$BUILD_SCRIPTS"/build.sh "$bench" "$OUT_BASE" "$vers" "$gran" "$indirs" \
        2>&1 | tee "$LOG_DIR/build.$bench.$vers.log"
done < "$BUILDLIST"
