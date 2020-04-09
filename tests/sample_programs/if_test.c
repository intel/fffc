// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

int test(int i) {
	if (i < 7) {
		return -1;
	}
	return 0;
}

int main() {
	test(7);
	return 0;
}
