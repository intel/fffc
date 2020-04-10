#define _GNU_SOURCE

#include <dlfcn.h>
#include <link.h>
#include <stdlib.h>
#include <stdio.h>

static int
callback(struct dl_phdr_info *info, size_t size, void *data) 
{
    int j;

   printf("name=%s (%d segments)\n", info->dlpi_name,
        info->dlpi_phnum);

   for (j = 0; j < info->dlpi_phnum; j++)
         printf("\t\t header %2d: address=%10p\n", j,
             (void *) (info->dlpi_addr + info->dlpi_phdr[j].p_vaddr));
    return 0;
}

void dothing() {
	printf("%s\n", "Before dlopen");
	dl_iterate_phdr(callback, NULL);
	void *handle = dlopen(NULL, RTLD_NOW);
	printf("%s\n", "After dlopen");
	dl_iterate_phdr(callback, NULL);
}


__attribute__((constructor))
static void c() {
	dothing();
}
