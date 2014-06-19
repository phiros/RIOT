/*
 * Copyright (C) 2014 Philipp Rosenkranz
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

#include "embUnit/embUnit.h"

#include <stddef.h>

#define ENABLE_DEBUG (0)
#include "debug.h"

#include <string.h>

#include "tests-gtspmap.h"

#define TEST_GTIMER_EPSILON (1000) // epsilon should be high enough for platforms without a FPU
#define TEST_GTIMER_OFFSET (100000)
#define TEST_GTIMER_RATE (0.000050)

#define GTSP_MAX_NEIGHBORS (3)

gtsp_sync_point_t *_gtsp_neighbor_add(uint16_t addr,
        gtsp_sync_point_t *sync_point);
gtsp_sync_point_t *_gtsp_neighbor_get(uint16_t addr);


gtsp_sync_point_t gtsp_neighbor_table[GTSP_MAX_NEIGHBORS] = { 0 };
uint32_t gtsp_neighbor_counter = 0;

static void test_find_null_in_empty_map_for_1_and_2(void)
{

    TEST_ASSERT(NULL == _gtsp_neighbor_get(1));
    TEST_ASSERT(NULL == _gtsp_neighbor_get(2));
}

static void test_add_1_and_check_val(void)
{
    gtsp_sync_point_t sync_point1;
    gtsp_sync_point_t *retrieved_sync_point1;

    sync_point1.src = 1;

    retrieved_sync_point1 =  _gtsp_neighbor_add(1, &sync_point1);

    TEST_ASSERT(sync_point1.src == retrieved_sync_point1->src);
}

static void test_find_1_and_check_val(void)
{
    gtsp_sync_point_t sync_point1;
    gtsp_sync_point_t *retrieved_sync_point1;

    sync_point1.src = 1;

    TEST_ASSERT(NULL != (retrieved_sync_point1 = _gtsp_neighbor_get(1)));
    if (retrieved_sync_point1 != NULL)
    {
        TEST_ASSERT(sync_point1.src == retrieved_sync_point1->src);
    }
}

static void test_add_2_and_check_val(void)
{
    gtsp_sync_point_t sync_point2;
    gtsp_sync_point_t *retrieved_sync_point2;

    sync_point2.src = 2;
    retrieved_sync_point2 = _gtsp_neighbor_add(2, &sync_point2);

    TEST_ASSERT(sync_point2.src == retrieved_sync_point2->src);
}

static void test_add_3_and_check_val(void)
{
    gtsp_sync_point_t s3;
    gtsp_sync_point_t *r3;

    s3.src = 3;
    r3 = _gtsp_neighbor_add(3, &s3);

    TEST_ASSERT(s3.src == r3->src);
}


static void test_add_5_should_overwrite_1(void)
{

    gtsp_sync_point_t s5;
    gtsp_sync_point_t *r5, *r1;

    s5.src = 5;
    r1 = _gtsp_neighbor_get(1);
    r5 = _gtsp_neighbor_add(5, &s5);



    TEST_ASSERT(s5.src == r5->src);
    TEST_ASSERT(s5.src == r1->src);
}

static void test_add_6_should_overwrite_2(void)
{
    gtsp_sync_point_t s6;
    gtsp_sync_point_t *r6, *r2;

    s6.src = 6;
    r2 = _gtsp_neighbor_get(2);
    r6 = _gtsp_neighbor_add(6, &s6);



    TEST_ASSERT(s6.src == r6->src);
    TEST_ASSERT(s6.src == r2->src);
}

static void test_add_7_should_overwrite_3(void)
{

    gtsp_sync_point_t s7;
    gtsp_sync_point_t *r7, *r3;

    s7.src = 7;
    r3 = _gtsp_neighbor_get(3);
    r7 = _gtsp_neighbor_add(7, &s7);

    TEST_ASSERT(s7.src == r7->src);
    TEST_ASSERT(s7.src == r3->src);
}

static void test_add_8_should_overwrite_5(void)
{

    gtsp_sync_point_t s8;
    gtsp_sync_point_t *r8, *r1;

    s8.src = 8;
    r1 = _gtsp_neighbor_get(5);
    r8 = _gtsp_neighbor_add(8, &s8);


    TEST_ASSERT(s8.src == r8->src);
    TEST_ASSERT(s8.src == r1->src);
}

static void test_map_contains_8_6_7(void)
{

    gtsp_sync_point_t *r7, *r6, *r8;

    r8 = _gtsp_neighbor_get(8);
    r7 = _gtsp_neighbor_get(7);
    r6 = _gtsp_neighbor_get(6);


    TEST_ASSERT(r8->src == 8);
    TEST_ASSERT(r7->src == 7);
    TEST_ASSERT(r6->src == 6);
}


Test *tests_gtspmap_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
    new_TestFixture(test_find_null_in_empty_map_for_1_and_2),
    new_TestFixture(test_add_1_and_check_val),
    new_TestFixture(test_find_1_and_check_val),
    new_TestFixture(test_add_2_and_check_val),
    new_TestFixture(test_add_3_and_check_val),
    new_TestFixture(test_add_5_should_overwrite_1),
    new_TestFixture(test_add_6_should_overwrite_2),
    new_TestFixture(test_add_7_should_overwrite_3),
    new_TestFixture(test_add_8_should_overwrite_5),
    new_TestFixture(test_map_contains_8_6_7),
};

    EMB_UNIT_TESTCALLER(gtspmap_tests, NULL, NULL, fixtures);

    return (Test *) &gtspmap_tests;
}

void tests_gtspmap(void)
{
    TESTS_RUN(tests_gtspmap_tests());
}



gtsp_sync_point_t *_gtsp_neighbor_add(uint16_t addr,
        gtsp_sync_point_t *sync_point)
{
    gtsp_sync_point_t *sp;
    if (NULL == (sp = _gtsp_neighbor_get(addr)))
    {
        sp = &gtsp_neighbor_table[gtsp_neighbor_counter];

        if (gtsp_neighbor_counter == GTSP_MAX_NEIGHBORS - 1)
        {
            gtsp_neighbor_counter = 0;
        }
        else
        {
            gtsp_neighbor_counter++;
        }
    }
    memcpy(sp, sync_point, sizeof(gtsp_sync_point_t));
    return sp;
}

gtsp_sync_point_t *_gtsp_neighbor_get(uint16_t addr)
{
    gtsp_sync_point_t *sync_point;
    for (int i = 0; i < GTSP_MAX_NEIGHBORS; i++)
    {
        sync_point = &gtsp_neighbor_table[i];
        if (sync_point->src == addr)
        {
            return sync_point;
        }
    }
    return NULL;
}
