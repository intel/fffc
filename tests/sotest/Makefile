# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

GCC_CC_FLAGS = -g -O0 -fsanitize=address -fprofile-arcs -lm -fno-common -Wl,-rpath=.

all:
	gcc $(GCC_CC_FLAGS) -shared -o libtest_so_constructor.so -fPIC test_constructor.c -ldl
	gcc $(GCC_CC_FLAGS) -shared -o libtest_so.so -fPIC test_so.c
	gcc $(GCC_CC_FLAGS) -shared -o libtest_so_interceptor.so -fPIC test_so_interceptor.c -ldl
	gcc $(GCC_CC_FLAGS) -o test_exe test_exe.c -L. -ltest_so

run:
	./test_exe
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libasan.so.4.0.0 libtest_so_interceptor.so" ./test_exe
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libasan.so.4.0.0 libtest_so_constructor.so" ./test_exe

fffc:
	fffc --overwrite test_exe ./fffc_output/
	setarch `uname -m` -R ./fffc_output/test_exe/f_runner.sh

clean:
	rm -rf libtest_so_constructor.so libtest_so_interceptor.so libtest_so.so test_exe ./fffc_output *.gcda


