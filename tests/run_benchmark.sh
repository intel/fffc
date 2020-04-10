#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd c_testsuite
make clean && make smoke

rm -rf /tmp/c_testsuite && fffc executables/* /tmp/c_testsuite

#export FFFC_MUTATION_COUNT=128
export FFFC_GENERATION_COUNT=5
setarch `uname -m` -R bash perf record -F99 --call-graph dwarf -- /tmp/c_testsuite/executables/00077.gcc/foo_runner.sh;
perf script | ~/projects/FlameGraph/stackcollapse-perf.pl > out.perf-folded;
cat out.perf-folded | ~/projects/FlameGraph/flamegraph.pl > "perf.svg";

