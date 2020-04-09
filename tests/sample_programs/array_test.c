// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

int arr[1024];
float arr2[10][10];

int test(float array[10][10]) {
	return array[0][0];
}

int main() {
	test(arr2);
	return 0;
}
