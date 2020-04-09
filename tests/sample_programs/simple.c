// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

int no_arg_test() {
	return 0;
}

int int_arg_test(int i) {
	return i*5/7^1337;
}

int float_arg_test(float f) {
	return (int) f*5/7^1337;
}

int char_arg_test(char c) {
	return (int) c*5/7^1337;
}

int double_arg_test(double d) {
	return (int) d*5/7^1337;
}

int two_arg_test(int i, int j) {
	return i + j*5/7^1337;
}

int main() {
	no_arg_test();
	int_arg_test(1);
	float_arg_test(0.0);
	char_arg_test('a');
	double_arg_test(0.0);
	two_arg_test(1,2);
	return 0;
}
