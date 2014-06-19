/*
 * Copyright (C) 2014 Philipp Rosenkranz
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @addtogroup  unittests
 * @{
 *
 * @file        tests-simplemap.h
 * @brief       Unittests for the ``simplemap`` module
 *
 * @author      Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 */
#ifndef __TESTS_GTSPMAP_H_
#define __TESTS_GTSPMAP_H_

#include "../unittests.h"

typedef struct {
    uint16_t src;
    uint64_t local_local; // << current local hardware time when beacon was processed
    uint64_t local_global; // << current local logical time when beacon was processed
    uint64_t remote_local; // << sender local time when message was sent
    uint64_t remote_global; // << sender global time when message was sent
    float remote_rate; // << sender current clock rate correction factor
    float relative_rate; // << current local clock rate relative to sender
} gtsp_sync_point_t;

/**
 * @brief   The entry point of this test suite.
 */
void tests_gtspmap(void);

/**
 * @brief   Generates tests for simplemap
 *
 * @return  embUnit tests if successful, NULL if not.
 */
Test *tests_gtspmap_tests(void);

#endif /* __TESTS_GTSPMAP_H_ */
/** @} */
