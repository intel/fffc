// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <unistd.h>
#include <stdio.h>

void foo(int i) {
	printf("%d\n", i);
    sleep(i);
}

int main(){
    foo(0);
}
