/*! \file read_gpsdo.c
 * \brief Intended for Sidekiq cards that support a GPSDO, this application
 * enables the GPS-based oscillator disciplining algorithm and prints the
 * frequency accuracy once per second
 *
 * <pre>
 * Copyright 2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

/***** INCLUDES *****/
#include "sidekiq_api.h"
#include "sidekiq_types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include "arg_parser.h"

/***** DEFINES *****/

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER          0
#endif

#ifndef DEFAULT_TIMEOUT
#   define DEFAULT_TIMEOUT             120
#endif

#ifndef DEFAULT_POLLING_TIME
#   define DEFAULT_POLLING_TIME       20
#endif

#ifndef DEFAULT_CONSECUTIVE_ERROR
#   define DEFAULT_CONSECUTIVE_ERROR       0
#endif

#ifndef POLL_30HZ_USLEEP
# define POLL_30HZ_USLEEP              (33*1000)
#endif

/***** GLOBAL DATA *****/

static uint8_t card = DEFAULT_CARD_NUMBER;
static bool card_present = false;
static char* p_serial = NULL;
static bool serial_present = false;
static uint32_t timeout = DEFAULT_TIMEOUT;
static uint32_t polling_time = DEFAULT_POLLING_TIME;
static uint32_t consecutive_error = DEFAULT_CONSECUTIVE_ERROR;
static char* p_pps_source = "host";
static bool temp_enabled = false;
static bool running = true;
static bool leave_gpsdo_enabled_on_exit = false;

