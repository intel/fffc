#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd doctest
make clean && make

rm -rf /tmp/doctest && fffc executables/* /tmp/doctest

setarch `uname -m` -R /tmp/doctest/executables/test2.gcc/*_runner.sh
