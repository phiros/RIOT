/**
 * ftsp.h - Declarations and types for the Gradient Time Synchronisation Protocol.
 *
 * Copyright (C) 2014  Philipp Rosenkranz
 *
 * This file subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup ftsp    FTSP - Gradient Time Synchronisation Protocol.
 * @ingroup  net
 * @brief    The Gradient Clock Synchronization Protocol is a decentralized clock
 *           synchronization protocol which tries to synchronizes not only the
 *           clock values but also the clock rate of all nodes in a network.
 * @see      <a href="http://www.disco.ethz.ch/publications/ipsn09.pdf">
 *              Sommer et.al.: Gradient Clock Synchronization in Wireless Sensor Networks
 *           </a>
 * @{
 * @file     ftsp.h
 * @brief    Declarations for the Gradient Clock Synchronization Protocol.
 * @author   Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */
#ifndef __FTSP_H
#define __FTSP_H

#include "generic_ringbuffer.h"
#include "gtimer.h"
#include "radio/types.h"

typedef struct  __attribute__((packed)) {
	uint8_t dispatch_marker; // << protocol marker
    uint16_t id;
    uint16_t root;
    uint16_t seq_number;
    uint64_t local;
    uint64_t offset;
    float relative_rate; // << sender logical clockrate
} ftsp_beacon_t;


typedef struct ftsp_sync_point {
    uint64_t local;
    uint64_t offset;
} ftsp_sync_point_t;

/**
 * @brief Starts the FTSP module
 */
void ftsp_init(void);

/**
 * @brief sets the beacon interval in seconds.
 */
void ftsp_set_beacon_delay(uint32_t delay_in_sec);

/**
 * @brief sets the minimal delay between sending and receiving a beacon.
 * To be more specific: Let t_s be the time when a timestamp is applied to a beacon
 * shortly before it is sent and t_r the time when the time of arrival
 * of this sent beacon is recorded at the receiver. The difference of t_s
 * and t_r then signifies the delay between sending and receiving a beacon.
 * This delay has to be determined for every platform (read: for different MCU /
 * transceiver combinations).
 */
void ftsp_set_prop_time(uint32_t us);

/**
 * @brief Causes ftsp to stop sending beacons / ignoring received beacons.
 */
void ftsp_pause(void);

/**
 * @brief Causes ftsp to restart/start sending beacons and processing received beacons.
 */
void ftsp_resume(void);

/**
 * @brief reads a frame supplied by the mac layer of sixlowpan.
 * This function should only be called by mac.c
 */
void ftsp_mac_read(uint8_t *frame_payload, uint16_t src, gtimer_timeval_t);

/**
 * @brief Refreshes the timestamp in a frame.
 * This function is executed shortly before transmitting a packet.
 * The function should only be executed by a transceiver driver.
 */
void ftsp_driver_timestamp(uint8_t *ieee802154_frame, uint8_t frame_length);

#endif /* __FTSP_H */
