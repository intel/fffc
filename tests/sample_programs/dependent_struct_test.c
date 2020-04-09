// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct a {
	int i;
};

struct b {
	struct a i;
};

int test(struct b t) {
	return t.i.i;
}

int main() {
	struct b t;
	return test(t);
}
