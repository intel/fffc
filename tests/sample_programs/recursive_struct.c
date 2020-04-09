// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct recursive {
	struct recursive *recurs;
	int i;
};

int test(struct recursive r) {
	return r.i;
}

int main() {
	struct recursive r;
	r.i = 0;
	test(r);
	return 0;
}
