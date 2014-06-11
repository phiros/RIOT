/**
 * Shell commands for the gtsp clock-sync module
 *
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup shell_commands
 * @{
 * @file    sc_gtsp.c
 * @brief   Provides interactive access to the GTSP clock-sync module.
 * @author  Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "gtsp.h"

static void _print_help(void);

void _gtsp(int argc, char **argv)
{
    if (argc == 2)
    {
        if (!strcmp(argv[1], "on"))
        {
            puts("gtsp enabled");
            gtsp_resume();
            return;
        }
        if (!strcmp(argv[1], "off"))
        {
            puts("gtsp disabled");
            gtsp_pause();
            return;
        }
#ifdef GTSP_ENABLE_TRIGGER
        if (!strcmp(argv[1], "trigger"))
        {
            gtsp_print_trigger();
            return;
        }
    }
    else if (argc == 4)
    {
        if (!strcmp(argv[1], "trigger"))
        {
            int16_t a = atoi(argv[3]);
            if (!strcmp(argv[2], "rm"))
            {
                gtsp_del_trigger_address((uint8_t) a);
                return;
            }

            if (!strcmp(argv[2], "add"))
            {
                gtsp_add_trigger_address((uint8_t) a);
                return;
            }
        }
    }
#else
    }
#endif
    _print_help();
}

static void _print_help(void)
{
    printf("Usage: gtsp [on] | [off] | [trigger [rm|add address]]  \n");
}
