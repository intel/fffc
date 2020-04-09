// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct anon_member {
	int i;
	int : 4;
	int j;
};

int test(struct anon_member a) {
	return a.i;
}

int main() {
	struct anon_member a;
	a.j = 7;
	test(a);
	return 0;
}