/***** COMMAND LINE VARIABLES *****/

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- Demonstration application for GPSDO interface.";
static const char* p_help_long ="\
    Intended for Sidekiq cards that support a GPSDO, this application demonstrates the\n\
    GPSDO interface. Support for a GPSDO is checked before enabling the GPS-based\n\
    oscillator disciplining algorithm. Once enabled, the locked status of the control \n\
    algorithm is polled. It may take several minutes for the algorithm to converge \n\
    to a locked state. After a lock is obtained the GPSDO frequency accuracy is\n\
    displayed along with a cumulative average of the accuracy.\n\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --timeout=" xstr(DEFAULT_TIMEOUT) "\n\
  --polling-time=" xstr(DEFAULT_POLLING_TIME) "\n\
  --pps-source=host"  "\n\
";

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT_PRESENT("card",
                'c',
                "Specify Sidekiq by card index",
                "ID",
                &card,
                UINT8_VAR_TYPE,
                &card_present),
    APP_ARG_OPT_PRESENT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE,
                &serial_present),
    APP_ARG_OPT("timeout",
                't',
                "Seconds to wait for the GPSDO to lock before terminating",
                "Seconds",
                &timeout,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("polling-time",
                'p',
                "Time to continue reading GPSDO frequency accuracy",
                "Seconds",
                &polling_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("consecutive-error",
                'e',
                "Consecutive iterations skiq_gpsdo_read_freq_accuracy is allowed to return EAGAIN",
                "Instances",
                &consecutive_error,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("pps-source",
                0,
                "Defines the 1PPS source (external or host) ",
                NULL,
                &p_pps_source,
                STRING_VAR_TYPE),
    APP_ARG_OPT("temp-enabled",
                0,
                "Print on-board temperature",
                NULL,
                &temp_enabled,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("leave-enabled",
                0,
                "Leave GPSDO enabled on exit",
                NULL,
                &leave_gpsdo_enabled_on_exit,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

/***** LOCAL FUNCTIONS *****/

/*****************************************************************************/
/** Parse card selection input provided by user for correctness 

    @return status:             indicating status
*/
static int32_t parse_card(void)
{
    int32_t status = 0;

    if(card_present && serial_present)
    {
        fprintf(stderr, "Error: must specify EITHER card ID or serial number, not"
                " both\n");
        status = -EPERM;
        return status;
    }

    /* Card number specification will overrule any serial specification */
    /* If specified, attempt to find the card with a matching serial 
        number when a card is not also specified. */
    if(serial_present && !card_present)
    {
        status = skiq_get_card_from_serial_string(p_serial, &card);
        if (0 != status)
        {
            fprintf(stderr, "Error: cannot find card with serial number %s (result"
                    " code %" PRIi32 ")\n", p_serial, status);
            status = -ENODEV;
            return status;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if(card > (SKIQ_MAX_NUM_CARDS - 1))
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        status = -ERANGE;
    }

    return status;
}

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param[in] signum: the signal number that occurred
    @return void
*/
static void app_cleanup(int signum)
{
    running = false;
}

/*****************************************************************************/
/** This is the main function for executing this application

    @param[in] argc:    the # of arguments from the command line
    @param[in] *argv:   a vector of ascii string arguments from the command line

    @return int:        indicating status
*/
int main( int argc, char *argv[] )
{
    uint64_t prev_rf_ts = 0, prev_sys_ts = 0;
    bool gpsdo_is_enabled = false;
    double gpsdo_freq_ppm = 0;
    bool gpsdo_is_locked = false;
    int32_t status = 0;
    pid_t owner = 0;
    skiq_gpsdo_support_t gpsdo_supported = skiq_gpsdo_support_unknown;
    bool gpsdo_initialized = false;
    uint32_t timeout_count = 0;
    uint32_t polling_count = 0;
    double cumulative_average = 0;
    skiq_1pps_source_t source;
    int8_t temp = 0;
    uint32_t error_counter = 0;

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -EINVAL;
        return status;
    }

    status = parse_card();
    if( status != 0 )
    {
        return status;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize Sidekiq */
    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, &card, 1);
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
            fprintf(stderr, "Error: unable to initialize libsidekiq: was a valid "
                    "card specified (specified card ID was %" PRIu8" )\n", card);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq (status %" PRIi32 ")"
                    " on card %" PRIu8 "\n", status, card);
        }
        return status;
    }

    // Not all cards with GPS capabilities support disciplining a local oscillator
    status = skiq_is_gpsdo_supported( card, &gpsdo_supported );
    if( status != 0 )
    {
        fprintf(stderr, "Error: card %" PRIu8 " is unable to query GPSDO "
                " (status %" PRIi32 ")\n", card, status);
        goto exit;
    }
    if ( gpsdo_supported != skiq_gpsdo_support_is_supported )
    {
        fprintf(stderr, "Warning: card %" PRIu8 " does not support GPSDO functionality\n", card);
        goto exit;
    }

    status = skiq_gpsdo_is_enabled( card, &gpsdo_is_enabled );
    if( status != 0 )
    {
        fprintf(stderr, "Error: card %" PRIu8 " is unable to query GPSDO"
                " (status %" PRIi32 ")\n", card, status);
        goto exit;
    }
    if ( gpsdo_is_enabled == false )
    {
        status = skiq_gpsdo_enable( card );
        if ( status != 0 )
        {
            fprintf(stderr, "Error: card %" PRIu8 " failed to enable GPSDO"
                " (status = %" PRIi32 ")\n", card, status);
            goto exit;
        }
        else
        {
            gpsdo_initialized = true;
            printf("Info: card %" PRIu8 " has enabled the GPSDO\n", card);
        }
    }
    else
    {
        printf("Info: card %" PRIu8 " GPSDO already enabled\n", card);
    }

    /* set the 1pps source to the user's setting */
    if ( 0 == strcasecmp( p_pps_source, "external" ) )
    {
        source = skiq_1pps_source_external;
    }
    else if ( 0 == strcasecmp( p_pps_source, "host" ) )
    {
        source = skiq_1pps_source_host;
    }
    else
    {
        fprintf(stderr, "Error: pps-source is invalid %s\n", p_pps_source);
        status = -1;
        goto exit;
    }


    status = skiq_write_1pps_source(card, source);
    if( status != 0 )
    {
        fprintf(stderr, "Error: card %" PRIu8 " is unable to set 1PPS source"
                " (status %" PRIi32 ")\n", card, status);
        goto exit;
    }
    else
    {
        printf("Info: card %" PRIu8 " is using %s as 1PPS source\n", card, source == skiq_1pps_source_external ? "external" : "host");
    
    }

    /* set the 1pps source to the user's setting */
    if ( 0 == strcasecmp( p_pps_source, "external" ) )
    {
        source = skiq_1pps_source_external;
    }
    else if ( 0 == strcasecmp( p_pps_source, "host" ) )
    {
        source = skiq_1pps_source_host;
    }
    else
    {
        fprintf(stderr, "Error: pps-source is invalid %s\n", p_pps_source);
        status = -1;
        goto exit;
    }


    status = skiq_write_1pps_source(card, source);
    if( status != 0 )
    {
        fprintf(stderr, "Error: card %" PRIu8 " is unable to set 1PPS source"
                " (status %" PRIi32 ")\n", card, status);
        goto exit;
    }
    else
    {
        printf("Info: card %" PRIu8 " is using %s as 1PPS source\n", card, source == skiq_1pps_source_external ? "external" : "host");
    
    }

    // The loop can exit early if a user inputs Ctrl+C to set running=false
    signal(SIGINT, app_cleanup);

    printf("\nInfo: card %" PRIu8 " is waiting for GPSDO to lock...\n", card);
    for( timeout_count=0; (timeout_count<=timeout) && (running) && (gpsdo_is_locked == false); timeout_count++ )
    {
        status = skiq_gpsdo_is_locked( card, &gpsdo_is_locked );
        if( status != 0 )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is unable to query GPSDO"
                    " locked status (status %" PRIi32 ")\n", card, status);
            goto exit;
        }

        printf("\rIs GPSDO Locked: %s | Timeout Counter: %" PRIu32 " / %" PRIu32
                ,gpsdo_is_locked ? " yes" : "no", timeout_count, timeout);
        fflush(stdout);

        if ( gpsdo_is_locked == false )
        {
            sleep(1);
        }
    }
    if ( gpsdo_is_locked == false && timeout_count>=timeout )
    {
        status = ETIME;
        fprintf(stderr, "Error: The GPSDO on card %" PRIu8 " failed to lock within the timeout"
                " period: %" PRIu32 " (status %" PRIi32 ")\n", card, timeout, status);
        goto exit;
    }

    if (running)
    {
        printf("\n\nInfo: card %" PRIu8 " is reading the GPSDO frequency accuracy...\n", card);
    }
    while( polling_count<polling_time && running )
    {
        uint64_t rf_ts = 0;
        uint64_t sys_ts = 0;

        status = skiq_gpsdo_is_locked( card, &gpsdo_is_locked );
        if( status != 0 )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is unable to query GPSDO"
                    " locked status (status %" PRIi32 ")\n", card, status);
            goto exit;
        }
        else if ( gpsdo_is_locked == false )
        {
            status = -EIO;
            fprintf(stderr, "Error: GPSDO on card %" PRIu8 " lost lock during "
                    "operation (status %" PRIi32 ")\n", card, status);
            goto exit;
        }

        status = skiq_read_last_1pps_timestamp( card, &rf_ts, &sys_ts );
        if( status != 0 )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is unable to read last "
                    " PPS status (status %" PRIi32 ")\n", card, status);
            goto exit;
        }

        if ( ( prev_rf_ts != rf_ts ) || ( prev_sys_ts != sys_ts ) )
        {
            prev_rf_ts = rf_ts;
            prev_sys_ts = sys_ts;
            polling_count++;

            status = skiq_gpsdo_read_freq_accuracy( card, &gpsdo_freq_ppm );

            //An EAGAIN error is returned by skiq_gpsdo_read_freq_accuracy if a gps fix or
            //gpsdo lock is momentarily lost while the function executes. Squash this error
            //unless it exceeds the limit set by consecutive_error. 
            if( status == -EAGAIN )
            {
                error_counter++;
                if( consecutive_error > error_counter)
                {
                    printf("Info: error number %" PRIi32 " has occurred %" PRIu32 " consecutive times. Clearing"
                    " the error and trying again\n", status, error_counter);
                    status = 0;
                }
                else
                {
                    printf("Info: error number %" PRIi32 " has occurred %" PRIu32 " consecutive times which exceeds"
                    " the test threshold of %" PRIu32 " consecutive errors.\n", status, error_counter, consecutive_error);
                }
            }
            else if( status == 0 )
            {
                error_counter = 0;
            }
            else
            {
                fprintf(stderr, "Error: card %" PRIu8 " is unable to read the frequency "
                        "accuracy of GPSDO oscillator (status %" PRIi32 ")\n", card, status);
                goto exit;
            }

            //A bufferless cumulative moving average. Equivalent to storing all values and 
            //summming then dividing by n.
            cumulative_average = ( gpsdo_freq_ppm + cumulative_average * ( polling_count - 1 ) )
                                 / polling_count;

            if(temp_enabled)
            {
                //Some long tests have failed due to the temp getting too high on the card.
                //So display the temp along with everything else.
                status = skiq_read_temp( card, &temp );
                if ( status != 0 )
                {
                    fprintf(stderr, "Error: failed to read on-board temperature (result code %" PRIi32
                            ")\n", status);
                    goto exit;
                }


                printf("Time: %3" PRIu32 " / %" PRIu32 " | "
                       "Cumulative Average (ppm): %f | "
                       "Last Measurement (ppm): %f | "
                        "Temperature (°C): %" PRIi8 " \n"
                       , polling_count, polling_time, cumulative_average, gpsdo_freq_ppm, temp );
            }
            else
            {
                printf("Time: %3" PRIu32 " / %" PRIu32 " | "
                       "Cumulative Average (ppm): %f | "
                       "Last Measurement (ppm): %f \n"
                       , polling_count, polling_time, cumulative_average, gpsdo_freq_ppm );
            }


            fflush(stdout);
        }
        else
        {
            usleep(POLL_30HZ_USLEEP);
        }
    }

exit: ;

    int32_t temp_status;
    
    // If the GPSDO was running before the application started allow it to continue running
    if((gpsdo_initialized == true) && (leave_gpsdo_enabled_on_exit == false)) 
    {
        temp_status = skiq_gpsdo_disable( card );
        if ( temp_status != 0 )
        {
            fprintf(stderr, "Warning: card %" PRIu8 " is unable to disable GPSDO "
                    "(status %" PRIi32 ")\n", card, temp_status);
        }
        else
        {
            printf("Info: card %" PRIu8 " has disabled the GPSDO\n", card);
        }
    }
    
    temp_status = skiq_exit();
    if ( temp_status != 0 )
    {
        fprintf(stderr, "Warning: libsidekiq failed to shutdown properly"
                "(status %" PRIi32 ")\n", temp_status);
    }

    return (status == 0 ? temp_status : status);
}

