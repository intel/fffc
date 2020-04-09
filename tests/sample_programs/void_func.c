// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

void f(int *i) {
	*i = *i * *i;
}

int main() {
	int i = 0;
	f(&i);
	return 0;
}
