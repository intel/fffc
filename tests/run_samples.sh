#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd sample_programs
make clean && make

rm -rf /tmp/samples && fffc executables/* /tmp/samples

for f in /tmp/samples/executables/*/*_runner.sh;
	do echo "$f" && setarch `uname -m` -R bash "$f" -H;
done
