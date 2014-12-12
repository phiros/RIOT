/*
 * Copyright (C) 2014 Philipp Rosenkranz
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "tests-posix.h"

void tests_posix(void)
{
    TESTS_RUN(tests_posix_semaphore_tests());
}
