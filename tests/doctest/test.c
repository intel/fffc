// test.c

// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: MIT

#include <stdio.h>

int f(int x) {
	printf("f(%d)\n", x);
	return x*x;
}

int g(int x) {
	printf("g(%d)\n", x);
	return x+1;
}

int main(void) {
	return f(g(f(g(0))));
}
