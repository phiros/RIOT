/**
 * Declarations for the clock-sync. evaluation module.
 *
 * Copyright (C) 2014  Philipp Rosenkranz
 *
 * This file subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup sys
 * @{
 * @file    clocksync_eval.h
 * @brief	Declarations for the clock-sync. evaluation module.
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */

#ifndef __CLOCKSYNC_EVAL_H_
#define __CLOCKSYNC_EVAL_H_

#include <stdint.h>

#include "gtimer.h"

typedef struct  __attribute__((packed)) {
	uint8_t dispatch_marker; // << protocol marker
    uint32_t counter; // << sender beacon counter
} clocksync_eval_beacon_t;

void clocksync_eval_init(void);

void clocksync_eval_mac_read(uint8_t *payload, uint16_t src, gtimer_timeval_t *gtimer_toa);

void clocksync_eval_set_beacon_interval(uint32_t lower_delay_in_ms, uint32_t jitter_in_ms);

void clocksync_eval_set_heartbeat_interval(uint32_t delay_in_ms);

void clocksync_eval_pause_sending(void);

void clocksync_eval_resume_sending(void);

void clocksync_eval_pause_heartbeat(void);

void clocksync_eval_resume_heartbeat(void);

#endif /* __CLOCKSYNC_EVAL_H_ */
