/**
 * Copyright (C) 2014  Philipp Rosenkranz.
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup gtsp
 * @{
 * @file    gtsp.c
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

#include "clocksync/gtsp.h"
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
#define GTSP_BEACON_INTERVAL (5 * 1000 * 1000)
#define GTSP_JUMP_THRESHOLD (10)
#define GTSP_MOVING_ALPHA 0.9

static void _gtsp_beacon_thread(void);
static void _gtsp_cyclic_driver_thread(void);
static void _gtsp_send_beacon(void);
static int _gtsp_buffer_lookup(generic_ringbuffer_t *rb, uint16_t src);

static int _gtsp_beacon_pid = 0;
static int _gtsp_clock_pid = 0;
static uint32_t _gtsp_beacon_interval = GTSP_BEACON_INTERVAL;
static uint32_t _gtsp_prop_time = 0;
static bool _gtsp_pause = true;
static uint32_t _gtsp_jump_threshold = GTSP_JUMP_THRESHOLD;
static bool gtsp_jumped = false;
//static uint64_t _gtsp_sync_error_estimate = 0;

char gtsp_beacon_stack[GTSP_BEACON_STACK_SIZE];
char gtsp_cyclic_stack[GTSP_CYCLIC_STACK_SIZE];
char gtsp_beacon_buffer[GTSP_BEACON_BUFFER_SIZE] =
{ 0 };
generic_ringbuffer_t gtsp_rb;
gtsp_sync_point_t gtsp_grb_buffer[GTSP_MAX_NEIGHBORS];

mutex_t gtsp_mutex;

void gtsp_init(void)
{
    mutex_init(&gtsp_mutex);
    grb_ringbuffer_init(&gtsp_rb, (char *) &gtsp_grb_buffer, GTSP_MAX_NEIGHBORS,
            sizeof(gtsp_sync_point_t));

    _gtsp_beacon_pid = thread_create(gtsp_beacon_stack, GTSP_BEACON_STACK_SIZE,
    PRIORITY_MAIN - 2, CREATE_STACKTEST, _gtsp_beacon_thread, "gtsp_beacon");

    _gtsp_clock_pid = thread_create(gtsp_cyclic_stack, GTSP_CYCLIC_STACK_SIZE,
            PRIORITY_MAIN - 2,
            CREATE_STACKTEST, _gtsp_cyclic_driver_thread, "gtsp_cyclic_driver");

    puts("GTSP initialized");
}

static void _gtsp_beacon_thread(void)
{
    while (1)
    {
        thread_sleep();
        DEBUG("_gtsp_beacon_thread locking mutex\n");
        mutex_lock(&gtsp_mutex);
        memset(gtsp_beacon_buffer, 0, sizeof(gtsp_beacon_t));
        if (!_gtsp_pause)
        {
            _gtsp_send_beacon();
        }
        mutex_unlock(&gtsp_mutex);
        DEBUG("_gtsp_beacon_thread: mutex unlocked\n");
    }
}

static void _gtsp_cyclic_driver_thread(void)
{
    while (1)
    {
        vtimer_usleep(_gtsp_beacon_interval);
        if (!_gtsp_pause)
        {
            DEBUG("_gtsp_cyclic_driver_thread: waking sending thread up");
            thread_wakeup(_gtsp_beacon_pid);
        }
    }
}

static void _gtsp_send_beacon(void)
{
    DEBUG("_gtsp_send_beacon\n");
    gtimer_timeval_t now;
    gtsp_beacon_t *gtsp_beacon = (gtsp_beacon_t *) gtsp_beacon_buffer;
    // NOTE: the calculation of the average rate used to be here
    gtimer_sync_now(&now);
    gtsp_beacon->dispatch_marker = GTSP_PROTOCOL_DISPATCH;
    gtsp_beacon->local = now.local + _gtsp_prop_time;
    gtsp_beacon->global = now.global + _gtsp_prop_time;
    gtsp_beacon->relative_rate = now.rate;
    sixlowpan_mac_send_ieee802154_frame(0, NULL, 8, gtsp_beacon_buffer,
            sizeof(gtsp_beacon_t), 1);
}

static float gtsp_compute_rate(void)
{
    DEBUG("gtsp_compute_rate\n");
    float avg_rate = gtimer_sync_get_relative_rate();
    float avg_offset = 0;
    int offset_count = 0;
    int neighbor_count = 0;
    gtsp_sync_point_t beacon;
    gtsp_sync_point_t *last_rcvd_beacon = &beacon;

    int last_index = grb_get_last_index(&gtsp_rb);
    for (int i = 0; i < last_index; i++)
    {
        grb_get_element(&gtsp_rb, (void **) &last_rcvd_beacon, i);

        int64_t offset = (int64_t) last_rcvd_beacon->remote_global
                - (int64_t) last_rcvd_beacon->local_global;

        neighbor_count++;
        avg_rate += last_rcvd_beacon->relative_rate;

        if (offset > 0)
        {
            // neighbor is ahead in time
            avg_offset += 1 * offset;
            offset_count++;
        }
        else
        {
            if (ABS64T(offset) < _gtsp_jump_threshold)
            {
                avg_offset += offset;
                offset_count++;
            }
        }
    }
    if (offset_count > 0 && !gtsp_jumped)
    {
        int64_t correction = ceilf(avg_offset / (offset_count + 1));
        if (ABS64T(correction) < _gtsp_jump_threshold)
        {
            gtimer_sync_set_global_offset(correction);
        }
    }

    gtsp_jumped = false;
    avg_rate /= (neighbor_count + 1);
    // little hack to make sure that clock rates don't get ridiculously large
    if (avg_rate > 0.00005)
        avg_rate = 0.00005;
    else if (avg_rate < -0.00005)
        avg_rate = -0.00005;
    return avg_rate;
}

void gtsp_mac_read(uint8_t *frame_payload, radio_packet_t *p)
{
    DEBUG("gtsp_mac_read");
    gtimer_timeval_t toa = p->toa;
    mutex_lock(&gtsp_mutex);
    gtsp_sync_point_t new_sync_point;
    gtsp_sync_point_t *sync_point;

    float relative_rate = 0.0f;
    gtsp_beacon_t *gtsp_beacon = (gtsp_beacon_t *) frame_payload;

    if (_gtsp_pause)
    {
        mutex_unlock(&gtsp_mutex);
        return; // don't accept packets if gtsp is paused
    }

    int buffer_index;
    // check for previously received beacons from the same node
    DEBUG("gtsp_mac_read: Looking up p->src address: %" PRIu16 "\n", p->src);
    if (-1 != (buffer_index = _gtsp_buffer_lookup(&gtsp_rb, p->src)))
    {
        DEBUG("gtsp_mac_read: _gtsp_buffer_lookup success!");
        grb_get_element(&gtsp_rb, (void **) &sync_point, buffer_index);
        // calculate local time between beacons
        int64_t delta_local = toa.local - sync_point->local_local;
        // calculate estimate of remote time between beacons
        int64_t delta_remote = -LPC2387_FLOAT_CALC_TIME // << float calculations take a long time on lpc2387 (no FPU)
        + (int64_t) gtsp_beacon->local - (int64_t) sync_point->remote_local
                + ((int64_t) gtsp_beacon->local
                        - (int64_t) sync_point->remote_local)
                        * gtsp_beacon->relative_rate;
        // estimate rate of the local clock relative to the rate of the remote clock
        float current_rate = (delta_remote - delta_local) / (float) delta_local;
        // use moving average filter in order to smoothen out short term fluctuations
        relative_rate = GTSP_MOVING_ALPHA * sync_point->relative_rate
                + (1 - GTSP_MOVING_ALPHA) * current_rate;
    }
    else
    {
        // create a new sync_point and add it to the ring buffer
        buffer_index = grb_add_element(&gtsp_rb, &new_sync_point);
        grb_get_element(&gtsp_rb, (void **) &sync_point, buffer_index);
        sync_point->src = p->src;
    }

    // store the received and calculated data in the sync point
    sync_point->local_local = toa.local;
    sync_point->local_global = toa.global;
    sync_point->remote_local = gtsp_beacon->local;
    sync_point->remote_global = gtsp_beacon->global;
    sync_point->remote_rate = gtsp_beacon->relative_rate;
    sync_point->relative_rate = relative_rate;

    // if our clock is to far behind jump to remote clock value
    int64_t offset = (int64_t) gtsp_beacon->global - (int64_t) toa.global;
    if (offset > _gtsp_jump_threshold)
    {
        gtsp_jumped = true;
        gtimer_sync_set_global_offset(offset);
    }DEBUG("gtsp_mac_read: gtsp_compute_rate");
    // compute new relative clock rate based on new sync point
    float avg_rate = gtsp_compute_rate();
    gtimer_sync_set_relative_rate(avg_rate);

    mutex_unlock(&gtsp_mutex);
    DEBUG("gtsp_mac_read: mutex unlocked");
}

void gtsp_set_beacon_delay(uint32_t delay_in_sec)
{
    _gtsp_beacon_interval = delay_in_sec * 1000 * 1000;
}

void gtsp_set_prop_time(uint32_t us)
{
    _gtsp_prop_time = us;
}

void gtsp_pause(void)
{
    _gtsp_pause = true;
    DEBUG("GTSP disabled");
}

void gtsp_resume(void)
{
    _gtsp_pause = false;
    DEBUG("GTSP enabled");
}

void gtsp_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length)
{
    gtimer_timeval_t now;
    ieee802154_frame_t frame;
    uint8_t hdrlen = ieee802154_frame_read(ieee802154_frame, &frame,
            frame_length);
    gtsp_beacon_t *beacon = (gtsp_beacon_t *) frame.payload;
    gtimer_sync_now(&now);
    beacon->local = now.local;
    beacon->global = now.global;
    memcpy(ieee802154_frame + hdrlen, beacon, sizeof(gtsp_beacon_t));
}

static int _gtsp_buffer_lookup(generic_ringbuffer_t *rb, uint16_t src)
{
    int last_index = grb_get_last_index(rb);
    gtsp_sync_point_t *cur;
    for (int i = 0; i <= last_index; i++)
    {
        cur = (gtsp_sync_point_t *) &rb->buffer[i * rb->entry_size];
        DEBUG(
                "_gtsp_buffer_lookup: looking for src: %" PRIu16 " cur->src: %" PRIu16 "\n",
                src, cur->src);
        if (cur->src == src)
        {
            return i;
        }
    }
    return -1;
}
