// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct Test {
	int i;
	enum {
		derp,
		dirp,
		dorp
	} heep;
	int j;
};

union Test2 {
	int i;
	enum {
		derp2,
		dirp2,
		dorp2
	} heep;
	int j;
};

struct Test3 {
	int i;
	enum {
		derp3,
		dirp4,
		dorp5
	} heep;
	int j;
	enum {
		derp6,
		dirp7,
		dorp8
	} heep2;
	int k;
};

struct Test4 {
	int i;
	enum {
		derp9,
		dirp10,
		dorp11
	};
	int j;
	enum {
		derp12,
		dirp13,
		dorp14
	};
	int k;
};

struct Test t;
union Test2 t2;
struct Test3 t3;
struct Test4 t4;

int main() {}
