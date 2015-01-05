/* Jody Bruchon's fast hashing function
 *
 * This function was written to generate a fast hash that also has a
 * fairly low collision rate. The collision rate is much higher than
 * a secure hash algorithm, but the calculation is drastically simpler
 * and faster.
 *
 * Copyright (C) 2014-2015 by Jody Bruchon <jody@jodybruchon.com>
 * Released under the terms of the GNU GPL version 2
 *
 * 2015-01-03: Re-licensed under the fdupes license as detailed in the
 * README file. Though no longer required, I would still request that
 * anyone who improves this code send me patches! Thanks. -Jody
 */

#include <stdio.h>
#include <stdlib.h>
#include "jody_hash.h"

/* Hash a block of arbitrary size; must be divisible by sizeof(hash_t)
 * The first block should pass a start_hash of zero.
 * All blocks after the first should pass start_hash as the value
 * returned by the last call to this function. This allows hashing
 * of any amount of data. If data is not divisible by the size of
 * hash_t, it is MANDATORY that the caller provide a data buffer
 * which is divisible by sizeof(hash_t). */
extern hash_t jody_block_hash(const hash_t * restrict data,
		const hash_t start_hash, const unsigned int count)
{
	register hash_t hash = start_hash;
	unsigned int len;
	hash_t tail;

#ifdef ARCH_HAS_LITTLE_ENDIAN
	/* Little-endian 64-bit hash_t tail mask */
	const hash_t le64_tail_mask[] = {
		0x0000000000000000,
		0xff00000000000000,
		0xffff000000000000,
		0xffffff0000000000,
		0xffffffff00000000,
		0xffffffffff000000,
		0xffffffffffff0000,
		0xffffffffffffff00,
		0xffffffffffffffff,
	};
 #define TAIL_MASK le64_tail_mask
#else
	/* Big-endian 64-bit hash_t tail mask */
	const hash_t be64_tail_mask[] = {
		0x0000000000000000,
		0x00000000000000ff,
		0x000000000000ffff,
		0x0000000000ffffff,
		0x00000000ffffffff,
		0x000000ffffffffff,
		0x0000ffffffffffff,
		0x00ffffffffffffff,
		0xffffffffffffffff,
	};
 #define TAIL_MASK be64_tail_mask
#endif	/* ARCH_HAS_LITTLE_ENDIAN */

	len = count / sizeof(hash_t);
	for (; len > 0; len--) {
		hash += *data;
		hash = (hash << JODY_HASH_SHIFT) | hash >> (sizeof(hash_t) * 8 - JODY_HASH_SHIFT);
		hash += (*data & (hash_t)0x000000ff);
		hash ^= (*data);
		hash += (*data & (hash_t)0xffffff00);
		hash = (hash << JODY_HASH_SHIFT) | hash >> (sizeof(hash_t) * 8 - JODY_HASH_SHIFT);
		hash += *data;
		data++;
	}

	/* Handle data tail (for blocks indivisible by sizeof(hash_t)) */
	len = count & (sizeof(hash_t) - 1);
	if (len) {
		tail = *data;
		tail &= TAIL_MASK[len];
		hash += tail;
		hash = (hash << JODY_HASH_SHIFT) | hash >> (sizeof(hash_t) * 8 - JODY_HASH_SHIFT);
		hash += (tail & (hash_t)0x000000ff);
		hash ^= (tail);
		hash += (tail & (hash_t)0xffffff00);
		hash = (hash << JODY_HASH_SHIFT) | hash >> (sizeof(hash_t) * 8 - JODY_HASH_SHIFT);
		hash += tail;
	}

	return hash;
}
