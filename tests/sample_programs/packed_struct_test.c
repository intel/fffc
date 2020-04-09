// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <stdint.h>

struct test_struct_unpacked {
	uint8_t i;
	uint16_t f;
} tsu;

struct test_struct {
	uint8_t i;
	uint16_t f;
} __attribute__((packed));

struct test_struct2_unpacked {
	uint8_t i;
	uint16_t f : 1;
} ts2u;

struct test_struct2 {
	uint8_t i;
	uint16_t f : 1;
} __attribute__((packed));

struct test_struct3_unpacked {
	uint8_t i;
	uint16_t f : 1;
	uint32_t g[];
} ts3u;

struct test_struct3 {
	uint8_t i;
	uint16_t f : 1;
	uint32_t g[];
} __attribute__((packed));

struct test_struct4_unpacked {
	uint8_t i;
	uint16_t f;
	uint8_t g;
} ts4u;

struct test_struct4 {
	uint8_t i;
	uint16_t f;
	uint8_t g;
} __attribute__((packed));

int test(struct test_struct s) {
	return s.f;
}

int test2(struct test_struct2 s) {
	return s.f;
}

int test3(struct test_struct3 s) {
	return s.f;
}

int test4(struct test_struct4 s) {
	return s.g;
}

int main() {
	struct test_struct s;
	s.i = 0;
	s.f = 101;
	struct test_struct2 s2;
	s2.i = 0;
	s2.f = 101;
	struct test_struct3 s3;
	s3.i = 0;
	s3.f = 101;
	struct test_struct4 s4;
	s4.i = 0;
	s4.f = 101;
	s4.g = 7;
	test(s);
	test2(s2);
	test3(s3);
	test4(s4);
	return 0;
}
