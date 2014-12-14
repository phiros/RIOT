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

#define TEST_MSG_FAIL (1)
#define TEST_MSG_SUCCESS (0)

static int thread_counter = 0;

#define TEST_FAIL_THREADED(coordinator_pid) \
    do { \
        msg_t m; \
        m.content.value = TEST_MSG_FAIL; \
        msg_send(&m, coordinator_pid); \
    } while (0)

#define TEST_SUCCESS_THREADED(coordinator_pid) \
    do { \
        msg_t m; \
        m.content.value = TEST_MSG_SUCCESS; \
        msg_send(&m, coordinator_pid); \
    } while (0)

#define RUN_AS_UNITTEST_THREAD(thread, arg, stack, prio) \
    do { \
        thread_counter++; \
        thread_create(stack, sizeof(stack), \
                      prio, CREATE_STACKTEST, thread, arg, ""); \
    } while (0)



static void test_coordinator(void)
{
    msg_t m;
    while(1) {
        if(msg_try_receive(&m) == 1)
        {
            if(!m.content.value) TEST_ASSERT(0); // leads to threads starving
            else thread_counter--;
        }
        if(thread_counter <= 0) TEST_ASSERT(1);
    }
}

static void *simple_thread(void *arg)
{
    TEST_FAIL_THREADED(*((kernel_pid_t *) arg));
    return NULL;
}

static void test_simple_thread(void)
{
    kernel_pid_t main_pid = thread_getpid();
    // start threads
    RUN_AS_UNITTEST_THREAD(simple_thread, &main_pid, simple_thread_stack, PRIORITY_MAIN -1);
    // start coordinator
    test_coordinator();
}



static void tests_posix_semaphore_two_threads_with_priority(void)
{
    TEST_ASSERT_EQUAL_INT(0, 0);
}

Test *tests_posix_semaphore_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
    new_TestFixture(test_simple_thread),
    new_TestFixture(tests_posix_semaphore_two_threads_with_priority),
};

EMB_UNIT_TESTCALLER(posix_tests, NULL, NULL, fixtures);

return (Test *)&posix_tests;
}
