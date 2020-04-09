// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

typedef struct { int i;} anonymous;

int test(anonymous a) {
	return a.i;
}

int main() {
	anonymous a;
	a.i = 1;
	return 0;
}
