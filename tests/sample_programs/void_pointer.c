// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

typedef void** voidy;
int herp = 7;

int test(voidy v) {
	return herp / 5;
}

int main() {
	test(0);
}
