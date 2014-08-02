/**
 * Copyright (C) 2014  Philipp Rosenkranz, Daniel Jentsch.
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup pulsesync
 * @{
 * @file    pulsesync.c
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
#include "random.h"
#include "transceiver.h"

#include "clocksync/pulsesync.h"
#include "sixlowpan/dispatch_values.h"
#include "gtimer.h"

#include "x64toa.h"

#ifdef MODULE_CC110X_NG
#define PULSESYNC_CALIBRATION_OFFSET ((uint32_t) 2300)

#elif MODULE_NATIVENET
#define PULSESYNC_CALIBRATION_OFFSET ((uint32_t) 1500)

#else
#warning "Transceiver not supported by PulseSync!"
#define PULSESYNC_CALIBRATION_OFFSET ((uint32_t) 0) // unknown delay
#endif

#define ENABLE_DEBUG (0)
#if ENABLE_DEBUG
#define DEBUG_ENABLED
#endif
#include <debug.h>

// Protocol parameters
#define PULSESYNC_PREFERRED_ROOT (1) // node with id==1 will become root
#define PULSESYNC_BEACON_INTERVAL (10 * 1000 * 1000) // in us
#define PULSESYNC_MAX_SYNC_POINT_AGE (20 * 60 * 1000 * 1000) // max age for reg. table entry
#define PULSESYNC_RATE_CALC_THRESHOLD (3) // at least 3 entries needed before rate is calculated
#define PULSESYNC_MAX_ENTRIES (8) // number of entries in the regression table
#define PULSESYNC_ENTRY_VALID_LIMIT (4) // number of entries to become synchronized
#define PULSESYNC_ENTRY_SEND_LIMIT (3) // number of entries to send sync messages
#define PULSESYNC_ROOT_TIMEOUT (3) // time to declare itself the root if no msg was received (in sync periods)
#define PULSESYNC_IGNORE_ROOT_MSG (4) // after becoming the root ignore other roots messages (in send period)
#define PULSESYNC_ENTRY_THROWOUT_LIMIT (300) // if time sync error is bigger than this clear the table
// In order to reduce the likelihood of a packet collision during the
// propagation of a beacon every node has to wait a random time before
// sending the beacon.
#define PULSESYNC_BEACON_PROPAGATION_DELAY (10*1000)

// easy to read status flags
#define PULSESYNC_OK (1)
#define PULSESYNC_ERR (0)
#define PULSESYNC_ENTRY_EMPTY (0)
#define PULSESYNC_ENTRY_FULL (1)

#define PULSESYNC_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define PULSESYNC_BEACON_BUFFER_SIZE (64)

// thread
static void *beacon_thread(void *arg);

static void send_beacon(void);
static void linear_regression(void);
static void add_new_entry(pulsesync_beacon_t *beacon, gtimer_timeval_t *toa);
static void clear_table(void);
static uint16_t get_transceiver_addr(void);

static int beacon_thread_id = 0;

// protocol state variables
static vtimer_t beacon_timer;
static uint32_t beacon_interval = PULSESYNC_BEACON_INTERVAL;
static uint32_t transmission_delay = PULSESYNC_CALIBRATION_OFFSET;
static bool pause_protocol = true;
static uint16_t root_id = PULSESYNC_PREFERRED_ROOT;
static uint16_t node_id = 0, seq_num = 0;
static uint8_t table_entries, heart_beats, num_errors;
static int64_t offset;
static float rate = 1.0;
static pulsesync_table_item_t table[PULSESYNC_MAX_ENTRIES];

static char beacon_stack[PULSESYNC_BEACON_STACK_SIZE];
static char beacon_buffer[PULSESYNC_BEACON_BUFFER_SIZE] =
{ 0 };

mutex_t pulsesync_mutex;

void pulsesync_init(void)
{
    mutex_init(&pulsesync_mutex);

    rate = 1.0;
    clear_table();
    heart_beats = 0;
    num_errors = 0;
    table_entries = 0;
    offset = 0;
    beacon_thread_id = 0;

    puts("PulseSync initialized");
}

static void *beacon_thread(void *arg)
{
    beacon_thread_id = thread_getpid();
    while (1)
    {
        if (node_id == PULSESYNC_PREFERRED_ROOT)
        {
            puts("i am root");
            vtimer_set_wakeup(&beacon_timer, timex_from_uint64(beacon_interval),
                    beacon_thread_id);
        }
        thread_sleep();
        puts("beacon_thread locking mutex");
        mutex_lock(&pulsesync_mutex);
        memset(beacon_buffer, 0, sizeof(pulsesync_beacon_t));
        if (!pause_protocol)
        {
            send_beacon();
        }
        mutex_unlock(&pulsesync_mutex);
        puts("beacon_thread: mutex unlocked");
    }
    return NULL;
}

static void send_beacon(void)
{
    //puts("pulsesync: send_beacon\n");
    if (node_id == PULSESYNC_PREFERRED_ROOT)
        seq_num++;
    printf("sending beacon with seq_num: %"PRIu16 "\n", seq_num);

    gtimer_timeval_t now;
    pulsesync_beacon_t *pulsesync_beacon = (pulsesync_beacon_t *) beacon_buffer;

    gtimer_sync_now(&now);
    pulsesync_beacon->dispatch_marker = PULSESYNC_PROTOCOL_DISPATCH;
    pulsesync_beacon->id = node_id;
    // TODO: do we need to add the transmission delay to the offset?
    pulsesync_beacon->id = node_id;
    pulsesync_beacon->global = now.global + transmission_delay;
    pulsesync_beacon->root = root_id;
    pulsesync_beacon->seq_number = seq_num;

    sixlowpan_mac_send_ieee802154_frame(0, NULL, 8, beacon_buffer,
            sizeof(pulsesync_beacon_t), 1);
}

void pulsesync_mac_read(uint8_t *frame_payload, uint16_t src,
        gtimer_timeval_t *toa)
{
    DEBUG("pulsesync_mac_read: pulsesync_mac_read");
    mutex_lock(&pulsesync_mutex);
    pulsesync_beacon_t gtsp_beacon;

    // copy beacon into local buffer
    memcpy(&gtsp_beacon, frame_payload, sizeof(gtsp_beacon_t));
    pulsesync_beacon_t *beacon = &gtsp_beacon;

    if (pause_protocol || node_id == PULSESYNC_PREFERRED_ROOT)
    {
        mutex_unlock(&pulsesync_mutex);
        return;
    }
    char buf[60];
    printf("pulsesync_mac_read ");
    printf("beacon->dispatch_marker: %"PRIu8 " ", beacon->dispatch_marker);
    printf("beacon->id: %"PRIu16 " ", beacon->id);
    printf("beacon->root: %"PRIu16 " ", beacon->root);
    printf("beacon->seq_number: %"PRIu16 " ", beacon->seq_number);
    printf("beacon->global: %s\n", l2s(beacon->global, X64LL_SIGNED, buf));

//    if ((beacon->root < root_id)
//            && !((heart_beats < PULSESYNC_IGNORE_ROOT_MSG)
//                    && (root_id == node_id)))
//    {
//        root_id = beacon->root;
//        seq_num = beacon->seq_number;
//    }
//    else
//    {

//        if ((root_id == beacon->root)
//                && (
//                        (beacon->seq_number > seq_num && beacon->seq_number - seq_num < UINT16_MAX - 100)
//                        ||
//                        (beacon->seq_number < seq_num && seq_num - beacon->seq_number > UINT16_MAX - 100)))
//        {
//            seq_num = beacon->seq_number;
//        }
//        else
//        {
//            DEBUG(
//                    "not (beacon->root < pulsesync_root_id) [...] and not (pulsesync_root_id == beacon->root)");
//            mutex_unlock(&pulsesync_mutex);
//            return;
//        }

//    }

    if ((root_id == beacon->root) && (beacon->seq_number > seq_num))
    {
        seq_num = beacon->seq_number;
    }
    else
    {
        mutex_unlock(&pulsesync_mutex);
        return;
    }

//    if (root_id < node_id)
//        heart_beats = 0;

    add_new_entry(beacon, toa);
    linear_regression();
    int64_t est_global = offset + ((int64_t) toa->local) * (rate);
    int64_t offset_global = est_global - (int64_t) toa->global;
    /*
     if (offset > 10 * 1000 * 1000 || offset < -10 * 1000 * 1000)
     {
     char buf[60];
     printf("est_global: %s ", l2s(est_global, X64LL_SIGNED, buf));
     printf("offset: %s ", l2s(offset, X64LL_SIGNED, buf));
     printf("toa->local: %s ", l2s(toa->local, X64LL_SIGNED, buf));
     printf("toa->global: %s ", l2s(toa->global, X64LL_SIGNED, buf));
     printf("rate: %f ", rate);
     printf("offset_global: %s ", l2s(offset, X64LL_SIGNED, buf));
     printf("table_entries: %"PRIu8 " ", table_entries);
     printf("culprit: %"PRIu16 " ", src);
     printf("culprit gl: %s\n", l2s(beacon->global, X64LL_SIGNED, buf));
     }
     */

    offset = offset_global;

    gtimer_sync_set_global_offset(offset);
    if (table_entries >= PULSESYNC_RATE_CALC_THRESHOLD)
    {
        gtimer_sync_set_relative_rate(rate - 1.0);
    }

    mutex_unlock(&pulsesync_mutex);

    timex_t wait = timex_from_uint64(
            1000 + (genrand_uint32() % PULSESYNC_BEACON_PROPAGATION_DELAY));

    vtimer_set_wakeup(&beacon_timer, wait, beacon_thread_id);
}

