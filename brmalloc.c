/*
 * Copyright (c) 2011 Bharath Ramesh <bramesh.dev@gmail.com>
 *
 * Distributed under the terms of GNU LGPL, version 2.1
 */

#include <stdint.h>
#include <string.h>
#include "brmalloc.h"

#define BITS_PER_LONG 64
#define LOG_BITS_PER_BYTE 3
#define LOG_BITS_PER_LONG 6
#define LOG_BYTES_PER_LONG 3
#define MAX_LONG 0xffffffffffffffff
#define K ((size_t)1024U)
#define M ((size_t)1024U * K)
#define G ((size_t)1024U * M)
#define MALLOC_OVERHEAD ((size_t)16U)

#ifndef ABORT
#define ABORT() abort()
#endif

#ifndef EXIT_ERR
#define EXIT_ERR() exit(EXIT_FAILURE)
#endif

#ifndef DEBUG
#include <stdio.h>
#define DEBUG(fmt, args...) printf("%d, %s: " fmt, __LINE__, __FUNCTION__, ##args)
#endif

#ifndef MALLOC_QUANTA
#define MALLOC_QUANTA ((size_t)16U)
#endif

#ifndef MALLOC_THRESHOLD
#define MALLOC_THRESHOLD ((size_t)1U * M)
#endif

#ifndef MALLOC_ZONE_SIZE
#define MALLOC_ZONE_SIZE ((size_t)16U * M)
#endif

#ifndef MMAP
#include <sys/mman.h>
#define MMAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE)
#define MMAP_PROT (PROT_READ | PROT_WRITE)
#define MMAP(s) mmap (0, (s), MMAP_PROT, MMAP_FLAGS, -1, 0)
#define MUNMAP(a, s) munmap ((a), (s))
#endif

struct __malloc_zone_type {
	unsigned char *bmap;
	size_t bmap_longs;
	size_t free_size;
	uint64_t id;
	void *region;
	uint64_t start_byte;
	struct __malloc_zone_type *next;
};

typedef struct __malloc_zone_type malloc_zone_t;

static malloc_zone_t *mz_head = NULL, *mz_tail = NULL;
static uint8_t mqbits = 0;

static inline void
free_region (malloc_zone_t *mz, uint64_t loc, size_t nbytes)
{
	unsigned char *bmap;
	size_t bmap_longs;
	uint64_t i, i_b, j_b, lbits, nbits, pattern, *wordptr;

	lbits = nbits = nbytes >> mqbits;
	i_b = loc >> LOG_BITS_PER_LONG;
	j_b = loc - (i_b << LOG_BITS_PER_LONG);
	bmap = mz->bmap;
	bmap_longs = mz->bmap_longs;
	for (i = i_b; i < bmap_longs; ++i) {
		if (lbits == 0)
			break;

		wordptr = (uint64_t *) bmap + i;
		if (i == i_b) {
			if ((j_b + nbits) < BITS_PER_LONG) {
				pattern = ((uint64_t) 1 << nbits) - 1;
				pattern = ~(pattern << j_b);
				*wordptr &= pattern;
				return;
			}

			pattern = ~(MAX_LONG << j_b);
			*wordptr &= pattern;
			lbits -= (BITS_PER_LONG -j_b);
			continue;
		}

		if (lbits >= BITS_PER_LONG) {
			*wordptr = 0;
			lbits -= BITS_PER_LONG;
			continue;
		}

		pattern = ~(((uint64_t) 1 << lbits) - 1);
		*wordptr &= pattern;
		return;
	}

	return;
}

static inline uint64_t
get_region (malloc_zone_t *mz, size_t nbytes)
{
	unsigned char *bmap;
	size_t bmap_longs;
	uint64_t i = 0, i_b, j, j_b, l, nbits, rbits, pattern, word, *wordptr;
	int64_t loc = -1;

	nbits = rbits = nbytes >> mqbits;
	bmap = mz->bmap;
	bmap_longs = mz->bmap_longs;
	l = mz->start_byte;
	while (i < bmap_longs) {
		if (rbits == 0)
			break;

		word = *((uint64_t *) bmap + l);
		if (word == MAX_LONG) {
			loc = -1;
			rbits = nbits;
			++i;
			++l;
			if (l == bmap_longs)
				l = 0;

			continue;
		}

		if (word == 0) {
			if (rbits >= BITS_PER_LONG) {
				rbits -= BITS_PER_LONG;
				if (loc == -1)
					loc = l << LOG_BITS_PER_LONG;

				++i;
				++l;
				if (l == bmap_longs) {
					l = 0;
					loc = -1;
					rbits = nbits;
				}

				continue;
			}

			rbits = 0;
			if (loc == -1)
				loc = l << LOG_BITS_PER_LONG;

			break;
		}

		if (rbits >= BITS_PER_LONG) {
			loc = -1;
			rbits = nbits;
			++i;
			++l;
			if (l == bmap_longs)
				l = 0;

			continue;
		}

		for (j = 0; j < BITS_PER_LONG; ++j) {
			if (rbits == 0)
				break;

			if ((word >> j) & 1) {
				loc = -1;
				rbits = nbits;
				++i;
				++l;
				if (l == bmap_longs)
					l = 0;

				break;
			}

			--rbits;
			if (loc == -1)
				loc = (l << LOG_BITS_PER_LONG) + j;
		}
	}

	if ((rbits != 0) || (loc == -1))
		return -1;

	mz->start_byte = (loc + nbits) >> LOG_BITS_PER_LONG;
	i_b = loc >> LOG_BITS_PER_LONG;
	j_b = loc - (i_b << LOG_BITS_PER_LONG);
	rbits = nbits;
	for (i = i_b; i < bmap_longs; ++i) {
		if (rbits == 0)
			break;

		wordptr = (uint64_t *) bmap + i;
		if (i == i_b) {
			if ((j_b + nbits) < BITS_PER_LONG) {
				pattern = ((uint64_t) 1 << nbits) - 1;
				pattern = pattern << j_b;
				*wordptr |= pattern;
				return loc;
			}

			pattern = MAX_LONG << j_b;
			*wordptr |= pattern;
			rbits -= (BITS_PER_LONG - j_b);
			continue;
		}

		if (rbits >= BITS_PER_LONG) {
			*wordptr = MAX_LONG;
			rbits -= BITS_PER_LONG;
			continue;
		}

		pattern = ((uint64_t) 1 << rbits) - 1;
		*wordptr |= pattern;
		return loc;
	}


	return loc;
}

