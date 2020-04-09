// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

typedef int (*myfunc)(int, int);

int callback(int i, int j) {
	return i+j;
}

int test(myfunc f) {
	return f(0, 1);
}

int main() {
	return 0;
}
