/**
 * Shell commands for the ftsp clock-sync module
 *
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup shell_commands
 * @{
 * @file    sc_ftsp.c
 * @brief   Provides interactive access to the FTSP clock-sync module.
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "clocksync/ftsp.h"

static void _print_help(void);

void _ftsp(int argc, char **argv)
{
    if (argc == 2)
    {
        if (!strcmp(argv[1], "on"))
        {
            puts("ftsp enabled");
            ftsp_resume();
            return;
        }
        if (!strcmp(argv[1], "off"))
        {
            puts("ftsp disabled");
            ftsp_pause();
            return;
        }
    }
    _print_help();
}

static void _print_help(void)
{
    printf("Usage: ftsp [on] | [off]\n");
}
