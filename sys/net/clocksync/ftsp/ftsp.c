/**
 * Copyright (C) 2014  Philipp Rosenkranz, Daniel Jentsch.
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
#include "sixlowpan/dispatch_values.h"
#include "random.h"
#include "transceiver.h"

#include "clocksync/ftsp.h"
#include "gtimer.h"
//#include "x64toa.h"

#ifdef MODULE_CC110X_NG
#define FTSP_CALIBRATION_OFFSET ((uint32_t) 2300)

#elif MODULE_NATIVENET
#define FTSP_CALIBRATION_OFFSET ((uint32_t) 1500)

#else
#warning "Transceiver not supported by FTSP!"
#define FTSP_CALIBRATION_OFFSET ((uint32_t) 0) // unknown delay
#endif

#define ENABLE_DEBUG (0)
#if ENABLE_DEBUG
#define DEBUG_ENABLED
#endif
#include <debug.h>

// Protocol parameters
#define FTSP_PREFERRED_ROOT (1) // node with id==1 will become root
#define FTSP_BEACON_INTERVAL (5 * 1000 * 1000) // 5 sec in us
#define FTSP_MAX_SYNC_POINT_AGE (20 * 60 * 1000 * 1000) // 20 min in us

#define FTSP_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define FTSP_CYCLIC_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define FTSP_BEACON_BUFFER_SIZE (64)

// threads
static void *beacon_thread(void *arg);
static void *cyclic_driver_thread(void *arg);

static void send_beacon(void);

static int beacon_pid = 0;
static int cyclic_driver_pid = 0;
static uint32_t beacon_interval = FTSP_BEACON_INTERVAL;
static uint32_t transmission_delay = FTSP_CALIBRATION_OFFSET;
static bool pause_protocol = true;

static uint16_t root_id = UINT16_MAX;
static uint16_t node_id = 0;

char ftsp_beacon_stack[FTSP_BEACON_STACK_SIZE];
char ftsp_cyclic_stack[FTSP_CYCLIC_STACK_SIZE];
char ftsp_beacon_buffer[FTSP_BEACON_BUFFER_SIZE] =
{ 0 };

static int8_t i, free_item;
static uint64_t local_average;
static uint8_t table_entries, heart_beats, num_errors, seq_num;
static int64_t offset_average;
static int64_t offset;
static float skew;
static table_item table[FTSP_MAX_ENTRIES];

static void clear_table(void);
static void add_new_entry(ftsp_beacon_t *beacon, gtimer_timeval_t *toa);
static void linear_regression(void);
static uint16_t get_transceiver_addr(void);
/*
 #ifdef DEBUG_ENABLED
 static void print_beacon(ftsp_beacon_t *beacon);
 #endif
 */

mutex_t ftsp_mutex;

void ftsp_init(void)
{
    mutex_init(&ftsp_mutex);

    skew = 0.0;
    local_average = 0;
    offset_average = 0;
    clear_table();
    heart_beats = 0;
    num_errors = 0;

    beacon_pid = thread_create(ftsp_beacon_stack, FTSP_BEACON_STACK_SIZE,
    PRIORITY_MAIN - 2, CREATE_STACKTEST, beacon_thread, NULL, "ftsp_beacon");

    puts("FTSP initialized");
}

static void *beacon_thread(void *arg)
{
    (void) arg;
    while (1)
    {
        thread_sleep();
        DEBUG("_ftsp_beacon_thread locking mutex\n");
        mutex_lock(&ftsp_mutex);
        memset(ftsp_beacon_buffer, 0, sizeof(ftsp_beacon_t));
        if (!pause_protocol)
        {
            send_beacon();
        }
        mutex_unlock(&ftsp_mutex);
        DEBUG("_ftsp_beacon_thread: mutex unlocked\n");
    }
    return NULL;
}

static void *cyclic_driver_thread(void *arg)
{
    (void) arg;
    genrand_init((uint32_t) node_id);
    uint32_t random_wait = (100 + genrand_uint32() % FTSP_BEACON_INTERVAL);
    vtimer_usleep(random_wait);

    while (1)
    {
        vtimer_usleep(beacon_interval);
        if (!pause_protocol)
        {
            DEBUG("_ftsp_cyclic_driver_thread: waking sending thread up");
            thread_wakeup(beacon_pid);
        }
    }
    return NULL;
}

