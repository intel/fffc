// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <alloca.h>

void foo() {
    int i = 0;
    int *j = alloca(256);
    int k = 0;

}

int main(){
    foo();
}
