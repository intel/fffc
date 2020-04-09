#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd tiny-bignum-c
make

PYTHONHASHSEED=0 fffc build/* `mktemp -d`
