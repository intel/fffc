#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd c_testsuite
make clean && make smoke_crash

rm -rf /tmp/c_testsuite && fffc executables/* /tmp/c_testsuite

#export FFFC_MUTATION_COUNT=128
export FFFC_GENERATION_COUNT=2
export FFFC_CRASH_PATH=/tmp
export FFFC_DATA_PATH=/tmp
setarch `uname -m` -R /tmp/c_testsuite/executables/00040.gcc/chk_runner.sh;

