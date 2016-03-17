# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

# This script sets the environment needed by the other scripts.
# The lines in this script needs to be run by a parent
# to the other script. It is therefore a good idea to run
# this script with the 'source' command (bash) before using any other script.

export PROJECTS_DIR="$HOME/daedal"
export SPEC_BENCH="$PROJECTS_DIR/spec2006/benchspec/CPU2006"
export BUILD_SCRIPTS="$PROJECTS_DIR/daedal-scripts/dae-buildscripts/build"
export LLVM_DIR="$PROJECTS_DIR/compiler/llvm"
export LLVM_BIN="$PROJECTS_DIR/compiler/build/llvm-build/bin"
export COMPILER_LIB="$PROJECTS_DIR/compiler/build/projects-build/lib"
