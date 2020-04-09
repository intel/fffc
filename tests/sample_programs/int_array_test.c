// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <stdlib.h>

int f(int *x) {
	return x[121];
}

int main() {
	int *vars = malloc(1024);
	return f(vars);
}
