/**
 * This file subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup sys
 * @{
 * @file    clocksync_eval.c
 * @brief	Clock-sync. evaluation module implementation.
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */
#include <stdio.h>
#include <string.h>

#include "thread.h"
#include "vtimer.h"
#include "mutex.h"
#include "sixlowpan/mac.h"
#include "sixlowpan/dispatch_values.h"
#include "random.h"
#include "msg.h"
#include "transceiver.h"
#include "hwtimer.h"

#include "gtimer.h"
#include "x64toa.h"
#include "clocksync/clocksync_eval.h"

#ifdef MODULE_GTSP
#include "clocksync/gtsp.h"
#endif
#ifdef MODULE_FTSP
#include "clocksync/ftsp.h"
#endif
#ifdef MODULE_PULSESYNC
#include "clocksync/pulsesync.h"
#endif
#ifdef MODULE_CC110X_NG
#include "cc110x_ng/cc110x-interface.h" // for radio statistics
#endif

#define ENABLE_DEBUG (0)
#include "debug.h"

// paramters
#define CLOCKSYNC_EVAL_BEACON_INTERVAL (10 * 1000 * 1000) // default to 10s
#define CLOCKSYNC_EVAL_HEARTBEAT_INTERVAL (1 * 1000 * 1000) // default to 1s


#define CLOCKSYNC_EVAL_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define CLOCKSYNC_EVAL_HEARTBEAT_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define CLOCKSYNC_EVAL_CYCLIC_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define CLOCKSYNC_EVAL_BEACON_BUFFER_SIZE (64)

static void *beacon_send_thread(void *arg);
static void *cyclic_driver_thread(void *arg);

static void send_beacon(void);
static uint16_t get_transceiver_addr(void);

static uint32_t beacon_counter = 0;
static int beacon_pid = 0;
static uint32_t beacon_interval = CLOCKSYNC_EVAL_BEACON_INTERVAL;
static uint32_t heartbeat_interval =
CLOCKSYNC_EVAL_HEARTBEAT_INTERVAL;
static uint32_t beacon_interval_jitter = 5000;
static uint32_t beacon_interval_lower = 5000;
static uint16_t node_id = 1;

static bool pause_protocol = true;
static bool heartbeat_pause = false;

char clocksync_eval_beacon_stack[CLOCKSYNC_EVAL_BEACON_STACK_SIZE];
char clocksync_eval_heartbeat_stack[CLOCKSYNC_EVAL_HEARTBEAT_STACK_SIZE];
char clocksync_eval_cyclic_beacon_stack[CLOCKSYNC_EVAL_CYCLIC_BEACON_STACK_SIZE];
char clocksync_eval_beacon_buffer[CLOCKSYNC_EVAL_BEACON_BUFFER_SIZE] =
{ 0 };

mutex_t clocksync_eval_mutex;

void clocksync_eval_init(void)
{
    mutex_init(&clocksync_eval_mutex);

    beacon_pid = thread_create(clocksync_eval_beacon_stack,
    CLOCKSYNC_EVAL_BEACON_STACK_SIZE, PRIORITY_MAIN - 2, CREATE_STACKTEST,
            beacon_send_thread, NULL, "clocksync_eval_beacon");

    thread_create(clocksync_eval_cyclic_beacon_stack,
    CLOCKSYNC_EVAL_CYCLIC_BEACON_STACK_SIZE,
    PRIORITY_MAIN - 2, CREATE_STACKTEST,
            cyclic_driver_thread, NULL,
            "clocksync_eval_driver_beacon");
}

void clocksync_eval_mac_read(uint8_t *payload, uint16_t src,
        gtimer_timeval_t *gtimer_toa)
{
    mutex_lock(&clocksync_eval_mutex);
    char tl_buf[60] =
    { 0 };
    char tg_buf[60] =
    { 0 };
    DEBUG("clocksync_eval_read_trigger");
    clocksync_eval_beacon_t *beacon = (clocksync_eval_beacon_t *) payload;
    printf("#et, a: %" PRIu16 ",", src);
    printf(" c: %"PRIu32",", beacon->counter);
    printf(" tl: %s,", l2s(gtimer_toa->local, X64LL_SIGNED, tl_buf));
    printf(" tg: %s\n", l2s(gtimer_toa->global, X64LL_SIGNED, tg_buf));
    mutex_unlock(&clocksync_eval_mutex);
}

void clocksync_eval_set_beacon_interval(uint32_t lower_delay_in_ms,
        uint32_t jitter_in_ms)
{
    beacon_interval_jitter = jitter_in_ms;
    beacon_interval_lower = lower_delay_in_ms;
}

