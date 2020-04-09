#! /bin/sh

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

cd tinn
make

fffc test `mktemp -d`
