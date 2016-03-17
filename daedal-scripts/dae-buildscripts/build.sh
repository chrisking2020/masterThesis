#!/bin/bash

# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

echoerr() { echo "$@" >&2; }

## Arguments ##
# $1: Benchmark name
# $2: Output location
# $3: Version name
# $4: Granularity
# $5: Indirections (e.g. "0 1 2 3 5 6", include citation marks)
#
# At the output location (which must exist) the benchmark is compiled to
# the directory '[Benchmark name]/[Version name]' (which will be
# created or cleared before compilarion).
###############

## Environment ##
# This scripts expects following environment variables to be set:
# SPEC_BENCH
# BUILD_SCRIPTS
# COMPILER_LIB
# LLVM_BIN
#
# SPEC_BENCH and BUILD_SCRIPTS are optional under the right conditions.
#################

BENCH="${1%/}"
OUT_BASE="${2%/}"
VERS="${3%/}"
GRAN="$4"
INDIRS="$5"

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
if ! [ -x "$BUILD_SCRIPTS/compileSPEC.sh" ] ; then
    echoerr "error: cannot find runnable 'compileSPEC.sh'"
    exit 1;
fi
if ! [ -x "$BUILD_SCRIPTS/compile-bench.sh" ] ; then
    echoerr "error: cannot find runnable 'compile-bench.sh'"
    exit 1;
fi

# check for correct arguments
if [ $# -ne 5 ]; then
    echoerr "error: wrong number of arguments"
    exit 1
fi

# check granularity and indirections
if ! [[ "$GRAN" =~ ^[0-9]+$ ]] ; then
    echoerr "error: the granularity is not a number"
    exit 1
fi
for i in $INDIRS ; do
    if ! [[ "$i" =~ ^[0-9]+$ ]] ; then
        echoerr "error: some indirection is not a number"
        exit 1
    fi
done

# check directories
BENCH_DIR="$SPEC_BENCH/$BENCH"
BENCH_SRC="$BENCH_DIR/src"
echo $BENCH_DIR
echo $BENCH_SRC
if [ -z "$BENCH" ]; then
    echoerr "error: please supply benchmark name"
    exit 1
fi
if ! [ -d "$BENCH_DIR" ]; then
    echoerr "error: cannot find benchmark $BENCH"
    exit 1
fi
if ! [ -d "$BENCH_SRC" ]; then
    echoerr "error: cannot find source for $BENCH"
    exit 1
fi

if [ -z "$VERS" ]; then
    echoerr "error: please supply version name"
    exit 1
fi
if [ -d "$OUT_BASE" ]; then
    OUT_BASE=$(readlink -m "$OUT_BASE")
else
    echoerr "error: output directory $OUT_BASE is not a directory"
    exit 1
fi
OUT_DIR="$OUT_BASE/$BENCH/$VERS"
mkdir -p "$OUT_DIR"
rm -rf "$OUT_DIR"/*
if ! [ -d "$OUT_DIR" ]; then
    echoerr "error: $OUT_DIR could not be created"
    exit 1
fi

# get compile command
export CLANG=`"$BUILD_SCRIPTS"/compile-bench.sh $BENCH`
if [ -z "$CLANG" ]; then
    echoerr "error: failed to get compilation command"
    exit 1
fi

# set base binary names
REFNAME="$BENCH.llvm"
CAENAME="CAE_$VERS.$BENCH"
DAENAME="DAE_$VERS.$BENCH"

# start processing
cd "$BENCH_SRC"
echo "Processing $BENCH"
echo `pwd`

# generate reference version
echo "$LLVM_BIN"/$CLANG -O3 *.c* -lm -o "$OUT_DIR/$REFNAME"
"$LLVM_BIN"/$CLANG -O3 *.c* -lm -o "$OUT_DIR/$REFNAME"
if [ $? -ne 0 ]; then
    echoerr "error: failed to build reference version"
fi

# build individual files with DAE
for i in `ls 2>/dev/null *.{c,cc,cpp}`
do
    # CORRECT THE ARGUMENTS
    "$BUILD_SCRIPTS"/compileSPEC.sh "$i" "$OUT_DIR" "$BENCH" "$INDIRS"
done

# generate DAE and CAE binaries
cd "$OUT_DIR"
echo "target datalayout = \"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128\"
target triple = \"x86_64-unknown-linux-gnu\"" > header.ll

shopt -s nullglob

echo "Gererating DAE"

cat header.ll Globals.ll \
| sed 's/\(_granularity\([^0-9][^ ]* \)*\)[0-9]\+[ ]*$/\1'"$GRAN"'/' \
> GV_DAE.ll

for i in $INDIRS
do
    "$LLVM_BIN"/$CLANG *.opt$i.ll *.out.ll GV_DAE.ll -o "$DAENAME.$i" \
        -O3 -static -lm \
        -L /usr/local/lib/ "$COMPILER_LIB/libDAE_prof_ST.a" \
        -lpapi -lcpufreq -lpthread -lrt
done

# cat header.ll Globals.ll \
# | sed 's/\(_granularity\([^0-9][^ ]* \)*\)[0-9]\+[ ]*$/\1'"1000000000"'/' \
# > GV_CAE.ll

echo "Generating CAE"

cp GV_DAE.ll GV_CAE.ll

"$LLVM_BIN"/$CLANG *.orig.ll *.out.ll GV_CAE.ll -o "$CAENAME" \
    -O3 -static -lm \
    -L /usr/local/lib/ "$COMPILER_LIB/libDAE_prof_ST.a" \
    -lpapi -lcpufreq -lpthread -lrt

rm header.ll
rm Globals.ll
