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
#include "random.h"
#include "msg.h"
#include "transceiver.h"
#include "hwtimer.h"

#include "gtimer.h"
#include "x64toa.h"
#include "nalp_protocols.h"
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

#ifdef MODULE_CC110X_NG
#include "cc110x_ng.h"
#define _TC_TYPE            TRANSCEIVER_CC1100

#elif MODULE_NATIVENET
#include "nativenet.h"
#define _TC_TYPE            TRANSCEIVER_NATIVE
#endif

#define ENABLE_DEBUG (0)
#include "debug.h"

#define CLOCKSYNC_EVAL_BEACON_INTERVAL (10 * 1000 * 1000) // default to 10s
#define CLOCKSYNC_EVAL_HEARTBEAT_INTERVAL (1 * 1000 * 1000) // default to 1s
#define CLOCKSYNC_EVAL_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define CLOCKSYNC_EVAL_HEARTBEAT_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define CLOCKSYNC_EVAL_CYCLIC_BEACON_STACK_SIZE (KERNEL_CONF_STACKSIZE_PRINTF_FLOAT)
#define CLOCKSYNC_EVAL_BEACON_BUFFER_SIZE (64)

static void _clocksync_eval_beacon_send_thread(void);
static void _clocksync_eval_cyclic_driver_thread_beacon(void);
static void _clocksync_eval_send_beacon(void);
static uint16_t _clocksync_eval_get_trans_addr(void);

static uint32_t _clocksync_eval_beacon_counter = 0;
static int _clocksync_eval_beacon_pid = 0;
static uint32_t _clocksync_eval_beacon_interval = CLOCKSYNC_EVAL_BEACON_INTERVAL;
static uint32_t _clocksync_eval_heartbeat_interval =
CLOCKSYNC_EVAL_HEARTBEAT_INTERVAL;
static uint32_t _clocksync_eval_beacon_interval_jitter = 5000;
static uint32_t _clocksync_eval_beacon_interval_lower = 5000;
static uint16_t _clocksynce_eval_transceiver_addr = 1;

static bool _clocksync_eval_beacon_pause = true;
static bool _clocksync_eval_heartbeat_pause = false;

char clocksync_eval_beacon_stack[CLOCKSYNC_EVAL_BEACON_STACK_SIZE];
char clocksync_eval_heartbeat_stack[CLOCKSYNC_EVAL_HEARTBEAT_STACK_SIZE];
char clocksync_eval_cyclic_beacon_stack[CLOCKSYNC_EVAL_CYCLIC_BEACON_STACK_SIZE];
char clocksync_eval_beacon_buffer[CLOCKSYNC_EVAL_BEACON_BUFFER_SIZE] =
{ 0 };

mutex_t clocksync_eval_mutex;

void clocksync_eval_init(void)
{
	mutex_init(&clocksync_eval_mutex);

	_clocksync_eval_beacon_pid = thread_create(clocksync_eval_beacon_stack,
	CLOCKSYNC_EVAL_BEACON_STACK_SIZE, PRIORITY_MAIN - 2, CREATE_STACKTEST,
			_clocksync_eval_beacon_send_thread, "clocksync_eval_beacon");

	thread_create(clocksync_eval_cyclic_beacon_stack,
	CLOCKSYNC_EVAL_CYCLIC_BEACON_STACK_SIZE,
	PRIORITY_MAIN - 2, CREATE_STACKTEST,
			_clocksync_eval_cyclic_driver_thread_beacon,
			"clocksync_eval_driver_beacon");
}

void clocksync_eval_read_trigger(uint8_t *payload, uint16_t src,
		gtimer_timeval_t *gtimer_toa)
{
	mutex_lock(&clocksync_eval_mutex);
	DEBUG("clocksync_eval_read_trigger");
	clocksync_eval_beacon_t *beacon = (clocksync_eval_beacon_t *) payload;
	printf("#et, a: %" PRIu16 ",", src);
	printf(" c: %"PRIu32",", beacon->counter);
	printf(" tl: %s,", l2s(gtimer_toa->local, X64LL_SIGNED));
	printf(" tg: %s\n", l2s(gtimer_toa->global, X64LL_SIGNED));
	mutex_unlock(&clocksync_eval_mutex);
}

void clocksync_eval_set_beacon_interval(uint32_t lower_delay_in_ms,
		uint32_t jitter_in_ms)
{
	_clocksync_eval_beacon_interval_jitter = jitter_in_ms;
	_clocksync_eval_beacon_interval_lower = lower_delay_in_ms;
}