static void send_beacon(void)
{
    DEBUG("_ftsp_send_beacon\n");
    gtimer_timeval_t now;
    ftsp_beacon_t *ftsp_beacon = (ftsp_beacon_t *) ftsp_beacon_buffer;
    gtimer_sync_now(&now);
    if ((root_id != 0xFFFF))
    {
        if (root_id == node_id)
        {
            if ((long) (now.local - local_average) >= 0x20000000)
            {
                local_average = now.local;
                offset_average = now.global - now.local;
            }
        }

        //      else
        //      {
        //          if (heart_beats >= ROOT_TIMEOUT)
        //          {
        //              PRINTF("FTSP: root timeout, declaring myself the root\n");
        //              heart_beats = 0;
        //              _ftsp_root_id = _ftsp_id;
        //              ++seq_num;
        //          }
        //      }

        if ((table_entries < FTSP_ENTRY_SEND_LIMIT) && (root_id != node_id))
        {
            ++heart_beats;
        }
        else
        {
            gtimer_sync_now(&now);
            ftsp_beacon->dispatch_marker = FTSP_PROTOCOL_DISPATCH;
            ftsp_beacon->id = node_id;
            ftsp_beacon->global = now.global;
            ftsp_beacon->root = root_id;
            ftsp_beacon->seq_number = seq_num;
#ifdef DEBUG_ENABLED
            /*
             print_beacon(ftsp_beacon);
             */
#endif
            sixlowpan_mac_send_ieee802154_frame(0, NULL, 8, ftsp_beacon_buffer,
                    sizeof(ftsp_beacon_t), 1);

            ++heart_beats;

            if (root_id == node_id)
                ++seq_num;
        }
    }
}

void ftsp_mac_read(uint8_t *frame_payload, uint16_t src, gtimer_timeval_t *toa)
{
    (void) src;
    DEBUG("ftsp_mac_read");
    mutex_lock(&ftsp_mutex);
    if (pause_protocol)
    {
        mutex_unlock(&ftsp_mutex);
        return;
    }

    ftsp_beacon_t *beacon = (ftsp_beacon_t *) frame_payload;

    if ((beacon->root < root_id)
            && !((heart_beats < FTSP_IGNORE_ROOT_MSG) && (root_id == node_id)))
    {
        root_id = beacon->root;
        seq_num = beacon->seq_number;
    }
    else
    {
        if ((root_id == beacon->root)
                && ((int8_t) (beacon->seq_number - seq_num) > 0))
        {
            seq_num = beacon->seq_number;
        }
        else
        {
            DEBUG(
                    "not (beacon->root < _ftsp_root_id) [...] and not (_ftsp_root_id == beacon->root)");
            mutex_unlock(&ftsp_mutex);
            return;
        }
    }

    if (root_id < node_id)
        heart_beats = 0;

    add_new_entry(beacon, toa);
    linear_regression();

    gtimer_sync_set_global_offset(offset);
    gtimer_sync_set_relative_rate(skew);

    mutex_unlock(&ftsp_mutex);
}

void ftsp_set_beacon_delay(uint32_t delay_in_sec)
{
    beacon_interval = delay_in_sec * 1000 * 1000;
}

void ftsp_set_prop_time(uint32_t us)
{
    transmission_delay = us;
}

void ftsp_pause(void)
{
    pause_protocol = true;
    DEBUG("FTSP disabled");
}

void ftsp_resume(void)
{
    node_id = get_transceiver_addr();
    if (node_id == FTSP_PREFERRED_ROOT)
    {
        //_ftsp_root_timeout = 0;
        root_id = node_id;
    }
    else
    {
        root_id = 0xFFFF;
    }
    skew = 0.0;
    local_average = 0;
    offset_average = 0;
    clear_table();
    heart_beats = 0;
    num_errors = 0;

    pause_protocol = false;
    if (cyclic_driver_pid == 0)
    {
        cyclic_driver_pid = thread_create(ftsp_cyclic_stack,
        FTSP_CYCLIC_STACK_SIZE,
        PRIORITY_MAIN - 2,
        CREATE_STACKTEST, cyclic_driver_thread, NULL, "ftsp_cyclic_driver");
    }

    DEBUG("FTSP enabled");
}

