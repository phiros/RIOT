/**
 * Shell commands for the pulsesync clock-sync module
 *
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup shell_commands
 * @{
 * @file    sc_pulsesync.c
 * @brief   Provides interactive access to the PulseSync clock-sync module.
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "clocksync/pulsesync.h"

static void _print_help(void);

void _pulsesync(int argc, char **argv)
{
    if (argc == 2)
    {
        if (!strcmp(argv[1], "on"))
        {
            puts("pulsesync enabled");
            pulsesync_resume();
            return;
        }
        if (!strcmp(argv[1], "off"))
        {
            puts("pulsesync disabled");
            pulsesync_pause();
            return;
        }
    }
    _print_help();
}

static void _print_help(void)
{
    printf("Usage: pulsesync [on] | [off]\n");
}
