/**
 * Implementation of the Gradient Time Synchronisation Protocol.
 *
 * Copyright (C) 2013  Freie Universität Berlin.
 *
 * This file subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup sys
 * @{
 * @file    gtsp.h
 * @brief	  A global timer implementation based upon GTSP.
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */
#ifndef GTSP_H
#define GTSP_H

#include "generic_ringbuffer.h"
#include "gtimer.h"
#include "ieee802154_frame.h"

#define ABS64T(X)	((X) < 0 ? -1*(X) : (X)) // not sure...


typedef struct  __attribute__((packed)) {
	uint8_t dispatch_marker;
    uint64_t local; // << sender hardware time
    uint64_t global; // << sender logical time
    float relative_rate; // << sender logical clockrate
} gtsp_beacon_t;


typedef struct {
	uint16_t src;
	uint64_t local_local; // << current local hardware time when message was processed
	uint64_t local_global; // << current local logical time when message was processed
	uint64_t remote_local;
	uint64_t remote_global;
	float remote_rate;
	float relative_rate;
} gtsp_sync_point_t;

/**
 * @brief Starts the GTSP protocol
 */
void gtsp_init(void);

void gtsp_set_delay(uint32_t s);

void gtsp_set_prop_time(uint32_t us);

void gtsp_pause(void);

void gtsp_resume(void);

static void _gtsp_beacon_thread(void);

static void _gtsp_cyclic_driver_thread(void);

/**
 * @brief sends a sync packet to all neighbors.
 */
static void _gtsp_send_beacon(void);

static int _gtsp_buffer_lookup(generic_ringbuffer_t *rb,
		uint16_t src);

/**
 * @brief reads a frame supplied by the mac layer of sixlowpan.
 * This function should only be called by mac.c
 */
void gtsp_mac_read(uint8_t *frame_payload, uint16_t src, gtimer_timeval_t *toa);



/**
 * @brief Refreshes the timestamp in a frame.
 * This function is executed shortly before transmitting a packet.
 * The function should only be executed by a transceiver driver.
 */
void gtsp_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length);

#endif /* GTSP_H */
