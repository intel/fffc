// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

char const a = 7;

int test(char const a) {
	return (int) a;
}

int main() {
	test(a);
	return 0;
}
