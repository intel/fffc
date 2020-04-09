// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct b;

struct a {
	int j;
	struct b *i;
};

struct b {
	struct a i;
};

int test(struct a t2) {
	return t2.i->i.j;
}

int main() {
	struct b t1;
	struct a t2;
	t2.i = &t1;
	return test(t2);
}
