#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd cppsmith
make clean && make

rm -rf /tmp/cppsmith && fffc executables/* /tmp/cppsmith

for f in /tmp/cppsmith/executables/*/*_runner.sh;
	do echo "$f" && setarch `uname -m` -R bash "$f" -H;
done
