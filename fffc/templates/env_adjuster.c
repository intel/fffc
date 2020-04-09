// Copyright (C) 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

#define DUMMY_VAR "FFFC_DUMMY="
#define TARGET_SIZE 32768
#define EXPORT_CMD "export "
#define ALIGNMENT 16
#define DEBUG 0

long unsigned align(long unsigned i) {
	return ALIGNMENT * ((i+(ALIGNMENT-1))/ALIGNMENT);
}

int get_env_size(long unsigned *pointer_size, long unsigned *data_size) {
	char **e = environ;
	while (*e) {
		if (strncmp(*e, DUMMY_VAR, strlen(DUMMY_VAR)) == 0) {
			continue;
		}
		*pointer_size += sizeof(*e);
		*data_size += strlen(*e) + 1;
		e++;
	};
	*pointer_size += sizeof(*e);
	return 0;
}

int get_payload_size(	long unsigned pointer_size,
						long unsigned data_size,
						long unsigned target_size,
						long unsigned *payload_size) {
	// check out target size
	if (target_size & 0x1F) {
		return -1;
	}

	// get the rounded pointer size after our addition
	pointer_size += sizeof(void*);
	long unsigned new_pointer_alloc_size = align(pointer_size);
	target_size -= new_pointer_alloc_size;

	// get the rounded data size after our addition
	data_size += strlen(DUMMY_VAR) + 1; // This is the minimum size of variable we can make
	long unsigned new_data_alloc_size = align(data_size);
	target_size -= new_data_alloc_size;
	target_size += new_data_alloc_size - data_size;

	// check to make sure we can actually make this work
	if (target_size < 0) {
		return -1;
	}

	// go home
	*payload_size = target_size;
	return 0;
}

int build_value(char **var, long unsigned payload_size) {
	long unsigned total_length = strlen(DUMMY_VAR) + payload_size + 1;
	*var = malloc(total_length);
	if (!*var) {
		return -1;
	}
	memset(*var, 0, total_length);
	strcat(*var, DUMMY_VAR);
	char *value = *var + strlen(*var);
	memset(value, 'x', payload_size);
	printf("%s\n", *var);
	free(*var);
	return 0;
}

int teardown_value(char *var) {
	free(var);
	return 0;
}

int main(int argc, char **argv)
{
	// get our data
	long unsigned pointer_size = 0;
	long unsigned data_size = 0;
	get_env_size(&pointer_size, &data_size);

	// get the payload size
	long unsigned payload_size = 0;
	get_payload_size(pointer_size, data_size, TARGET_SIZE, &payload_size);

	// build the var
	char *env_var = 0;
	build_value(&env_var, payload_size);

	// go home
	return 0;
}
