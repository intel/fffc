#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd csmith
make clean && make

rm -rf /tmp/csmith && fffc executables/* /tmp/csmith

for f in /tmp/csmith/executables/*/*_runner.sh;
	do echo "$f" && setarch `uname -m` -R bash "$f" -H;
done
