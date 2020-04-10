#define _GNU_SOURCE 1

#include <stdio.h>
#include <dlfcn.h>

int INTERCEPT = 1;

int f(int x) {
	if (INTERCEPT) {
		printf("Derp: %d\n", x);
		INTERCEPT = 0;
	}
	int (*orig_f)(int) = dlsym(RTLD_NEXT, "f");
	return orig_f(x);
}