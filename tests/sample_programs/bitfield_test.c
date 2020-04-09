// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct test_struct {
	int i : 1;
	int j : 1;
	int : 0;
	int k : 4;
	float f;
};

int test(struct test_struct s) {
	return s.i;
}

int main() {
	struct test_struct s;
	s.i = 1;
	s.j = 0;
	s.f = 21.2;
	test(s);
	return 0;
}
