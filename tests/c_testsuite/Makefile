# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

CLANG_CC_FLAGS = -g -O0 -fsanitize=address -fprofile-arcs -fPIC -fPIE -lm -shared-libasan
GCC_CC_FLAGS = -g -O0 -fsanitize=address -fprofile-arcs -fPIC -fPIE -lm

SOURCES = $(wildcard *.c)

GCC_OBJECTS = $(addprefix objects/,$(patsubst %.c,%.gcc.o,$(SOURCES)))
CLANG_OBJECTS = $(addprefix objects/,$(patsubst %.c,%.clang.o,$(SOURCES)))

GCC_DWARVES = $(addprefix dwarves/,$(patsubst %.c,%.gcc.dwarf,$(SOURCES)))
CLANG_DWARVES = $(addprefix dwarves/,$(patsubst %.c,%.clang.dwarf,$(SOURCES)))

GCC_EXECUTABLES = $(addprefix executables/,$(patsubst %.c,%.gcc,$(SOURCES)))
CLANG_EXECUTABLES = $(addprefix executables/,$(patsubst %.c,%.clang,$(SOURCES)))

.PHONY: directories

all: directories $(GCC_DWARVES) $(CLANG_DWARVES) $(GCC_OBJECTS) $(CLANG_OBJECTS) $(GCC_EXECUTABLES) $(CLANG_EXECUTABLES)

directories: dwarves/ objects/ executables/

dwarves/:
	-mkdir dwarves/

objects/:
	-mkdir objects/

executables/:
	-mkdir executables/

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

smoke: objects/ executables/ objects/00077.gcc.o executables/00077.gcc
	
smoke_crash: objects/ executables/ objects/00040.gcc.o executables/00040.gcc
	
smoke_multicall: objects/ executables/ objects/00221.gcc.o executables/00221.gcc

smoke_multicrash: objects/ executables/ objects/00222.gcc.o executables/00222.gcc
	
smoke_multiprint: objects/ executables/ objects/00223.gcc.o executables/00223.gcc
	
clean:
	rm -rf dwarves/ objects/ executables/ asts/
