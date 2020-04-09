// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <stdint.h>

struct test_struct {
	int i;
	float f;
};

int test(struct test_struct s) {
	return s.i;
}

int test_pointer(struct test_struct *s) {
	return s->i;
}

struct test_struct2 {
	uint8_t i;
	uint16_t f;
};

int test2(struct test_struct2 s) {
	return s.f;
}

int test2_pointer(struct test_struct2 *s) {
	return s->f;
}

int main() {
	struct test_struct s;
	s.i = 40404;
	s.f = -1.2;
	test(s);
	test_pointer(&s);
	struct test_struct2 s2;
	test2(s2);
	test2_pointer(&s2);
	return 0;
}
