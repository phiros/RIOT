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
#include "random.h"

#include "clocksync/ftsp.h"
#include "gtimer.h"
#include "nalp_protocols.h"
#include "generic_ringbuffer.h"
#include "x64toa.h"

#ifdef MODULE_CC110X_NG
#include "cc110x_ng.h"
#define _TC_TYPE            TRANSCEIVER_CC1100

#elif MODULE_NATIVENET
#include "nativenet.h"
#define _TC_TYPE            TRANSCEIVER_NATIVE
#endif

#define ENABLE_DEBUG (0)
#include <debug.h>

#define FTSP_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define FTSP_CYCLIC_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define FTSP_BEACON_BUFFER_SIZE (64)

#define FTSP_PREFERRED_ROOT (1) // node with id==1 will become root if possible
#define FTSP_ROOT_TIMEOUT (UINT32_MAX)
#define FTSP_IGNORE_ROOT_MSG (0)
#define FTSP_MAX_ENTRIES (8)
#define FTSP_SEND_LIMIT (3)
#define FTSP_SYNC_LIMIT (4)
#define FTSP_IGNORE_THRESHOLD (500)
#define FTSP_BEACON_INTERVAL (5 * 1000 * 1000) // in ms
#define FTSP_JUMP_THRESHOLD (10)

static void _ftsp_beacon_thread(void);
static void _ftsp_cyclic_driver_thread(void);
static void _ftsp_send_beacon(void);

static int _ftsp_beacon_pid = 0;
static int _ftsp_clock_pid = 0;
static uint32_t _ftsp_beacon_interval = FTSP_BEACON_INTERVAL;
static uint32_t _ftsp_prop_time = 0;
static bool _ftsp_pause = true;

static uint16_t _ftsp_root_id = UINT16_MAX;
static uint16_t _ftsp_id = 0;
static uint32_t _ftsp_seq_num = 0;
static uint32_t _ftsp_heartbeats = 0;
//static uint32_t _ftsp_errors = 0;
static uint32_t _ftsp_root_timeout = FTSP_ROOT_TIMEOUT;
//static int64_t _ftsp_last_offset = 0;

char ftsp_beacon_stack[FTSP_BEACON_STACK_SIZE];
char ftsp_cyclic_stack[FTSP_CYCLIC_STACK_SIZE];
char ftsp_beacon_buffer[FTSP_BEACON_BUFFER_SIZE] =
{ 0 };

//static void _ftsp_correct_clock(void);
//static bool _ftsp_is_synced(void);
//static ftsp_sync_point_t *_ftsp_regression_table_enqueue(
//        ftsp_sync_point_t *sync_point);
//static ftsp_sync_point_t *_ftsp_regression_table_get_head(void);
//static ftsp_sync_point_t *_ftsp_regression_table_get_tail(void);
//static void _ftsp_regression_table_rm_head(void);
//static void _ftsp_regression_table_reset(void);
static uint16_t _ftsp_get_trans_addr(void);

// KEEEP!!!
//static void _ftsp_print_sync_point(ftsp_sync_point_t *entry);
static void _ftsp_print_beacon(ftsp_beacon_t *beacon);

// contiki ftsp
static int8_t i, free_item, oldest_item;
static uint64_t local_average, age, oldest_time;
static uint8_t num_entries, table_entries, heart_beats, num_errors,
        seq_num;
static int64_t offset_average, time_error, a, b;
static int64_t local_sum, offset_sum;
static float skew;
static table_item table[MAX_ENTRIES];

static void clear_table(void);
static void add_new_entry(ftsp_beacon_t *beacon, gtimer_timeval_t *toa);
static void calculate_conversion(void);
int is_synced(void);

mutex_t ftsp_mutex;

/*
 * TODO: fix overflow problem in offset calculation
 */

void ftsp_init(void)
{
    mutex_init(&ftsp_mutex);

    skew = 0.0;
    local_average = 0;
    offset_average = 0;
    clear_table();
    heart_beats = 0;
    num_errors = 0;

    _ftsp_beacon_pid = thread_create(ftsp_beacon_stack, FTSP_BEACON_STACK_SIZE,
    PRIORITY_MAIN - 2, CREATE_STACKTEST, _ftsp_beacon_thread, "ftsp_beacon");

    puts("FTSP initialized");
}

