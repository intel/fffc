// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

struct s {
	int i;
	int j[];
};

int locals_test() {
	int i;
	float *j;
	struct s snakey;
	i = 0;
	j = 0;
	snakey.i = 0;
	return 0;
}

int main() {
	return locals_test();
}
