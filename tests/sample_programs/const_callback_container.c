// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

int test(const int i) {
	return (int)i*5/7;
}

struct CBContainer {
	int (*cb)(const int);
};

int main() {
	struct CBContainer c;
	c.cb = test;
	return c.cb(0);
}
