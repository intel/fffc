// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

int i = 4;
int j[10] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
int *k = j;

int main() {
	return j[i] - 5;
}
