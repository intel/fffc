// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

union union_test {
	int i;
	float f;
};

int test(union union_test u) {
	return u.i;
}

int main() {
	union union_test u;
	u.i = -1;
	test(u);
	return 0;
}
