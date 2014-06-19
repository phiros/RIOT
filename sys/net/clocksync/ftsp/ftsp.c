/**
 * Copyright (C) 2014  Philipp Rosenkranz.
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup ftsp
 * @{
 * @file    ftsp.c
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @author  Daniel Jentsch <d.jentsch@fu-berlin.de>
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "thread.h"
#include "timex.h"
#include "vtimer.h"
#include "mutex.h"
#include "ieee802154_frame.h"
#include "sixlowpan/mac.h"
#include "malloc.h"

#include "clocksync/ftsp.h"
#include "gtimer.h"
#include "nalp_protocols.h"
#include "generic_ringbuffer.h"

#define ENABLE_DEBUG (0)
#include <debug.h>

#define GTSP_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_DEFAULT)
#define GTSP_CYCLIC_STACK_SIZE (KERNEL_CONF_STACKSIZE_DEFAULT)
#define GTSP_BEACON_BUFFER_SIZE (64)

#define LPC2387_FLOAT_CALC_TIME (10)
#define GTSP_MAX_NEIGHBORS (10)
#define GTSP_BEACON_INTERVAL (30 * 1000 * 1000)
#define GTSP_JUMP_THRESHOLD (10)
#define GTSP_MOVING_ALPHA 0.9

static void _ftsp_beacon_thread(void);
static void _ftsp_cyclic_driver_thread(void);
static void _ftsp_send_beacon(void);
static int _ftsp_buffer_lookup(generic_ringbuffer_t *rb, uint16_t src);

static int _ftsp_beacon_pid = 0;
static int _ftsp_clock_pid = 0;
static uint32_t _ftsp_beacon_interval = GTSP_BEACON_INTERVAL;
static uint32_t _ftsp_prop_time = 0;
static bool _ftsp_pause = true;
static uint32_t _ftsp_jump_threshold = GTSP_JUMP_THRESHOLD;
static bool ftsp_jumped = false;

static uint16_t _ftsp_root_id = UINT16_MAX;
//static uint64_t _ftsp_sync_error_estimate = 0;

char ftsp_beacon_stack[GTSP_BEACON_STACK_SIZE];
char ftsp_cyclic_stack[GTSP_CYCLIC_STACK_SIZE];
char ftsp_beacon_buffer[GTSP_BEACON_BUFFER_SIZE] =
{ 0 };
generic_ringbuffer_t ftsp_rb;
ftsp_sync_point_t ftsp_grb_buffer[GTSP_MAX_NEIGHBORS];

mutex_t ftsp_mutex;

void ftsp_init(void)
{
    mutex_init(&ftsp_mutex);
    grb_ringbuffer_init(&ftsp_rb, (char *) &ftsp_grb_buffer, GTSP_MAX_NEIGHBORS,
            sizeof(ftsp_sync_point_t));

    _ftsp_beacon_pid = thread_create(ftsp_beacon_stack, GTSP_BEACON_STACK_SIZE,
    PRIORITY_MAIN - 2, CREATE_STACKTEST, _ftsp_beacon_thread, "ftsp_beacon");

    _ftsp_clock_pid = thread_create(ftsp_cyclic_stack, GTSP_CYCLIC_STACK_SIZE,
    PRIORITY_MAIN - 2,
    CREATE_STACKTEST, _ftsp_cyclic_driver_thread, "ftsp_cyclic_driver");

    puts("GTSP initialized");
}

static void _ftsp_beacon_thread(void)
{
    while (1)
    {
        thread_sleep();
        DEBUG("_ftsp_beacon_thread locking mutex\n");
        mutex_lock(&ftsp_mutex);
        memset(ftsp_beacon_buffer, 0, sizeof(ftsp_beacon_t));
        if (!_ftsp_pause)
        {
            _ftsp_send_beacon();
        }
        mutex_unlock(&ftsp_mutex);
        DEBUG("_ftsp_beacon_thread: mutex unlocked\n");
    }
}

static void _ftsp_cyclic_driver_thread(void)
{
    while (1)
    {
        vtimer_usleep(_ftsp_beacon_interval);
        if (!_ftsp_pause)
        {
            DEBUG("_ftsp_cyclic_driver_thread: waking sending thread up");
            thread_wakeup(_ftsp_beacon_pid);
        }
    }
}

static void _ftsp_send_beacon(void)
{
    DEBUG("_ftsp_send_beacon\n");
    gtimer_timeval_t now;
    ftsp_beacon_t *ftsp_beacon = (ftsp_beacon_t *) ftsp_beacon_buffer;
    // NOTE: the calculation of the average rate used to be here
    gtimer_sync_now(&now);
    ftsp_beacon->dispatch_marker = GTSP_PROTOCOL_DISPATCH;
    ftsp_beacon->local = now.local + _ftsp_prop_time;
    ftsp_beacon->global = now.global + _ftsp_prop_time;
    ftsp_beacon->relative_rate = now.rate;
    sixlowpan_mac_send_ieee802154_frame(0, NULL, 8, ftsp_beacon_buffer,
            sizeof(ftsp_beacon_t), 1);
}

static float ftsp_compute_rate(void)
{
    DEBUG("ftsp_compute_rate\n");
    float avg_rate = gtimer_sync_get_relative_rate();
    int64_t sum_offset = 0;
    int offset_count = 0;
    int neighbor_count = 0;
    ftsp_sync_point_t beacon;
    ftsp_sync_point_t *last_rcvd_beacon = &beacon;

    int last_index = grb_get_last_index(&ftsp_rb);
    for (int i = 0; i < last_index; i++)
    {
        grb_get_element(&ftsp_rb, (void **) &last_rcvd_beacon, i);

        int64_t offset = (int64_t) last_rcvd_beacon->remote_global
                - (int64_t) last_rcvd_beacon->local_global;

        neighbor_count++;
        avg_rate += last_rcvd_beacon->relative_rate;

        if (offset > - _ftsp_jump_threshold)
        {
            // neighbor is ahead in time
            sum_offset += offset;
            offset_count++;
        }
    }
    if (offset_count > 0 && !ftsp_jumped)
    {
        int64_t correction = sum_offset / (offset_count + 1);
        if (ABS64T(correction) < _ftsp_jump_threshold)
        {
            gtimer_sync_set_global_offset(correction);
        }
    }

    ftsp_jumped = false;
    avg_rate /= (neighbor_count + 1);
    // little hack to make sure that clock rates don't get ridiculously large
    if (avg_rate > 0.00005)
        avg_rate = 0.00005;
    else if (avg_rate < -0.00005)
        avg_rate = -0.00005;
    return avg_rate;
}

void ftsp_mac_read(uint8_t *frame_payload, uint16_t src, gtimer_timeval_t toa)
{
    DEBUG("ftsp_mac_read");
    mutex_lock(&ftsp_mutex);
    ftsp_sync_point_t new_sync_point;
    ftsp_sync_point_t *sync_point;

    ftsp_beacon_t *ftsp_beacon = (ftsp_beacon_t *) frame_payload;

    if (_ftsp_pause)
    {
        mutex_unlock(&ftsp_mutex);
        return; // don't accept packets if ftsp is paused
    }

    // check for previously received beacons from the same node
    DEBUG("ftsp_mac_read: Looking up src address: %" PRIu16 "\n", src);
    if(ftsp_beacon->id < _ftsp_root_id && !(_ftsp_heartbeats < FTSP_IGNORE_ROOT_MSG && _ftsp_root_id == _ftsp_node_address)) {
        DEBUG("node new reference node elected");
    }
    mutex_unlock(&ftsp_mutex);
    DEBUG("ftsp_mac_read: mutex unlocked");
}

void ftsp_set_beacon_delay(uint32_t delay_in_sec)
{
    _ftsp_beacon_interval = delay_in_sec * 1000 * 1000;
}

void ftsp_set_prop_time(uint32_t us)
{
    _ftsp_prop_time = us;
}

void ftsp_pause(void)
{
    _ftsp_pause = true;
    DEBUG("GTSP disabled");
}

void ftsp_resume(void)
{
    _ftsp_pause = false;
    DEBUG("GTSP enabled");
}

void ftsp_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length)
{
    if (ieee802154_frame[0] == GTSP_PROTOCOL_DISPATCH)
    {
        gtimer_timeval_t now;
        ieee802154_frame_t frame;
        uint8_t hdrlen = ieee802154_frame_read(ieee802154_frame, &frame,
                frame_length);
        ftsp_beacon_t *beacon = (ftsp_beacon_t *) frame.payload;
        gtimer_sync_now(&now);
        beacon->local = now.local;
        beacon->global = now.global;
        memcpy(ieee802154_frame + hdrlen, beacon, sizeof(ftsp_beacon_t));
    }
}

static int _ftsp_buffer_lookup(generic_ringbuffer_t *rb, uint16_t src)
{
    int last_index = grb_get_last_index(rb);
    ftsp_sync_point_t *cur;
    for (int i = 0; i <= last_index; i++)
    {
        cur = (ftsp_sync_point_t *) &rb->buffer[i * rb->entry_size];
        DEBUG(
                "_ftsp_buffer_lookup: looking for src: %" PRIu16 " cur->src: %" PRIu16 "\n",
                src, cur->src);
        if (cur->src == src)
        {
            return i;
        }
    }
    return -1;
}