void clocksync_eval_set_heartbeat_interval(uint32_t delay_in_ms)
{
	_clocksync_eval_heartbeat_interval = delay_in_ms * 1000;
}

void clocksync_eval_pause_sending(void)
{
	_clocksync_eval_beacon_pause = true;
	DEBUG("clocksync_eval beacon sending off");
}

void clocksync_eval_resume_sending(void)
{
	genrand_init((uint32_t) _clocksync_eval_get_trans_addr());

	_clocksync_eval_beacon_pause = false;
	DEBUG("clocksync_eval beacon sending on");
}

void clocksync_eval_pause_heartbeat(void)
{
	_clocksync_eval_heartbeat_pause = true;
	DEBUG("clocksync_eval heartbeat off");
}

void clocksync_eval_resume_heartbeat(void)
{
	_clocksynce_eval_transceiver_addr = _clocksync_eval_get_trans_addr();
	_clocksync_eval_heartbeat_pause = false;
	DEBUG("clocksync_eval heartbeat on");
}

static void _clocksync_eval_beacon_send_thread(void)
{
	while (1)
	{
		DEBUG("_clocksync_eval_beacon_send_thread: sleeping\n");
		thread_sleep();
		DEBUG("_clocksync_eval_beacon_send_thread: woke up\n");
		if (!_clocksync_eval_beacon_pause)
		{
			mutex_lock(&clocksync_eval_mutex);
			if (!_clocksync_eval_heartbeat_pause)
			{
				gtimer_timeval_t now;
				gtimer_sync_now(&now);
				// about ~7800us - 8000us on lpc2387
				printf("#eh,");
				printf(" a: %"PRIu16 ",", _clocksynce_eval_transceiver_addr);
				printf(" c: %"PRIu32 ",", _clocksync_eval_beacon_counter);
				printf(" gl: %s,", l2s(now.local, X64LL_SIGNED));
				printf(" gg: %s,", l2s(now.global, X64LL_SIGNED));
				if(now.rate > 1.0) puts("bigger");
				if(now.rate < -1.0) puts("smaller");
				printf(" gr: %f", now.rate);
#ifdef MODULE_CC110X_NG
				printf(", pi: %"PRIu32, cc110x_statistic.packets_in);
				printf(", po: %"PRIu32, cc110x_statistic.raw_packets_out);
				printf(", cr: %"PRIu32, cc110x_statistic.packets_in_crc_fail);
				printf(", s: %"PRIu32, cc110x_statistic.packets_in_while_tx);
#endif
				printf("\n");

			}
			_clocksync_eval_send_beacon();
			_clocksync_eval_beacon_interval =
					(_clocksync_eval_beacon_interval_lower
							+ genrand_uint32()
									% _clocksync_eval_beacon_interval_jitter)
							* 1000;
			DEBUG("_clocksync_eval_beacon_send_thread: new beacon interval: %"PRIu32 "\n",
					_clocksync_eval_beacon_interval);
			mutex_unlock(&clocksync_eval_mutex);
		}
	}
}


static void _clocksync_eval_cyclic_driver_thread_beacon(void)
{
	while (1)
	{
		vtimer_usleep(_clocksync_eval_beacon_interval);
		if (!_clocksync_eval_beacon_pause)
		{
			DEBUG(
					"_clocksync_eval_cyclic_driver_thread: waking sender thread up");
			thread_wakeup(_clocksync_eval_beacon_pid);
		}
	}
}

static void _clocksync_eval_send_beacon(void)
{
	DEBUG("_clocksync_eval_send_beacon\n");
	gtimer_timeval_t now;
	clocksync_eval_beacon_t *clocksync_eval_beacon =
			(clocksync_eval_beacon_t *) clocksync_eval_beacon_buffer;
	gtimer_sync_now(&now);
	memset(clocksync_eval_beacon_buffer, 0, sizeof(clocksync_eval_beacon_t));
	clocksync_eval_beacon->dispatch_marker = CLOCKSYNC_EVAL_PROTOCOL_DISPATCH;
	clocksync_eval_beacon->counter = _clocksync_eval_beacon_counter;
	sixlowpan_mac_send_ieee802154_frame(0, NULL, 8,
			clocksync_eval_beacon_buffer, sizeof(clocksync_eval_beacon_t), 1);
	_clocksync_eval_beacon_counter++;
}

static uint16_t _clocksync_eval_get_trans_addr(void)
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
