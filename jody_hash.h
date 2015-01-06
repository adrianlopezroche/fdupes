/* Jody Bruchon's fast hashing function (headers)
 *
 * Copyright (C) 2014-2015 by Jody Bruchon <jody@jodybruchon.com>
 * See jody_hash.c for more information.
 */

#ifndef _JODY_HASH_H
#define _JODY_HASH_H

#include <stdint.h>

typedef uint64_t hash_t;
#define JODY_HASH_SHIFT 11

extern hash_t jody_block_hash(const hash_t * restrict data,
		const hash_t start_hash, const unsigned int count);

#endif	/* _JODY_HASH_H */
