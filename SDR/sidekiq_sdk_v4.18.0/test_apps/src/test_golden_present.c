/*! \file test_golden_present.c
 * \brief This file contains a basic application that checks for the presence of
 * the golden FPGA image
 *
 * <pre>
 * Copyright 2014 - 2018 Epiq Solutions, All Rights Reserved
 * </pre>
 */
#include <sidekiq_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include <arg_parser.h>

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- determine if golden FPGA is saved";
static const char* p_help_long =
"\
Reads flash memory to see if the golden FPGA bitstream is presently saved.  This\n\
image will be used as a fallback in the event that the user FPGA is missing or\n\
corrupted.";

/* command line argument variables */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("card",
                'c',
                "Specify Sidekiq by card index",
                "ID",
                &card,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

int main( int argc, char *argv[] )
{
    skiq_xport_type_t type;
    skiq_xport_init_level_t level = skiq_xport_init_level_basic;
    bool usb_available = true;
    int32_t status=0;
    uint8_t present=0;
    pid_t owner = 0;

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    if ( ( UINT8_MAX == card ) && ( NULL == p_serial ) )
    {
        fprintf(stderr, "Error: one of --card or --serial MUST be specified\n");
        return (-1);
    }
    else if ( ( UINT8_MAX != card ) && ( NULL != p_serial ) )
    {
        fprintf(stderr, "Error: EITHER --card OR --serial must be specified,"
                " not both\n");
        return (-1);
    }

    /* disable messages */
    skiq_register_logging(NULL);

    /* If specified, attempt to find the card with a matching serial number. */
    if ( NULL != p_serial )
    {
        status = skiq_get_card_from_serial_string(p_serial, &card);
        if (0 != status)
        {
            fprintf(stderr, "Error: cannot find card with serial number %s"
                    " (result code %" PRIi32 ")\n", p_serial, status);
            return (-1);
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return (-1);
    }

    if( skiq_is_xport_avail(card, skiq_xport_type_usb) == 0 )
    {
        usb_available = true;
        type = skiq_xport_type_usb;
    }
    else
    {
        usb_available = false;
        type = skiq_xport_type_pcie;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    status = skiq_init(type, level, &card, 1);
    if ( status != 0 )
    {
        if ( ( EBUSY == status ) &&
             ( 0 != skiq_is_card_avail(card, &owner) ) )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is already in use (by"
                    " process ID %u); cannot initialize card.\n", card,
                    (unsigned int) owner);
        }
        else if ( -EINVAL == status )
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq; was a"
                    " valid card specified ? (result code %" PRIi32 ")\n",
                    status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with"
                    " status %" PRIi32 "\n", status);
        }
        return (-1);
    }

    skiq_read_golden_fpga_present_in_flash( card, &present );

    /* traditionally a return status of 0 indicates success */
    if( present == 1 )
    {
        printf("Info: golden FPGA presence detected\n");
        status = 0;
    }
    else if ( ( present == 0 ) && ( usb_available ) )
    {
        printf("Info: golden FPGA presence NOT detected\n");
        status = 1;
    }
    else if ( ( present == 0 ) && ( !usb_available ) )
    {
        /* this is the worst case, no golden and no USB available over which to
         * store the golden image, cannot proceed */
        printf("Info: golden FPGA presence NOT detected and USB not available\n");
        status = 2;
    }

    skiq_exit();

    return ((int) status);
}

