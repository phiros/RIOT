/*
 * Copyright (C) 2014 Philipp Rosenkranz
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @addtogroup  unittests
 * @{
 *
 * @file        tests-posix.h
 * @brief       Unittests for the ``posix`` module (excluding pnet)
 *
 * @author      Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 */
#ifndef __TESTS_POSIX_H_
#define __TESTS_POSIX_H_

#include "../unittests.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   The entry point of this test suite.
 */
void tests_posix(void);

/**
 * @brief   Generates tests for posix semaphores
 *
 * @return  embUnit tests if successful, NULL if not.
 */
Test *tests_posix_semaphore_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* __TESTS_POSIX_H_ */
/** @} */
