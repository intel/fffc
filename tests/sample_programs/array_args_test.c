// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <stdio.h>

int test_array_arg(int a[9]) {
	return a[8];
}

int test_static_array_arg(int a[static 9]) {
	return a[7];
}

int test_vla(int a[]) {
	return a[6];
}

int test_vla_defn(int n, int a[n]) {
	for (int i=1; i < n; i++) {
		a[i-1] += a[i];
	}
	return a[n-1];
}

int test_array_oob(int n, int a[9]) {
	printf("%d\n", n);
	for (int i=1; i < n; i++) {
		a[i-1] += a[i];
	}
	return a[n-1];
}

int test_crasher(int n) {
	int a[16];
	return a[n & 0xFFF];
}

int main() {
	int arr[9];
	test_array_arg(arr);
	test_static_array_arg(arr);
	test_vla(arr);
	test_vla_defn(9, arr);
	test_crasher(6);
	test_array_oob(6, arr);
	return 0;
}
