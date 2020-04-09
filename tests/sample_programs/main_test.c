// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <stdio.h>

void __cyg_profile_func_enter (void *this_fn, void *call_site) __attribute__((no_instrument_function));
void __cyg_profile_func_exit  (void *this_fn, void *call_site) __attribute__((no_instrument_function));

void __cyg_profile_func_enter (void *this_fn, void *call_site)
{
printf("Function Entry : %p %p \n", this_fn, call_site);
}

void __cyg_profile_func_exit (void *this_fn, void *call_site)
{
printf("Function Exit : %p %p \n", this_fn, call_site);
}

int herpderp() {
	return 0;
}

int main() {
	return 0;
}
