/*! \file read_temp.c
 * \brief This file contains a basic application that reads the
 * temperature of the specified Sidekiq.
 *
 * <pre>
 * Copyright 2013 - 2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <sidekiq_api.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <arg_parser.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- obtain temperature measurements";
static const char* p_help_long =
"\
Reads the onboard temperature of a given Sidekiq.\n\
\n\
Some product's temperature sensors are only available with a 'full' init level.\n\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n";

/* command line argument variables */
static uint8_t card = DEFAULT_CARD_NUMBER;
static bool card_was_specified = false;
static char* p_serial = NULL;
static uint32_t repeat = 0;
static bool do_full_init = false;
static bool running = true;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT_PRESENT("card",
                        'c',
                        "Specify Sidekiq by card index",
                        "ID",
                        &card,
                        UINT8_VAR_TYPE,
                        &card_was_specified),
    APP_ARG_OPT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_OPT("full",
                'f',
                "Initialize at skiq_xport_init_level_full instead of skiq_xport_init_level_basic (default behavior)",
                NULL,
                &do_full_init,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("repeat",
                0,
                "Read the onboard temperature N additional times",
                "N",
                &repeat,
                UINT32_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);
    running = false;
}

/*****************************************************************************/
/** This is the main function for executing read_temp application

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ASCII string arguments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    int8_t temp = 0;
    int32_t status = 0;
    uint32_t i = 0;
    pid_t owner = 0;

    bool skiq_initialized = false;

    skiq_xport_type_t type = skiq_xport_type_auto;
    skiq_xport_init_level_t level = skiq_xport_init_level_basic;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -1;
        goto finished;
    }

    if ( card_was_specified && (NULL != p_serial) )
    {
        printf("Error: must specify EITHER card ID or serial number, not"
                " both\n");
        status = -1;
        goto finished;
    }

    /* If specified, attempt to find the card with a matching serial number. */
    if ( NULL != p_serial )
    {
        status = skiq_get_card_from_serial_string(p_serial, &card);
        if (0 != status)
        {
            fprintf(stderr, "Error: cannot find card with serial number %s"
                    " (result code %" PRIi32 ")\n", p_serial, status);
            status = -1;
            goto finished;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        status = -1;
        goto finished;
    }

    if ( do_full_init )
    {
        level = skiq_xport_init_level_full;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize Sidekiq */
    status = skiq_init(type, level, &card, 1);
    if( status != 0 )
    {
        if ( ( EBUSY == status ) &&
             ( 0 != skiq_is_card_avail( card, &owner ) ) )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is already in use (by"
                    " process ID %u); cannot initialize card.\n", card,
                    (unsigned int) owner);
        }
        else if ( -EINVAL == status )
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq; was a"
                    " valid card specified? (result code %" PRIi32 ")\n",
                    status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with"
                    " status %d\n", status);
        }
        status = -1;
        goto finished;
    }
    skiq_initialized = true;

    for ( i = 0; ( i <= repeat ) && running; i++ )
    {
        /* read the temperature */
        status = skiq_read_temp( card, &temp );
        if ( status == 0 )
        {
            printf("Info: on-board temperature is %" PRIi8 " degrees Celsius\n", temp);
        }
        else if ( status == -EAGAIN )
        {
            fprintf(stderr, "Error: on-board temperature is temporarily unavailable\n");
        }
        else if ( status == -ENODEV )
        {
            fprintf(stderr, "Warning: on-board temperature may not available at configured "
                    "skiq_xport_init_level, try specifying --full in the command argument list\n");
        }
        else
        {
            fprintf(stderr, "Error: failed to read on-board temperature (result code %" PRIi32
                    ")\n", status);
        }

        sleep(1);
    }

    status = 0;

finished:
    if (skiq_initialized)
    {
        skiq_exit();
        skiq_initialized = false;
    }

    return ((int) status);
}

