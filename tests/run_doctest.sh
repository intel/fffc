#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd doctest
make clean && make

fffc --overwrite executables/* /tmp/doctest

setarch `uname -m` -R /tmp/doctest/executables/test.gcc/*_runner.sh
setarch `uname -m` -R /tmp/doctest/executables/test.clang/*_runner.sh
