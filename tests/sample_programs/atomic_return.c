// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

_Atomic long long* test(_Atomic long long i) {
	return &i;
}

int main() {
	_Atomic long long i = 0;
	test(i);
	return 0;
}
