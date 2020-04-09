// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

int test(char *c) {
	return 0;
}

int main() {
	char c = 'a';
	test(&c);
	return 0;
}
