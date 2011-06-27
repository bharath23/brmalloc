/*
 * Copyright (c) 2011 Bharath Ramesh <bramesh.dev@gmail.com>
 *
 * Distributed under the terms of GNU LGPL, version 2.1
 */

#include <stdio.h>
#include <string.h>
#include "brmalloc.h"

#define MIN_ALLOC_SIZE ((size_t)8U)
#define MAX_ALLOC_SIZE ((size_t)4096U)
#define MAX_NUM_ALLOCS 8192
#define NUM_ITERATIONS 32

struct __malloc_type {
	unsigned char *ptr;
	unsigned char pattern;
	size_t size;
};

typedef struct __malloc_type malloc_t;

int
main (void)
{
	malloc_t allocs[MAX_NUM_ALLOCS];
	int i, j;
	size_t k, size;
	unsigned char pattern, *ptr;

	memset (allocs, 0, sizeof (malloc_t) * MAX_NUM_ALLOCS);
	for (i = 0; i < NUM_ITERATIONS; ++i) {
		printf ("allocating memory.\n");
		for (j = 0; j < MAX_NUM_ALLOCS; ++j) {
			if (allocs[j].size != 0)
				continue;

			size = (random () % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE))
				+ MIN_ALLOC_SIZE;
			pattern = random () % 256;
			ptr = (unsigned char *) brm_malloc (size);
			if (ptr == NULL) {
				printf ("ERROR: brm_malloc failed.\n");
				continue;
			}

			memset (ptr, pattern, size);
			allocs[j].pattern = pattern;
			allocs[j].ptr = ptr;
			allocs[j].size = size;
		}

		printf ("testing for memory corruption.\n");
		for (j = 0; j < MAX_NUM_ALLOCS; ++j) {
			pattern = allocs[j].pattern;
			ptr = allocs[j].ptr;
			size = allocs[j].size;
			if (size == 0)
				continue;

			for (k = 0; k < size; ++k) {
				if (ptr[k] == pattern)
					continue;

				printf ("ptr: %p, pattern: %hhu, detected: "
						"%hhu\n", ptr, pattern, ptr[k]);
				printf ("ERROR: memory overrun detected.\n");
				exit (EXIT_FAILURE);
			}
		}

		printf ("pattern test successful.\n");
		printf ("freeing random allocations.\n");
		for (j = 0; j < MAX_NUM_ALLOCS; ++j) {
			size = allocs[j].size;
			if (size == 0)
				continue;

			if (random () & 1) {
				brm_free (allocs[j].ptr);
				allocs[j].pattern = 0;
				allocs[j].ptr = NULL;
				allocs[j].size = 0;
			}
		}
	}

	for (j = 0; j < MAX_NUM_ALLOCS; ++j) {
		size = allocs[j].size;
		if (size == 0)
			continue;

		brm_free (allocs[j].ptr);
		allocs[j].pattern = 0;
		allocs[j].ptr = NULL;
		allocs[j].size = 0;
	}

	printf ("malloc test success.\n");

	return EXIT_SUCCESS;
}
