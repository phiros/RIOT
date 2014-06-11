/**
 * Bitvector declarations
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

#include <stdint.h>

#ifndef BITVECTOR_H_
#define BITVECTOR_H_

#define BITVECTOR_SIZE (256)

void bitvector_init(uint8_t *vec);

uint8_t bitvector_is_member(uint8_t *vec, uint8_t num);

void bitvector_add(uint8_t *vec, uint8_t num);

void bitvector_remove(uint8_t *vec, uint8_t num);
#endif /* BITVECTOR_H_ */
