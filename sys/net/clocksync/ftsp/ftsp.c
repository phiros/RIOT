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

/*
 * TODO: could _ftsp_seq_num overflow?
 * Seems kind of important for FTSP (indirectly determines root)...
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
#include "random.h"

#include "clocksync/ftsp.h"
#include "gtimer.h"
#include "nalp_protocols.h"
#include "generic_ringbuffer.h"

#ifdef MODULE_CC110X_NG
#include "cc110x_ng.h"
#define _TC_TYPE            TRANSCEIVER_CC1100

#elif MODULE_NATIVENET
#include "nativenet.h"
#define _TC_TYPE            TRANSCEIVER_NATIVE
#endif

#define ENABLE_DEBUG (0)
#include <debug.h>

#define GTSP_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_DEFAULT)
#define GTSP_CYCLIC_STACK_SIZE (KERNEL_CONF_STACKSIZE_DEFAULT)
#define GTSP_BEACON_BUFFER_SIZE (64)

#define FTSP_PREFERRED_ROOT (1) // node with id==1 will become root if possible
#define FTSP_ROOT_TIMEOUT (UINT32_MAX)
#define FTSP_IGNORE_ROOT_MSG (0)
#define FTSP_MAX_ENTRIES (8)
#define FTSP_SEND_LIMIT (3)
#define FTSP_SYNC_LIMIT (4)
#define FTSP_IGNORE_THRESHOLD (500)
#define FTSP_BEACON_INTERVAL (30 * 1000 * 1000)
#define FTSP_JUMP_THRESHOLD (10)

static void _ftsp_beacon_thread(void);
static void _ftsp_cyclic_driver_thread(void);
static void _ftsp_send_beacon(void);

static int _ftsp_beacon_pid = 0;
static int _ftsp_clock_pid = 0;
static uint32_t _ftsp_beacon_interval = FTSP_BEACON_INTERVAL;
static uint32_t _ftsp_prop_time = 0;
static bool _ftsp_pause = true;
static uint32_t _ftsp_jump_threshold = FTSP_JUMP_THRESHOLD;
static bool ftsp_jumped = false;

static uint16_t _ftsp_root_id = UINT16_MAX;
static uint16_t _ftsp_id = 0;
static uint32_t _ftsp_seq_num = 0;
static uint32_t _ftsp_heartbeats = 0;
static uint32_t _ftsp_errors = 0;
static uint32_t _ftsp_root_timeout = FTSP_ROOT_TIMEOUT;

char ftsp_beacon_stack[GTSP_BEACON_STACK_SIZE];
char ftsp_cyclic_stack[GTSP_CYCLIC_STACK_SIZE];
char ftsp_beacon_buffer[GTSP_BEACON_BUFFER_SIZE] =
{ 0 };

static void _ftsp_correct_clock(void);
static ftsp_sync_point_t *_ftsp_regression_table_enqueue(
        ftsp_sync_point_t *sync_point);
static ftsp_sync_point_t *_ftsp_regression_table_get_head(void);
static ftsp_sync_point_t *_ftsp_regression_table_get_tail(void);
static void _ftsp_regression_table_rm_head(void);
static ftsp_sync_point_t *_ftsp_regression_table_reset(void);
static uint16_t _ftsp_get_trans_addr(void);

typedef struct ftsp_regression_table
{
    ftsp_sync_point_t table[FTSP_MAX_ENTRIES];
    int rear;
    int front;
    int size;
} ftsp_regression_table_t;

static ftsp_regression_table_t _ftsp_rtable =
{ .table =
{
{ 0 } }, .rear = -1, .front = -1, .size = 0 };

mutex_t ftsp_mutex;

void ftsp_init(void)
{
    mutex_init(&ftsp_mutex);

    _ftsp_beacon_pid = thread_create(ftsp_beacon_stack, GTSP_BEACON_STACK_SIZE,
    PRIORITY_MAIN - 2, CREATE_STACKTEST, _ftsp_beacon_thread, "ftsp_beacon");

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

    if (_ftsp_root_id == UINT16_MAX)
    {
        if (++_ftsp_heartbeats >= _ftsp_root_timeout)
        {
            _ftsp_seq_num = 0;
            _ftsp_root_id = _ftsp_get_trans_addr;
            DEBUG("_ftsp_send_beacon: NODE: %"PRIu16" declares itself as root\n");
        }
    }
    else
    {
        // we have a root
        if (_ftsp_root_id != _ftsp_id && _ftsp_heartbeats >= _ftsp_root_timeout)
        {
            _ftsp_heartbeats = 0;
            _ftsp_root_id = _ftsp_id;
            _ftsp_seq_num++;
        }
        if (_ftsp_root_id != _ftsp_id && _ftsp_rtable.size < FTSP_SEND_LIMIT)
            return;
    }

    gtimer_sync_now(&now);
    ftsp_beacon->dispatch_marker = GTSP_PROTOCOL_DISPATCH;
    ftsp_beacon->id = _ftsp_id;
    // TODO: do we need to add the transmission delay to the offset?
    ftsp_beacon->offset = now.global - now.local;
    ftsp_beacon->relative_rate = now.rate;
    ftsp_beacon->root = _ftsp_root_id;
    ftsp_beacon->seq_number = _ftsp_seq_num;
    sixlowpan_mac_send_ieee802154_frame(0, NULL, 8, ftsp_beacon_buffer,
            sizeof(ftsp_beacon_t), 1);

    if (_ftsp_root_id == _ftsp_id)
        _ftsp_seq_num++;
}

void ftsp_mac_read(uint8_t *frame_payload, uint16_t src, gtimer_timeval_t toa)
{
    DEBUG("ftsp_mac_read");
    mutex_lock(&ftsp_mutex);
    ftsp_sync_point_t *sync_point;

    ftsp_beacon_t *ftsp_beacon = (ftsp_beacon_t *) frame_payload;

    if (_ftsp_pause)
    {
        mutex_unlock(&ftsp_mutex);
        return; // don't accept packets if ftsp is paused
    }

    if (ftsp_beacon->id < _ftsp_root_id
            && !(_ftsp_heartbeats < FTSP_IGNORE_ROOT_MSG
                    && _ftsp_root_id == _ftsp_id))
    {
        DEBUG("ftsp_mac_read: Switched reference node to: %"PRIu16"\n",ftsp_beacon->id);
        _ftsp_root_id = ftsp_beacon->root;
        _ftsp_seq_num = ftsp_beacon->seq_number;
    }
    else if (_ftsp_root_id == ftsp_beacon->root
            && ftsp_beacon->seq_number > _ftsp_seq_num)
    {
        _ftsp_seq_num = ftsp_beacon->seq_number;
    }
    else
    {
        continue;
    }
    if (_ftsp_root_id < _ftsp_id)
        _ftsp_heartbeats = 0;

    uint64_t neighbor_global = ftsp_beacon->local + ftsp_beacon->offset;
    gtimer_timeval_t now;
    gtimer_sync_now(&now);

    int64_t sync_error = neighbor_global - now.global;
    if (sync_error < 0)
        sync_error = -1 * sync_error;

    if (ftsp_is_synced() && sync_error > FTSP_IGNORE_THRESHOLD)
    {
        if (++_ftsp_errors > 3)
            _ftsp_regression_table_reset();
        return; // ignore this beacon
    }

    _ftsp_errors = 0;

    ftsp_sync_point_t new_entry;
    new_entry.local = now.local;
    new_entry.offset = neighbor_global - now.local;

    if (_ftsp_rtable.size == FTSP_MAX_ENTRIES)
        _ftsp_regression_table_rm_head();

    _ftsp_regression_table_enqueue(new_entry);
    _ftsp_correct_clock();

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
    _ftsp_id = _ftsp_get_trans_addr();
    if (_ftsp_id == FTSP_PREFERRED_ROOT)
        _ftsp_root_timeout = 0;
    genrand_init((uint32_t) _ftsp_id);
    _ftsp_beacon_interval = (100
            + genrand_uint32() % FTSP_BEACON_INTERVAL) * 1000;
    _ftsp_pause = false;
    if (_ftsp_clock_pid == 0)
    {
        _ftsp_clock_pid = thread_create(ftsp_cyclic_stack,
        GTSP_CYCLIC_STACK_SIZE,
        PRIORITY_MAIN - 2,
        CREATE_STACKTEST, _ftsp_cyclic_driver_thread, "ftsp_cyclic_driver");
    }

    DEBUG("FTSP enabled");
}

static void _ftsp_correct_clock(void)
{
    ftsp_sync_point_t *entry;
    float skew = 0;

    if (_ftsp_rtable.size == 0)
        return; // nothing to do (yet)

    entry = _ftsp_regression_table_get_head();

    uint64_t new_local_avg = entry->local;
    int64_t new_offset_avg = entry->offset;

    int64_t local_sum = 0;
    int64_t local_avg_rest = 0;
    int64_t offset_sum = 0;
    int64_t offset_avg_rest = 0;
    int table_entries = _ftsp_rtable.size;

    // TODO: replace with foreach macro
    if (_ftsp_rtable.front <= _ftsp_rtable.rear)
    {
        entry = &_ftsp_rtable[_ftsp_rtable.front];
        for (int i = 0; i <= _ftsp_rtable.rear; i++)
        {
            entry = &_ftsp_rtable[i];
            local_sum += (((int64_t) entry->local) - new_local_avg)
                    / table_entries;
            local_avg_rest += (((int64_t) entry->local) - new_local_avg)
                    % table_entries;
            offset_sum += (entry->offset - new_offset_avg) / table_entries;
            offset_avg_rest += (entry->offset - new_offset_avg) % table_entries;
        }
    }
    else
    {
        entry = &_ftsp_rtable[_ftsp_rtable.front];
        for (int i = _ftsp_rtable.front; i < FTSP_MAX_ENTRIES; i++)
        {
            entry = &_ftsp_rtable[i];
            local_sum += (((int64_t) entry->local) - new_local_avg)
                    / table_entries;
            local_avg_rest += (((int64_t) entry->local) - new_local_avg)
                    % table_entries;
            offset_sum += (entry->offset - new_offset_avg) / table_entries;
            offset_avg_rest += (entry->offset - new_offset_avg) % table_entries;
        }
        for (int i = 0; i <= _ftsp_rtable.rear; i++)
        {
            entry = &_ftsp_rtable[i];
            local_sum += (((int64_t) entry->local) - new_local_avg)
                    / table_entries;
            local_avg_rest += (((int64_t) entry->local) - new_local_avg)
                    % table_entries;
            offset_sum += (entry->offset - new_offset_avg) / table_entries;
            offset_avg_rest += (entry->offset - new_offset_avg) % table_entries;
        }
    }

    new_local_avg += local_sum + local_avg_rest / table_entries;
    new_offset_avg += offset_sum + offset_avg_rest / table_entries;

    // TODO: replace with foreach macro
    if (_ftsp_rtable.front <= _ftsp_rtable.rear)
    {
        for (int i = _ftsp_rtable.front; i <= _ftsp_rtable.rear; i++)
        {
            entry = &_ftsp_rtable[i];
            int64_t a = entry->local - new_local_avg;
            int64_t b = entry->offset - new_offset_avg;

            local_sum += a * a;
            offset_sum += a * b;
        }
    }
    else
    {
        for (int i = _ftsp_rtable.front; i < FTSP_MAX_ENTRIES; i++)
        {
            entry = &_ftsp_rtable[i];
            int64_t a = entry->local - new_local_avg;
            int64_t b = entry->offset - new_offset_avg;

            local_sum += a * a;
            offset_sum += a * b;
        }
        for (int i = 0; i <= _ftsp_rtable.rear; i++)
        {
            entry = &_ftsp_rtable[i];
            int64_t a = entry->local - new_local_avg;
            int64_t b = entry->offset - new_offset_avg;

            local_sum += a * a;
            offset_sum += a * b;
        }
    }

    if (local_sum != 0)
    {
        skew = (float) offset_sum / (float) local_sum;
        gtimer_sync_set_relative_rate(skew);
    }

    entry = _ftsp_regression_table_get_tail();
    int64_t local_diff = entry->local - new_local_avg;

    int64_t offset_diff = (int64_t) (skew * local_diff);
    int64_t offset_est = new_offset_avg + offset_diff;
    gtimer_timeval_t now;
    gtimer_sync_now(&now);
    int64_t offset_now = now.global - now.local;
    int64_t offset = offset_est - offset_now;
    gtimer_sync_set_global_offset(offset);
}

// Neighbor table

static ftsp_sync_point_t *_ftsp_regression_table_enqueue(
        ftsp_sync_point_t *sync_point)
{
    ftsp_sync_point_t *sp;
    if (_ftsp_rtable.size < FTSP_MAX_ENTRIES)
    {
        if (_ftsp_rtable.front == -1 && _ftsp_rtable.front == _ftsp_rtable.rear)
            _ftsp_rtable.front = 0;
        _ftsp_rtable.rear = (_ftsp_rtable.rear + 1) % (FTSP_MAX_ENTRIES - 1);
        sp = &_ftsp_rtable.table[_ftsp_rtable.rear];
        _ftsp_rtable.size++;
    }
    memcpy(sp, sync_point, sizeof(ftsp_sync_point_t));
    return sp;
}

static ftsp_sync_point_t *_ftsp_regression_table_get_head(void)
{
    ftsp_sync_point_t *sp = NULL;
    if (_ftsp_rtable.size > 0)
    {
        sp = &_ftsp_rtable.table[_ftsp_rtable.front];
    }
    return sp;
}

static ftsp_sync_point_t *_ftsp_regression_table_get_tail(void)
{
    ftsp_sync_point_t *sp = NULL;
    if (_ftsp_rtable.size > 0)
    {
        sp = &_ftsp_rtable.table[_ftsp_rtable.rear];
    }
    return sp;
}

static void _ftsp_regression_table_rm_head(void)
{
    if (_ftsp_regression_table_get_head() != NULL)
    {
        if (_ftsp_rtable.front == _ftsp_rtable.rear)
        {
            _ftsp_rtable.front = -1;
            _ftsp_rtable.rear = -1;
        }
        else
        {
            _ftsp_rtable.front = (_ftsp_rtable.front + 1)
                    % (FTSP_MAX_ENTRIES - 1);
        }
        _ftsp_rtable.size--;
    }
}

static ftsp_sync_point_t *_ftsp_regression_table_reset(void)
{
    _ftsp_rtable.front = -1;
    _ftsp_rtable.rear = -1;
    _ftsp_rtable.size = 0;
}

static uint16_t _ftsp_get_trans_addr(void)
{
    msg_t mesg;
    transceiver_command_t tcmd;
    radio_address_t a;

    if (transceiver_pid < 0)
    {
        puts("Transceiver not initialized");
        return 1;
    }

    tcmd.transceivers = _TC_TYPE;
    tcmd.data = &a;
    mesg.content.ptr = (char *) &tcmd;
    mesg.type = GET_ADDRESS;

    msg_send_receive(&mesg, &mesg, transceiver_pid);
    return a;
}

