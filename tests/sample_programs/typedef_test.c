// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

typedef int TEST;

TEST herp(TEST t) {
	t *= 7;
	t /= 3;
	return t;
}

int main() {
	TEST x = 0;
	herp(x);
	return 0;
}