void pulsesync_set_beacon_delay(uint32_t delay_in_sec)
{
    beacon_interval = delay_in_sec * 1000 * 1000;
}

void pulsesync_set_prop_time(uint32_t us)
{
    transmission_delay = us;
}

void pulsesync_pause(void)
{
    DEBUG("pulsesync: paused");
    pause_protocol = true;
}

void pulsesync_resume(void)
{
    DEBUG("pulsesync: resume");
    node_id = get_transceiver_addr();
    root_id = PULSESYNC_PREFERRED_ROOT;
//    if (node_id == PULSESYNC_PREFERRED_ROOT)
//    {
//        root_id = node_id;
//    }
//    else
//    {
//        root_id = 0xFFFF;
//    }
    clear_table();
    heart_beats = 0;
    num_errors = 0;

    pause_protocol = false;

    if (beacon_thread_id == 0)
    {
        beacon_thread_id = thread_create(beacon_stack,
        PULSESYNC_BEACON_STACK_SIZE,
        PRIORITY_MAIN - 2, CREATE_STACKTEST, beacon_thread, NULL,
                "pulsesync_beacon");
    }
}

void pulsesync_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length)
{
    if (ieee802154_frame[0] == PULSESYNC_PROTOCOL_DISPATCH)
    {
        gtimer_timeval_t now;
        ieee802154_frame_t frame;
        uint8_t hdrlen = ieee802154_frame_read(ieee802154_frame, &frame,
                frame_length);
        pulsesync_beacon_t *beacon = (pulsesync_beacon_t *) frame.payload;
        gtimer_sync_now(&now);
        beacon->global = now.global + transmission_delay;
        memcpy(ieee802154_frame + hdrlen, beacon, sizeof(pulsesync_beacon_t));
    }
}

