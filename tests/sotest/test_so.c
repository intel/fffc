#include <stdio.h>

int f(int x) {
	printf("Herp: %d\n", x);
	if (x < 12) {
		f(x+1);
	}
	return 0;
}