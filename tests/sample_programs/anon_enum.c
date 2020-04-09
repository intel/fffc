// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

typedef enum {RED, BLUE} annie;

int test(annie a) {
	return RED;
}

int main() {
	annie a;
	test(a);
	return 0;
}