int pulsesync_is_synced(void)
{
    if ((table_entries >= PULSESYNC_ENTRY_VALID_LIMIT) || (root_id == node_id))
        return PULSESYNC_OK;
    else
        return PULSESYNC_ERR;
}

static void linear_regression(void)
{
    DEBUG("pulsesync: linear_regression");
    int64_t sum_local = 0, sum_global = 0, covariance = 0,
            sum_local_squared = 0;

    if (table_entries == 0)
        return;

    for (uint8_t i = 0; i < PULSESYNC_MAX_ENTRIES; i++)
    {
        if (table[i].state == PULSESYNC_ENTRY_FULL)
        {
            sum_local += table[i].local;
            sum_global += table[i].global;
            sum_local_squared += table[i].local * table[i].local;
            covariance += table[i].local * table[i].global;
        }
    }
    if (table_entries > 1)
    {
        rate = (covariance - (sum_local * sum_global) / table_entries);
        rate /= (sum_local_squared - ((sum_local * sum_local) / table_entries));
    }
    else
    {
        rate = 1.0;
    }
    offset = (sum_global - rate * sum_local) / table_entries;

    DEBUG("pulsesync linear_regression calculated: table_entries=%u, is_synced=%u\n",
            table_entries, pulsesync_is_synced());
}

