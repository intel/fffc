// test2.c

// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int f(char *x, char *unused) {
	if (*x != 'X') {
		exit(-1);
	}
	printf("%s\n", x);
	return 0;
}

int main(void) {
	char *arg = strdup("hello!");
	char *arg2 = strdup("supercalifragilisticexpialidocious(sp)");
	return f(arg, arg2);
}
