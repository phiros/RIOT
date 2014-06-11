/**
 * Bitvector implementation
 *
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @file
 * @author Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @author Freie Universität Berlin, Computer Systems & Telematics
 *
 */

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bitvector.h"

#define SETBIT(a,n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define UNSETBIT(a,n) (a[n/CHAR_BIT] &= ~(1<<(n%CHAR_BIT)))
#define GETBIT(a,n) (a[n/CHAR_BIT] &  (1<<(n%CHAR_BIT)))

void bitvector_init(uint8_t *vec) {
	memset(vec, 0, BITVECTOR_SIZE);
}

uint8_t bitvector_is_member(uint8_t *vec, uint8_t num) {
	return GETBIT(vec, num);
}

void bitvector_add(uint8_t *vec, uint8_t num) {
	SETBIT(vec, num);
}

void bitvector_remove(uint8_t *vec, uint8_t num) {
	UNSETBIT(vec, num);
}