//XXX: This function not only adds an entry but also removes old entries.
static void add_new_entry(pulsesync_beacon_t *beacon, gtimer_timeval_t *toa)
{
    int8_t free_item = -1;
    uint8_t oldest_item = 0;
    uint64_t oldest_time = UINT64_MAX;
    uint64_t limit_age = toa->local - PULSESYNC_MAX_SYNC_POINT_AGE;

    // check for unsigned wrap around
    if (toa->local < PULSESYNC_MAX_SYNC_POINT_AGE)
        limit_age = 0;

    table_entries = 0;

    int64_t time_error = (int64_t) (beacon->global - toa->global);
    if (pulsesync_is_synced() == PULSESYNC_OK)
    {
        if ((time_error > PULSESYNC_ENTRY_THROWOUT_LIMIT)
                || (-time_error > PULSESYNC_ENTRY_THROWOUT_LIMIT))
        {
            DEBUG("pulsesync: error large; new root elected?\n");
            if (++num_errors > 3)
            {
                puts("pulsesync: number of errors to high clearing table\n");
                clear_table();
            }
        }
        else
        {
            num_errors = 0;
        }
    }
    else
    {
        DEBUG("pulsesync: not synced (yet)\n");
    }
    int del_because_old = 0;
    for (uint8_t i = 0; i < PULSESYNC_MAX_ENTRIES; ++i)
    {
        if (table[i].local < limit_age)
        {
            table[i].state = PULSESYNC_ENTRY_EMPTY;
            del_because_old++;
        }

        if (table[i].state == PULSESYNC_ENTRY_EMPTY)
            free_item = i;
        else
            ++table_entries;

        if (oldest_time > table[i].local)
        {
            oldest_time = table[i].local;
            oldest_item = i;
        }
    }

    if (free_item < 0)
        free_item = oldest_item;
    else
        ++table_entries;

    table[free_item].state = PULSESYNC_ENTRY_FULL;
    table[free_item].local = toa->local;
    table[free_item].global = beacon->global;

    printf("free_item: %"PRId8 " ", free_item);
    printf("oldest_item: %"PRIu8 " ", oldest_item);
    printf("table_entries: %"PRIu8 " ", table_entries);
    printf("del_because_old: %d\n", del_because_old);

}

static void clear_table(void)
{
    for (int8_t i = 0; i < PULSESYNC_MAX_ENTRIES; ++i)
        table[i].state = PULSESYNC_ENTRY_EMPTY;

    table_entries = 0;
}

static uint16_t get_transceiver_addr(void)
{
    msg_t mesg;
    transceiver_command_t tcmd;
    radio_address_t a;

    if (transceiver_pid < 0)
    {
        puts("Transceiver not initialized");
        return 1;
    }

    tcmd.transceivers = TRANSCEIVER_DEFAULT;
    tcmd.data = &a;
    mesg.content.ptr = (char *) &tcmd;
    mesg.type = GET_ADDRESS;

    msg_send_receive(&mesg, &mesg, transceiver_pid);
    return a;
}

