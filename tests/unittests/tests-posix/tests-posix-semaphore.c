/*
 * Copyright (C) 2014 Philipp Rosenkranz
 * Copyright (C) 2013 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */
#include "thread.h"
#include "semaphore.h"
#include "mutex.h"

#include "tests-posix.h"

char simple_thread_stack[KERNEL_CONF_STACKSIZE_MAIN];

static sem_t semaphore;
static kernel_pid_t simple_pid;

static mutex_t assertion_mutex;
#define TEST_ASSERT_THREADED_EQUAL_INT(expected_, actual_) \
    do { \
        long long ____expected__ = (long long) (expected_); \
        long long ____actual__ = (long long) (actual_); \
        if (____expected__ != ____actual__) { \
            mutex_lock(&assertion_mutex); \
            assertImplementationLongLong(____expected__, ____actual__, __LINE__, __FILE__); \
            mutex_unlock(&assertion_mutex); \
        } \
    } while (0)

static void *simple_thread(void *arg)
{
    (void) arg;
    TEST_ASSERT_THREADED_EQUAL_INT(0, 1);
    return NULL;
}

static void tests_posix_semaphore_two_threads(void)
{
    mutex_init(&assertion_mutex);
    simple_pid = thread_create(simple_thread_stack,
                                         sizeof(simple_thread_stack),
                                         PRIORITY_MAIN - 1,
                                         CREATE_STACKTEST | CREATE_WOUT_YIELD,
                                         simple_thread,
                                         NULL,
                                         "second");
    TEST_ASSERT_THREADED_EQUAL_INT(0, (int) simple_pid);
}

static void tests_posix_semaphore_two_threads_with_priority(void)
{
    TEST_ASSERT_EQUAL_INT(0, 0);
}

Test *tests_posix_semaphore_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(tests_posix_semaphore_two_threads),
        new_TestFixture(tests_posix_semaphore_two_threads_with_priority),
};

EMB_UNIT_TESTCALLER(posix_tests, NULL, NULL, fixtures);

return (Test *)&posix_tests;
}