static inline malloc_zone_t *
new_malloc_zone (void)
{
	size_t bmap_size, bmap_longs;
	malloc_zone_t *mz;
	void *region;

	region = MMAP (MALLOC_ZONE_SIZE);
	if (region == MAP_FAILED) {
		DEBUG ("ERROR: MMAP failed.\n");
		return NULL;
	}

	bmap_size = (MALLOC_ZONE_SIZE / MALLOC_QUANTA) >> LOG_BITS_PER_BYTE;
	bmap_longs = bmap_size >> LOG_BYTES_PER_LONG;
	mz = (malloc_zone_t *) malloc (sizeof (malloc_zone_t));
	mz->bmap = (unsigned char *) malloc (bmap_size);
	if (mz->bmap == NULL) {
		DEBUG ("ERROR: Unable to allocate bitmap.\n");
		free (mz);
		MUNMAP (region, MALLOC_ZONE_SIZE);
		return NULL;
	}

	memset (mz->bmap, 0, bmap_size);
	mz->bmap_longs = bmap_longs;
	mz->free_size = MALLOC_ZONE_SIZE;
	mz->id = (uint64_t) mz;
	mz->region = region;
	mz->start_byte = 0;
	mz->next = NULL;
	if (mz_tail != NULL)
		mz_tail->next = mz;

	mz_tail = mz;
	if (mz_head == NULL)
		mz_head = mz;

	return mz;
}

static void __attribute__ ((constructor))
brm_init (void)
{
	uint8_t i, no_ones;
	size_t size;

	no_ones = 0;
	size = MALLOC_QUANTA;
	for (i = 0; i < BITS_PER_LONG; ++i) {
		if ((size >> i) & 1) {
			++no_ones;
			if (no_ones > 1) {
				DEBUG ("ERROR: MALLOC_QUANTA not power of "
						"2.\n");
				EXIT_ERR ();
			}

			mqbits = i;
		}
	}

	if (no_ones == 0) {
		DEBUG ("ERROR: MALLOC_QUANTA set to 0 (zero).\n");
		EXIT_ERR ();
	}

	if (new_malloc_zone () == NULL) {
		DEBUG ("ERROR: new_malloc_zone failed.\n");
		EXIT_ERR ();
	}

	return;
}

void
brm_free (void *ptr)
{
	uint64_t *base;
	malloc_zone_t *mz;
	int64_t offset;
	size_t size;

	base = (uint64_t *) ptr - 2;
	mz = (malloc_zone_t *) *((uint64_t *) base);
	size = (size_t) *((uint64_t *) base + 1);
	if (mz->id != (uint64_t) mz) {
		DEBUG ("ptr: %p, base: %p\n", ptr, base);
		DEBUG ("ERROR: data corruption.\n");
		ABORT ();
	}

	offset = ((void *) base - mz->region) >> mqbits;
	free_region (mz, offset, size);
	mz->free_size += size;

	return;
}

void *
brm_malloc (size_t size)
{
	uint64_t addr;
	malloc_zone_t *mz;
	size_t nbytes;
	int64_t offset;

	if (size == 0)
		return NULL;

	nbytes = (size + MALLOC_OVERHEAD + MALLOC_QUANTA - 1) &
		~(MALLOC_QUANTA - 1);
	if (nbytes >= MALLOC_THRESHOLD) {
		DEBUG ("UNIMPLEMENTED\n");
		EXIT_ERR();
	}

	mz = mz_head;
	while (1) {
		if (nbytes > mz->free_size) {
			DEBUG ("UNIMPLEMENTED\n");
			EXIT_ERR ();
		}

		offset = get_region (mz, nbytes);
		if (offset == -1) {
			mz = mz->next;
			if (mz == NULL) {
				if ((mz = new_malloc_zone ()) == NULL) {
					DEBUG ("ERROR: new_malloc zone "
							"failed.\n");
					EXIT_ERR ();
				}
			}

			continue;
		}

		mz->free_size -= nbytes;
		addr = (uint64_t) mz->region + ((size_t) offset << mqbits);
		*((uint64_t *) addr) = (uint64_t) mz;
		*((uint64_t *) addr + 1) = (uint64_t) nbytes;
		break;
	}

	return (void *) (addr + 2 * sizeof (uint64_t));
}
