#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd c_testsuite
make clean && make smoke_multicall

rm -rf /tmp/c_testsuite && fffc executables/* /tmp/c_testsuite

#export FFFC_MUTATION_COUNT=128
export FFFC_GENERATION_COUNT=5
setarch `uname -m` -R /tmp/c_testsuite/executables/00221.gcc/foo_runner.sh;