void clocksync_eval_set_heartbeat_interval(uint32_t delay_in_ms)
{
    heartbeat_interval = delay_in_ms * 1000;
}

void clocksync_eval_pause_sending(void)
{
    pause_protocol = true;
    DEBUG("clocksync_eval beacon sending off");
}

void clocksync_eval_resume_sending(void)
{
    genrand_init((uint32_t) get_transceiver_addr());

    pause_protocol = false;
    DEBUG("clocksync_eval beacon sending on");
}

void clocksync_eval_pause_heartbeat(void)
{
    heartbeat_pause = true;
    DEBUG("clocksync_eval heartbeat off");
}

void clocksync_eval_resume_heartbeat(void)
{
    node_id = get_transceiver_addr();
    heartbeat_pause = false;
    DEBUG("clocksync_eval heartbeat on");
}

static void *beacon_send_thread(void *arg)
{
    while (1)
    {
        DEBUG("_clocksync_eval_beacon_send_thread: sleeping\n");
        thread_sleep();
        DEBUG("_clocksync_eval_beacon_send_thread: woke up\n");
        if (!pause_protocol)
        {
            mutex_lock(&clocksync_eval_mutex);
            send_beacon();
            if (!heartbeat_pause)
            {
                char tl_buf[60] =
                { 0 };
                char tg_buf[60] =
                { 0 };
#ifdef GTIMER_USE_VTIMER
                timex_t now;
                vtimer_now(&now);
                uint64_t local;
                local = timex_uint64(now);
                printf("#eh,");
                printf(" a: %"PRIu16 ",", node_id);
                printf(" gl: %s,", l2s(local, X64LL_SIGNED, tl_buf));
                printf(" gg: %s,", l2s(local+1, X64LL_SIGNED, tg_buf));
                // display rate as integer; newlib's printf and floats do no play nice together
                printf(" gr: 100");
#else
                gtimer_timeval_t now;
                gtimer_sync_now(&now);
                // about ~7800us - 8000us on lpc2387
                printf("#eh,");
                printf(" a: %"PRIu16 ",", node_id);
                printf(" gl: %s,", l2s(now.local, X64LL_SIGNED, tl_buf));
                printf(" gg: %s,", l2s(now.global, X64LL_SIGNED, tg_buf));
                // display rate as integer; newlib's printf and floats do no play nice together
                printf(" gr: %d", (int) (now.rate * 1000000000));
#endif
#ifdef MODULE_CC110X_NG
                printf(", pi: %"PRIu32, cc110x_statistic.packets_in);
                printf(", po: %"PRIu32, cc110x_statistic.raw_packets_out);
                printf(", cr: %"PRIu32, cc110x_statistic.packets_in_crc_fail);
                printf(", s: %"PRIu32, cc110x_statistic.packets_in_while_tx);
#endif
                printf("\n");

            }
            beacon_interval =
                    (beacon_interval_lower
                            + genrand_uint32()
                                    % beacon_interval_jitter)
                            * 1000;
            DEBUG("_clocksync_eval_beacon_send_thread: new beacon interval: %"PRIu32 "\n",
                    beacon_interval);
            mutex_unlock(&clocksync_eval_mutex);
        }
    }
    return NULL;
}

static void *cyclic_driver_thread(void *arg)
{
    while (1)
    {
        vtimer_usleep(beacon_interval);
        if (!pause_protocol)
        {
            DEBUG(
                    "_clocksync_eval_cyclic_driver_thread: waking sender thread up");
            thread_wakeup(beacon_pid);
        }
    }
    return NULL;
}

static void send_beacon(void)
{
    DEBUG("_clocksync_eval_send_beacon\n");
    gtimer_timeval_t now;
    clocksync_eval_beacon_t *clocksync_eval_beacon =
            (clocksync_eval_beacon_t *) clocksync_eval_beacon_buffer;
    gtimer_sync_now(&now);
    memset(clocksync_eval_beacon_buffer, 0, sizeof(clocksync_eval_beacon_t));
    clocksync_eval_beacon->dispatch_marker = CLOCKSYNC_EVAL_PROTOCOL_DISPATCH;
    clocksync_eval_beacon->counter = beacon_counter;
    sixlowpan_mac_send_ieee802154_frame(0, NULL, 8,
            clocksync_eval_beacon_buffer, sizeof(clocksync_eval_beacon_t), 1);
    beacon_counter++;
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
    mesg.content.ptr = (char *) &tcmd;
    mesg.type = GET_ADDRESS;

    msg_send_receive(&mesg, &mesg, transceiver_pid);
    return a;
}
