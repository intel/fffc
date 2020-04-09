// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

enum Test {
	passed = 1,
	failed = 2,
	unknown = 2,
};

int test(enum Test t) {
	t = failed;
	return passed;
}

int main() {
	enum Test t = passed;
	test(t);
	return 0;
}
