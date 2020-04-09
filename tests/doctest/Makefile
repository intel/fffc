# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

CLANG_CC_FLAGS = -g -O0 -fsanitize=address -fprofile-arcs -lm -shared-libasan -fno-common
GCC_CC_FLAGS = -g -O0 -fsanitize=address -fprofile-arcs -lm -fno-common

SOURCES = $(wildcard *.c)

GCC_EXECUTABLES = $(addprefix executables/,$(patsubst %.c,%.gcc,$(SOURCES)))
CLANG_EXECUTABLES = $(addprefix executables/,$(patsubst %.c,%.clang,$(SOURCES)))

.PHONY: directories

all: directories $(GCC_EXECUTABLES) $(CLANG_EXECUTABLES)

directories: executables/

executables/:
	-mkdir executables/

executables/%.clang: %.c
	clang $(CLANG_CC_FLAGS) $< -o $@

executables/%.gcc: %.c
	gcc $(GCC_CC_FLAGS) $< -o $@

clean:
	rm -rf executables/ *.profraw *.gcda fffc_state* fffc_crash*