void ftsp_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length)
{

    if (ieee802154_frame[0] == FTSP_PROTOCOL_DISPATCH)
    {
        gtimer_timeval_t now;
        ieee802154_frame_t frame;
        uint8_t hdrlen = ieee802154_frame_read(ieee802154_frame, &frame,
                frame_length);
        ftsp_beacon_t *beacon = (ftsp_beacon_t *) frame.payload;
        gtimer_sync_now(&now);
        beacon->global = now.global;
        memcpy(ieee802154_frame + hdrlen, beacon, sizeof(ftsp_beacon_t));
    }

}

int ftsp_is_synced(void)
{
    if ((table_entries >= FTSP_ENTRY_VALID_LIMIT) || (root_id == node_id))
        return FTSP_OK;
    else
        return FTSP_ERR;
}

static void linear_regression(void)
{
    DEBUG("calculate_conversion");
    int64_t sum_local = 0, sum_global = 0, covariance = 0, sum_global_squared = 0;

    if (table_entries == 0)
        return;

    for (i = 0; i < FTSP_MAX_ENTRIES; i++)
    {
        if (table[i].state == FTSP_ENTRY_FULL)
        {
            sum_local += table[i].local;
            sum_global += table[i].global;
            sum_global_squared += table[i].global * table[i].global;
            covariance += table[i].local * table[i].global;
        }
    }

    skew = (covariance - (sum_global * sum_local) / table_entries);
    skew /= (sum_global_squared - ((sum_global * sum_local) / table_entries));
    skew -= 1;

    offset = (sum_local - skew * sum_global) / table_entries;

    DEBUG("FTSP conversion calculated: num_entries=%u, is_synced=%u\n",
            num_entries, ftsp_is_synced());
}

//XXX: This function not only adds an entry but also removes old entries.
// This is the wrong place and the wrong time.
static void add_new_entry(ftsp_beacon_t *beacon, gtimer_timeval_t *toa)
{
    free_item = -1;
    uint8_t oldest_item = 0;
    uint64_t oldest_time = UINT64_MAX;
    uint64_t limit_age = toa->local - FTSP_MAX_SYNC_POINT_AGE;
    // surround errors at the beginning
    if (toa->local < FTSP_MAX_SYNC_POINT_AGE)
      limit_age = 0;

    table_entries = 0;

    int64_t time_error = (int64_t) (beacon->global - toa->global);
    if (ftsp_is_synced() == FTSP_OK)
    {
        if ((time_error > FTSP_ENTRY_THROWOUT_LIMIT)
                || (-time_error > FTSP_ENTRY_THROWOUT_LIMIT))
        {
            DEBUG("(big error, new root?)\n");
            if (++num_errors > 3)
            {
                puts("FTSP: num_errors > 3 => clear_table()\n");
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
        DEBUG("FTSP not synced\n");
    }



    for (i = 0; i < FTSP_MAX_ENTRIES; ++i)
    {
        if (table[i].local < limit_age)
            table[i].state = FTSP_ENTRY_EMPTY;

        if (table[i].state == FTSP_ENTRY_EMPTY)
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

    table[free_item].state = FTSP_ENTRY_FULL;
    table[free_item].local = toa->local;
    table[free_item].global = beacon->global;
}

static void clear_table(void)
{
    for (i = 0; i < FTSP_MAX_ENTRIES; ++i)
        table[i].state = FTSP_ENTRY_EMPTY;

    table_entries = 0;
}

#ifdef DEBUG_ENABLED
/*
 static void print_beacon(ftsp_beacon_t *beacon)
 {
 char buf[66];
 printf("----\nbeacon: \n");
 printf("\t id: %"PRIu16"\n", beacon->id);
 printf("\t root: %"PRIu16"\n", beacon->root);
 printf("\t seq_number: %"PRIu16"\n", beacon->seq_number);
 printf("\t global: %s\n", l2s(beacon->global, X64LL_SIGNED, buf));
 }
 */
#endif /* DEBUG_ENABLED */

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

