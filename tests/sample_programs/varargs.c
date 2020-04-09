// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

typedef int (*myfunc) (int, ...);

int aaaargs(int i, ...) {
	return i;
}

int test(myfunc m) {
	return m(0);
}

int main() {
	test(aaaargs);
	return 0;
}
