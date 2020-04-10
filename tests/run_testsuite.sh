#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd c_testsuite
make clean && make -j8

rm -rf /tmp/c_testsuite && fffc executables/* /tmp/c_testsuite

for f in /tmp/c_testsuite/executables/*/*_runner.sh;
	do echo "$f" && setarch `uname -m` -R bash "$f" -H;
done
