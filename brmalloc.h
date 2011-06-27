/*
 * Copyright (c) 2011 Bharath Ramesh <bramesh.dev@gmail.com>
 *
 * Distributed under the terms of GNU LGPL, version 2.1
 */

#ifndef __BRMALLOC_H__
#define __BRMALLOC_H__

#include <stdlib.h>

void brm_free (void *ptr);
void *brm_malloc (size_t size);

#endif /* __BRMALLOC_H__ */
