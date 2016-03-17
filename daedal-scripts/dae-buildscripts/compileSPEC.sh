#!/bin/bash

# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

## Warning ##
# This script does not contain any parameter failsafes and should therfore
# only be called by the scripts it is designed for.
#############

COMMENT() { cat - ; }

## Arguments ##
# $1: Source file
# $2: Output directory
# $3: Benchmark name
# $4: Indirections (e.g. "0 1 2 3 5 6")
###############

## Environment ##
# This scripts expects following environment variables to be set:
# LLVM_BIN
# COMPILER_LIB
# CLANG
#################

SRCFILE="$1"
OUTDIR="$2"
BENCH="$3"
INDIR="$4"

echo "$SRCFILE"

# original file

"$LLVM_BIN"/$CLANG -S -emit-llvm "$SRCFILE" \
    -O3 -o "$OUTDIR/$SRCFILE.ll"
cd "$OUTDIR"

# chunk the loop and rename the kernel function
cat "$SRCFILE.ll" \
| "$LLVM_BIN"/opt -S -loop-simplify \
| "$LLVM_BIN"/opt -S -reg2mem \
| "$LLVM_BIN"/opt -S -licm \
| "$LLVM_BIN"/opt -S -load "$COMPILER_LIB/libLoopChunk.so" \
    -loop-chunk -bench-name "$BENCH" \
| "$LLVM_BIN"/opt -S -mem2reg \
| "$LLVM_BIN"/opt -S -load "$COMPILER_LIB/libLoopExtract.so" \
    -aggregate-extracted-args -second-loop-extract -is-dae -bench-name "$BENCH" \
> "$SRCFILE.loop.mem.ll"

# insert statistic print
cat "$SRCFILE.loop.mem.ll" \
| sed 's/tail \(call void @exit(\)/\1/g' \
| sed 's/call void @exit(/call void @profiler_print_stats()\n  &/g' \
| sed 's/declare void @exit(/declare void @profiler_print_stats()\n&/g' \
> "$SRCFILE.stats.ll"

grep 'main\|__kernel__' "$SRCFILE.stats.ll" &> /dev/null
res=$?
if [ $res -eq 0 ] ; then

    # create CAE
    cat "$SRCFILE.stats.ll" \
    | "$LLVM_BIN"/opt -S -load "$COMPILER_LIB/libTimeOrig.so" \
        -papi-orig \
    | "$LLVM_BIN"/opt -S -always-inline -O3 \
    > "$SRCFILE.orig.ll"

    # create DAE
    for i in $INDIR
    do
        cat "$SRCFILE.stats.ll" \
        | "$LLVM_BIN"/opt -S -load "$COMPILER_LIB/libFKernelPrefetch.so" \
            -tbaa -basicaa -f-kernel-prefetch \
            -indir-thresh $i -follow-partial \
        | tee "$SRCFILE.pref$i.ll" \
        | "$LLVM_BIN"/opt -S -always-inline -O3 \
        | COMMENT '"$LLVM_BIN"/opt -S -O3' \
        | COMMENT '"$LLVM_BIN"/opt -S -always-inline' \
        | "$LLVM_BIN"/opt -S -load "$COMPILER_LIB/libRemoveRedundantPref.so" \
            -rrp \
        | "$LLVM_BIN"/opt -S -O3 \
        > "$SRCFILE.opt$i.ll"
    done

elif [ $res -eq 1 ] ; then

    # no DAE
    cat "$SRCFILE.stats.ll" \
    | "$LLVM_BIN"/opt -S -always-inline -O3 \
    > "$SRCFILE.out.ll"

else
    echo "error: something went wrong with the DAE process" >&2
    exit 1
fi

#rm "$SRCFILE.stats.ll"
#rm "$SRCFILE.loop.mem.ll"