static void _ftsp_beacon_thread(void)
{
    while (1)
    {
        thread_sleep();
        puts("_ftsp_beacon_thread locking mutex\n");
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
    genrand_init((uint32_t) _ftsp_id);
    uint32_t random_wait = (100 + genrand_uint32() % FTSP_BEACON_INTERVAL);
    vtimer_usleep(random_wait);

    while (1)
    {
        vtimer_usleep(_ftsp_beacon_interval);
        if (!_ftsp_pause)
        {
            puts("_ftsp_cyclic_driver_thread: waking sending thread up");
            thread_wakeup(_ftsp_beacon_pid);
        }
    }
}

static void _ftsp_send_beacon(void)
{
    puts("_ftsp_send_beacon\n");
    gtimer_timeval_t now;
    ftsp_beacon_t *ftsp_beacon = (ftsp_beacon_t *) ftsp_beacon_buffer;
    // NOTE: the calculation of the average rate used to be here

    if (_ftsp_root_id == UINT16_MAX)
    {
        if (++_ftsp_heartbeats >= _ftsp_root_timeout)
        {
            _ftsp_seq_num = 0;
            _ftsp_root_id = _ftsp_id;
            printf(
                    "_ftsp_send_beacon: NODE: %"PRIu16" declares itself as root\n",
                    _ftsp_id);
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
        if (_ftsp_root_id != _ftsp_id && num_entries < FTSP_SEND_LIMIT)
            return;
    }

    gtimer_sync_now(&now);
    ftsp_beacon->dispatch_marker = FTSP_PROTOCOL_DISPATCH;
    ftsp_beacon->id = _ftsp_id;
    // TODO: do we need to add the transmission delay to the offset?
    ftsp_beacon->offset = (int64_t) now.global - (int64_t) now.local;
    ftsp_beacon->local = now.local;
    ftsp_beacon->relative_rate = now.rate;
    ftsp_beacon->root = _ftsp_root_id;
    ftsp_beacon->seq_number = seq_num;
    puts("_ftsp_send_beacon: sending beacon with contents:\n");
    _ftsp_print_beacon(ftsp_beacon);
    sixlowpan_mac_send_ieee802154_frame(0, NULL, 8, ftsp_beacon_buffer,
            sizeof(ftsp_beacon_t), 1);

    if (_ftsp_root_id == _ftsp_id)
        _ftsp_seq_num++;
}

void ftsp_mac_read(uint8_t *frame_payload, uint16_t src, gtimer_timeval_t *toa)
{
    puts("ftsp_mac_read entry");
    mutex_lock(&ftsp_mutex);
    puts("ftsp_mac_read after mutex");

    ftsp_beacon_t *beacon = (ftsp_beacon_t *) frame_payload;
    if ((beacon->root < _ftsp_root_id)
            && ~((heart_beats < IGNORE_ROOT_MSG) && (_ftsp_root_id == _ftsp_id)))
    {
        _ftsp_root_id = beacon->root;
        seq_num = beacon->seq_number;
    }
    else
    {
        if ((_ftsp_root_id == beacon->root)
                && ((int8_t) (beacon->seq_number - seq_num) > 0))
        {
            seq_num = beacon->seq_number;
        }
        else
        {
            mutex_unlock(&ftsp_mutex);
            return;
        }
    }

    if (_ftsp_root_id < _ftsp_id)
        heart_beats = 0;

    add_new_entry(beacon, toa);
    calculate_conversion();

    mutex_unlock(&ftsp_mutex);
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
    DEBUG("FTSP disabled");
}

void ftsp_resume(void)
{
    _ftsp_id = _ftsp_get_trans_addr();
    if (_ftsp_id == FTSP_PREFERRED_ROOT)
        _ftsp_root_timeout = 0;

    _ftsp_pause = false;
    if (_ftsp_clock_pid == 0)
    {
        puts("thread started");
        _ftsp_clock_pid = thread_create(ftsp_cyclic_stack,
        FTSP_CYCLIC_STACK_SIZE,
        PRIORITY_MAIN - 2,
        CREATE_STACKTEST, _ftsp_cyclic_driver_thread, "ftsp_cyclic_driver");
    }

    puts("FTSP enabled");
}

/*
static bool _ftsp_is_synced(void)
{
    if (_ftsp_rtable.size >= FTSP_SYNC_LIMIT)
        return true;
    else
        return false;
}
*/
/*
static void _ftsp_correct_clock(void)
{
    puts("_ftsp_correct_clock");
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

    char buf[66];

    // TODO: replace with foreach macro
    if (_ftsp_rtable.front <= _ftsp_rtable.rear)
    {
        for (int i = _ftsp_rtable.front; i <= _ftsp_rtable.rear; i++)
        {
            printf("_ftsp_rtable.front <= _ftsp_rtable.rear loop index: %d\n",
                    i);
            entry = &_ftsp_rtable.table[i];
            _ftsp_print_sync_point(entry);
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
        entry = &_ftsp_rtable.table[_ftsp_rtable.front];
        for (int i = _ftsp_rtable.front; i < FTSP_MAX_ENTRIES; i++)
        {
            printf("_ftsp_rtable.front > _ftsp_rtable.rear loop1 index: %d\n",
                    i);
            entry = &_ftsp_rtable.table[i];
            _ftsp_print_sync_point(entry);
            local_sum += (((int64_t) entry->local) - new_local_avg)
                    / table_entries;
            local_avg_rest += (((int64_t) entry->local) - new_local_avg)
                    % table_entries;
            offset_sum += (entry->offset - new_offset_avg) / table_entries;
            offset_avg_rest += (entry->offset - new_offset_avg) % table_entries;
        }
        for (int i = 0; i <= _ftsp_rtable.rear; i++)
        {
            printf("_ftsp_rtable.front > _ftsp_rtable.rear loop2 index: %d\n",
                    i);
            entry = &_ftsp_rtable.table[i];
            _ftsp_print_sync_point(entry);
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
            entry = &_ftsp_rtable.table[i];
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
            entry = &_ftsp_rtable.table[i];
            int64_t a = entry->local - new_local_avg;
            int64_t b = entry->offset - new_offset_avg;

            local_sum += a * a;
            offset_sum += a * b;
        }
        for (int i = 0; i <= _ftsp_rtable.rear; i++)
        {
            entry = &_ftsp_rtable.table[i];
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
    printf("_ftsp_correct_clock: local_diff %s ",
            l2s(local_diff, X64LL_SIGNED, buf));
    printf(" entry->local %s ", l2s(entry->local, X64LL_SIGNED, buf));
    printf(" new_local_avg %s \n", l2s(new_local_avg, X64LL_SIGNED, buf));

    int64_t offset_diff = (int64_t) (skew * local_diff);
    printf("_ftsp_correct_clock: offset_diff %s ",
            l2s(offset_diff, X64LL_SIGNED, buf));
    printf(" skew %f \n", skew);
    int64_t offset_est = new_offset_avg + offset_diff;
    printf("_ftsp_correct_clock: offset_est %s ",
            l2s(offset_est, X64LL_SIGNED, buf));
    printf("_ftsp_correct_clock: new_offset_avg %s ",
            l2s(new_offset_avg, X64LL_SIGNED, buf));
    printf(" new_offset_avg %s \n", l2s(offset_diff, X64LL_SIGNED, buf));
    gtimer_timeval_t now;
    gtimer_sync_now(&now);
    int64_t offset_now = now.global - now.local;
    printf(" offset_now %s ", l2s(offset_now, X64LL_SIGNED, buf));
    printf(" now.global %s ", l2s(now.global, X64LL_SIGNED, buf));
    printf(" now.local %s \n", l2s(now.local, X64LL_SIGNED, buf));
    int64_t offset = offset_est - offset_now;
    _ftsp_last_offset = offset;
    gtimer_sync_set_global_offset(offset, 1);
}
*/

void ftsp_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length)
{
    /*
     if (ieee802154_frame[0] == FTSP_PROTOCOL_DISPATCH)
     {
     gtimer_timeval_t now;
     ieee802154_frame_t frame;
     uint8_t hdrlen = ieee802154_frame_read(ieee802154_frame, &frame,
     frame_length);
     ftsp_beacon_t *beacon = (ftsp_beacon_t *) frame.payload;
     gtimer_sync_now(&now);
     beacon->local = now.local;
     beacon->offset = _ftsp_last_offset;
     beacon->relative_rate = now.rate;
     memcpy(ieee802154_frame + hdrlen, beacon, sizeof(ftsp_beacon_t));
     }
     */
}

int is_synced(void)
{
    if ((num_entries >= ENTRY_VALID_LIMIT) || (_ftsp_root_id == _ftsp_id))
        return FTSP_OK;
    else
        return FTSP_ERR;
}

static void add_new_entry(ftsp_beacon_t *beacon, gtimer_timeval_t *toa)
{
    puts("calculate_conversion");
    char buf[66];
    free_item = -1;
    oldest_item = 0;
    age = 0;
    oldest_time = 0;

    table_entries = 0;

    time_error = (int64_t) (beacon->local + beacon->offset - toa->global);
    if (is_synced() == FTSP_OK)
    {
        printf("FTSP synced, error %s\n", l2s(time_error, X64LL_SIGNED, buf));
        if ((time_error > ENTRY_THROWOUT_LIMIT)
                || (-time_error > ENTRY_THROWOUT_LIMIT))
        {
            printf("(big)\n");
            if (++num_errors > 3)
            {
                printf("FTSP: num_errors > 3 => clear_table()\n");
                clear_table();
            }
        }
        else
        {
            printf("(small)\n");
            num_errors = 0;
        }
    }
    else
    {
        printf("FTSP not synced\n");
    }

    for (i = 0; i < MAX_ENTRIES; ++i)
    {
        age = toa->local - table[i].local_time;

        if (age >= 0x7FFFFFFFL)
            table[i].state = ENTRY_EMPTY;

        if (table[i].state == ENTRY_EMPTY)
            free_item = i;
        else
            ++table_entries;

        if (age >= oldest_time)
        {
            oldest_time = age;
            oldest_item = i;
        }
    }

    if (free_item < 0)
        free_item = oldest_item;
    else
        ++table_entries;

    table[free_item].state = ENTRY_FULL;
    table[free_item].local_time = toa->local;
    table[free_item].time_offset = beacon->local + beacon->offset - toa->local;
}

static void calculate_conversion(void)
{
    puts("calculate_conversion");
    for (i = 0; (i < MAX_ENTRIES) && (table[i].state != ENTRY_FULL); ++i)
        ;

    if (i >= MAX_ENTRIES)
        return;   // table is empty

    local_average = table[i].local_time;
    offset_average = table[i].time_offset;

    local_sum = 0;
    offset_sum = 0;

    while (++i < MAX_ENTRIES)
    {
        if (table[i].state == ENTRY_FULL)
        {
            local_sum += (long) (table[i].local_time - local_average)
                    / table_entries;
            offset_sum += (long) (table[i].time_offset - offset_average)
                    / table_entries;
        }
    }

    local_average += local_sum;
    offset_average += offset_sum;

    local_sum = offset_sum = 0;
    for (i = 0; i < MAX_ENTRIES; ++i)
    {
        if (table[i].state == ENTRY_FULL)
        {
            a = table[i].local_time - local_average;
            b = table[i].time_offset - offset_average;

            local_sum += (long long) a * a;
            offset_sum += (long long) a * b;
        }
    }

    if (local_sum != 0)
        skew = (float) offset_sum / (float) local_sum;

    num_entries = table_entries;

    gtimer_sync_set_relative_rate(skew);
    gtimer_sync_set_global_offset(offset_average,1);

    printf("FTSP conversion calculated: num_entries=%u, is_synced=%u\n",
            num_entries, is_synced());
}

void clear_table(void)
{
    for (i = 0; i < MAX_ENTRIES; ++i)
        table[i].state = ENTRY_EMPTY;

    num_entries = 0;
}

static void _ftsp_print_beacon(ftsp_beacon_t *beacon)
{
    char buf[66];
    printf("----\nbeacon: \n");
    printf("\t id: %"PRIu16"\n", beacon->id);
    printf("\t root: %"PRIu16"\n", beacon->root);
    printf("\t seq_number: %"PRIu16"\n", beacon->seq_number);
    printf("\t local: %s\n", l2s(beacon->local, X64LL_SIGNED, buf));
    printf("\t offset: %s\n", l2s(beacon->offset, X64LL_SIGNED, buf));
    printf("\t relative_rate: %f\n----\n", beacon->relative_rate);
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

