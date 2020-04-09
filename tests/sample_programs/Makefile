# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

CLANG_CC_FLAGS = -g -O0 -fsanitize=address -fPIC -fPIE -shared-libasan -fprofile-arcs
GCC_CC_FLAGS = -g -O0 -fsanitize=address -fPIC -fPIE -fprofile-arcs

SOURCES = $(wildcard *.c)

GCC_OBJECTS = $(addprefix objects/,$(patsubst %.c,%.gcc.o,$(SOURCES)))
CLANG_OBJECTS = $(addprefix objects/,$(patsubst %.c,%.clang.o,$(SOURCES)))

GCC_DWARVES = $(addprefix dwarves/,$(patsubst %.c,%.gcc.dwarf,$(SOURCES)))
CLANG_DWARVES = $(addprefix dwarves/,$(patsubst %.c,%.clang.dwarf,$(SOURCES)))

GCC_EXECUTABLES = $(addprefix executables/,$(patsubst %.c,%.gcc,$(SOURCES)))
CLANG_EXECUTABLES = $(addprefix executables/,$(patsubst %.c,%.clang,$(SOURCES)))

CLANG_ASTS = $(addprefix asts/,$(patsubst %.c,%.clang.ast,$(SOURCES)))

.PHONY: directories

all: directories $(GCC_DWARVES) $(CLANG_DWARVES) $(GCC_OBJECTS) $(CLANG_OBJECTS) $(GCC_EXECUTABLES) $(CLANG_EXECUTABLES) $(CLANG_ASTS)

directories: dwarves/ objects/ executables/ asts/

dwarves/:
	-mkdir dwarves/

objects/:
	-mkdir objects/

executables/:
	-mkdir executables/

asts/:
	-mkdir asts/

dwarves/%.dwarf: objects/%.o
	dwarfdump $< > $@

objects/%.gcc.o: %.c
	gcc -c $(GCC_CC_FLAGS) $< -o $@

objects/%.clang.o: %.c
	clang -c $(CLANG_CC_FLAGS) $< -o $@

executables/%.clang: %.c
	clang $(CLANG_CC_FLAGS) $< -o $@

executables/%.gcc: %.c
	gcc $(GCC_CC_FLAGS) $< -o $@

asts/%.clang.ast: %.c
	clang -Xclang -ast-dump -fsyntax-only $< -fno-color-diagnostics -- > $@

clean:
	rm -rf dwarves/ objects/ executables/ asts/ *.img
