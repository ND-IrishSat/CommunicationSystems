/*! \file rx_benchmark.c
 * \brief This file contains a basic application for benchmarking
 * the sending of packets to the DMA driver.
 *  
 * <pre>
 * Copyright 2014-2018 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <inttypes.h>

#include <sidekiq_api.h>
#include <arg_parser.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "characterize receive";
static const char* p_help_long = "\
Receives data using the chosen transport layer, reporting back benchmark\n\
information collected during execution.\n\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --handle=A1\n\
  --rate=1000000";

/* command line argument variables */
static char* p_handle = "A1";
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static uint32_t sample_rate = 1000000;
static bool blocking_rx = false;
static bool packed = false;
static bool low_latency = false;
static bool balanced = false;
static char* p_temp_log_name = NULL;
static bool temp_log_is_set = false;

/* global variables shared amongst threads */
static pthread_t monitor_thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool running = true;
static uint64_t num_bytes=0; // # bytes received
static uint32_t throughput=0; // current MB/s
static uint32_t target=0; // desired MB/s
static bool target_is_set = false;
static uint32_t num_pkts[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = 0 };
static uint64_t ts_gaps[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = 0 };
static uint64_t threshold=0; // max number of timestamp errors before failure
static bool threshold_is_set = false;
static uint32_t run_time=0;
static FILE *p_temp_log=NULL;

/* keep track of the specified handles to start streaming */
static skiq_rx_hdl_t handles[skiq_rx_hdl_end];
static uint8_t nr_handles = 0;

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
    APP_ARG_OPT("handle",
                0,
                "Comma delimited list of Rx handles to enable",
                "Rx[,Rx]...",
                &p_handle,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("target",
                        0,
                        "Desired data throughput in megabytes per second",
                        "MB/s",
                        &target,
                        UINT32_VAR_TYPE,
                        &target_is_set),
    APP_ARG_OPT_PRESENT("threshold",
                        0,
                        "Number of timestamp gaps before considering test a failure",
                        "NUMBER",
                        &threshold,
                        UINT64_VAR_TYPE,
                        &threshold_is_set),
    APP_ARG_OPT("time",
                't',
                "Number of seconds to run benchmark",
                "SECONDS",
                &run_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("blocking",
                0,
                "Perform blocking during skiq_receive call",
                NULL,
                &blocking_rx,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("packed",
                0,
                "Use packed mode for I/Q samples",
                NULL,
                &packed,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("low-latency",
                0,
                "Configure receive stream mode to low latency",
                NULL,
                &low_latency,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("balanced",
                0,
                "Configure receive stream mode to balanced",
                NULL,
                &balanced,
                BOOL_VAR_TYPE),
    APP_ARG_OPT_PRESENT("temp-log",
                        0,
                        "File name to log temperature data",
                        "PATH",
                        &p_temp_log_name,
                        STRING_VAR_TYPE,
                        &temp_log_is_set),    
    APP_ARG_TERMINATOR,
};


/**************************************************************************************************/
static const char *
rx_hdl_cstr( skiq_rx_hdl_t hdl )
{
    const char* p_hdl_str =
        (hdl == skiq_rx_hdl_A1) ? "A1" :
        (hdl == skiq_rx_hdl_A2) ? "A2" :
        (hdl == skiq_rx_hdl_B1) ? "B1" :
        (hdl == skiq_rx_hdl_B2) ? "B2" :
        (hdl == skiq_rx_hdl_C1) ? "C1" :
        (hdl == skiq_rx_hdl_D1) ? "D1" :
        "unknown";

    return p_hdl_str;
}


static skiq_rx_hdl_t
str2hdl( const char *str )
{
    return ( 0 == strcasecmp( str, "A1" ) ) ? skiq_rx_hdl_A1 :
        ( 0 == strcasecmp( str, "A2" ) ) ? skiq_rx_hdl_A2 :
        ( 0 == strcasecmp( str, "B1" ) ) ? skiq_rx_hdl_B1 :
        ( 0 == strcasecmp( str, "B2" ) ) ? skiq_rx_hdl_B2 :
        ( 0 == strcasecmp( str, "C1" ) ) ? skiq_rx_hdl_C1 :
        ( 0 == strcasecmp( str, "D1" ) ) ? skiq_rx_hdl_D1 :
        skiq_rx_hdl_end;
}


static int32_t
parse_hdl_list( char *handle_str,
                skiq_rx_hdl_t rx_handles[],
                uint8_t *p_nr_handles,
                skiq_chan_mode_t *p_chan_mode )
{
#define TOKEN_LIST ",;:"
    bool handle_requested[skiq_rx_hdl_end];
    skiq_rx_hdl_t rx_hdl;
    char *token, *handle_str_dup;

    for ( rx_hdl = skiq_rx_hdl_A1; rx_hdl < skiq_rx_hdl_end; rx_hdl++ )
    {
        handle_requested[rx_hdl] = false;
    }

    handle_str_dup = strdup( handle_str );
    if ( handle_str_dup == NULL )
    {
        return -ENOMEM;
    }

    token = strtok( handle_str_dup, TOKEN_LIST );
    while ( token != NULL )
    {
        rx_hdl = str2hdl( token );
        if ( rx_hdl == skiq_rx_hdl_end )
        {
            free( handle_str_dup );
            return -EPROTO;
        }
        else
        {
            handle_requested[rx_hdl] = true;
        }

        token = strtok( NULL, TOKEN_LIST );
    }

    *p_nr_handles = 0;
    for ( rx_hdl = skiq_rx_hdl_A1; rx_hdl < skiq_rx_hdl_end; rx_hdl++ )
    {
        if ( handle_requested[rx_hdl] )
        {
            rx_handles[(*p_nr_handles)++] = rx_hdl;
        }
    }

    /* set chan_mode based on whether one of the second handles in each pair is requested */
    if ( handle_requested[skiq_rx_hdl_A2] || handle_requested[skiq_rx_hdl_B2] )
    {
        *p_chan_mode = skiq_chan_mode_dual;
    }
    else
    {
        *p_chan_mode = skiq_chan_mode_single;
    }

    free( handle_str_dup );

    return 0;
#undef TOKEN_LIST
}


/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    printf("Info: received signal %d, cleaning up libsidekiq...\n", signum);
    running = false; // clears the running flag so that everything exits
}

/*****************************************************************************/
/** This is a separate thread that monitors the performance of the DMA engine.

    @param none
    @return none
*/
void* monitor_performance(void *ptr)
{
    uint64_t last_ts_gaps[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = 0 };
    uint64_t monitor_time=0;

    /* run until the running flag gets cleared */
    while( running )
    {
        sleep(1);
        /* if still running after sleeping, report statistics */
        if ( running )
        {
            uint8_t i;

            pthread_mutex_lock(&lock);

            throughput = num_bytes / 1000000;

            printf("Receive throughput: %3u MB/s", throughput);
            for (i = 0; i < nr_handles; i++)
            {
                skiq_rx_hdl_t hdl = handles[i];

                if ( hdl < skiq_rx_hdl_end )
                {
                    printf(" (RX%s pkts %" PRIu32 ") (# Rx%s timestamp gaps total %" PRIu64
                           ", delta %" PRIu64 ")", rx_hdl_cstr(hdl), num_pkts[hdl],
                           rx_hdl_cstr(hdl), ts_gaps[hdl], ts_gaps[hdl] - last_ts_gaps[hdl]);

                    last_ts_gaps[hdl] = ts_gaps[hdl];
                }
            }
            printf("\n");

            if( run_time > 0 )
            {
                running = (0 == --run_time) ? false : running;
            }

            num_bytes=0;
            pthread_mutex_unlock(&lock);

            if( p_temp_log != NULL )
            {
                int32_t temp_status=0;
                int8_t temp=0;

                // append the header if the beginning of the file
                if( monitor_time == 0 )
                {
                    fprintf(p_temp_log, "Time(s),Temperature(C)\n");
                }
                monitor_time++;

                if( (temp_status=skiq_read_temp(card, &temp)) == 0 )
                {
                    printf("Current temperature: %d C\n", temp);
                    // write the temperature to the file
                    fprintf(p_temp_log, "%" PRIu64 ",%d\n", monitor_time, temp);
                }
                else
                {
                    printf("Unable to obtain temperature (status=%d)\n",
                           temp_status);
                }
            }
        }
    }

    return NULL;
}

/*****************************************************************************/
/** This is the main function for executing the tx_benchmark app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    int32_t status = 0;
    skiq_rx_block_t* p_rx_block;
    uint32_t data_len;
    skiq_xport_type_t type = skiq_xport_type_auto;
    skiq_xport_init_level_t level = skiq_xport_init_level_full;
    skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
    skiq_rx_hdl_t rx_hdl=skiq_rx_hdl_A1;
    uint64_t curr_ts[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = 0 };
    uint64_t next_ts[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = 0 };
    bool first_block[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = true };
    uint8_t i;
    pid_t owner = 0;
    skiq_rx_stream_mode_t stream_mode = skiq_rx_stream_mode_high_tput;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    if ( threshold_is_set && ( threshold == 0 ) )
    {
        fprintf(stderr, "Error: cannot specify a timestamp gap threshold of 0\n");
        return (-1);
    }

    if( balanced == true && low_latency == true )
    {
        fprintf(stderr, "Error: cannot specify both balanced and low latency stream mode\n");
        return (-1);
    }

    if( low_latency == true && packed == true )
    {
        fprintf(stderr, "Error: cannot specify both low latency stream mode and packed mode\n");
        return (-1);
    }

    if( (UINT8_MAX != card) && (NULL != p_serial) )
    {
        fprintf(stderr, "Error: must specify EITHER card ID or serial number, not"
                " both\n");
        return (-1);
    }
    if (UINT8_MAX == card)
    {
        card = DEFAULT_CARD_NUMBER;
    }

    /* If specified, attempt to find the card with a matching serial number. */
    if ( NULL != p_serial )
    {
        status = skiq_get_card_from_serial_string(p_serial, &card);
        if ( 0 != status )
        {
            fprintf(stderr, "Error: cannot find card with serial number %s (result"
                    " code %" PRIi32 ")\n", p_serial, status);
            return (-1);
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 "\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return (-1);
    }

    /* map argument values to Sidekiq specific variable values */
    status = parse_hdl_list( p_handle, handles, &nr_handles, &chan_mode );
    if ( status == 0 )
    {
        if ( nr_handles == 0 )
        {
            fprintf(stderr, "Error: invalid number of handles specified (must be greater than zero)\n");
            status = -1;
            goto sidekiq_exit;
        }
    }
    else
    {
        fprintf(stderr, "Error: invalid handle list specified: '%s'\n", p_handle);
        status = -1;
        goto sidekiq_exit;
    }

    /* open the temperature log if it's set */
    if( temp_log_is_set == true )
    {
        p_temp_log = fopen( p_temp_log_name, "w+" );
        if( p_temp_log == NULL )
        {
            fprintf(stderr, "Error: unable to open temperature log %s (errno=%d)\n", p_temp_log_name, errno);
            goto sidekiq_exit;
        }
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize the interface */
    status = skiq_init( type, level, &card, 1 );
    if( status != 0 )
    {
        if ( ( EBUSY == status ) &&
             ( 0 != skiq_is_card_avail(card, &owner) ) )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is already in use (by process ID"
                    " %u); cannot initialize card.\n", card,
                    (unsigned int) owner);
        }
        else if ( -EINVAL == status )
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq; was a valid card"
                    " specified? (result code %" PRIi32 ")\n", status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with status %" PRIi32
                    "\n", status);
        }

        if( p_temp_log != NULL )
        {
            fclose( p_temp_log );
        }
        return (-1);
    }

    if ( blocking_rx )
    {
        /* set a modest rx timeout */
        status = skiq_set_rx_transfer_timeout( card, 100000 );
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to set RX transfer timeout with status %d\n",
                    status);
            skiq_exit();
            if( p_temp_log != NULL )
            {
                fclose( p_temp_log );
            }
            return (-1);
        }
    }

    /* set the receive streaming mode to either depending on command line */
    if( low_latency == true )
    {
        stream_mode = skiq_rx_stream_mode_low_latency;
    }
    else if( balanced == true )
    {
        stream_mode = skiq_rx_stream_mode_balanced;
    }
    status = skiq_write_rx_stream_mode( card, stream_mode );
    if( status != 0 )
    {
        fprintf(stderr, "Error: failed to set RX stream mode with status %d\n", status);
        skiq_exit();
        if( p_temp_log != NULL )
        {
            fclose( p_temp_log );
        }
        return (-1);
    }

    /* set the mode (packed or unpacked) */
    if( (status=skiq_write_iq_pack_mode(card, packed)) != 0 )
    {
        if ( status == -ENOTSUP )
        {
            fprintf(stderr, "Error: packed mode is not supported on this Sidekiq product\n");
        }
        else
        {
            fprintf(stderr, "Error: failed to set the packed mode with status %d\n", status);
        }
        skiq_exit();
        if( p_temp_log != NULL )
        {
            fclose( p_temp_log );
        }
        return (-1);
    }
    printf("Info: IQ pack mode: %s\n", (packed == true) ? "enabled" : "disabled");

    // configure the sample rate and start streaming
    for( i=0; i<nr_handles; i++ )
    {
        status = skiq_write_rx_sample_rate_and_bandwidth(card, handles[i], sample_rate, (uint32_t)(0.8 * sample_rate));
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure sample rate and bandwidth\n");
            skiq_exit();
            if( p_temp_log != NULL )
            {
                fclose( p_temp_log );
            }
            return (-3);
        }
    }

    status = skiq_write_chan_mode(card, chan_mode);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure channel mode\n");
        skiq_exit();
        if( p_temp_log != NULL )
        {
            fclose( p_temp_log );
        }
        return (-3);
    }

    /* initialize a thread to monitor our performance */
    pthread_create( &monitor_thread, NULL, monitor_performance, NULL );

    /* start receive streaming */
    printf("Info: starting %u Rx interface(s) on card %u\n", nr_handles, card);
    status = skiq_start_rx_streaming_multi_immediate( card, handles, nr_handles );
    if ( status != 0 )
    {
        fprintf(stderr, "Error: starting %u Rx interface(s) on card %u failed with status %d\n", nr_handles, card, status);
        goto sidekiq_exit;
    }

    /* run forever and ever and ever (or until Ctrl-C) */
    while( running )
    {
        if( skiq_receive(card,
                         &rx_hdl,
                         &p_rx_block,
                         &data_len) == skiq_rx_status_success )
        {
            pthread_mutex_lock(&lock);
            if ( rx_hdl < skiq_rx_hdl_end )
            {
                curr_ts[rx_hdl] = p_rx_block->rf_timestamp;
                if ( !first_block[rx_hdl] )
                {
                    if ( curr_ts[rx_hdl] != next_ts[rx_hdl] )
                    {
                        if ( curr_ts[rx_hdl] < next_ts[rx_hdl] )
                        {
                            fprintf(stderr, "Error: Rx%s backward timestamp detected: current = "
                                    "0x%016" PRIx64 ", expected = 0x%016" PRIx64 "\n",
                                    rx_hdl_cstr(rx_hdl), curr_ts[rx_hdl], next_ts[rx_hdl] );
                        }
                        ts_gaps[rx_hdl]++;
                    }
                }
                else
                {
                    first_block[rx_hdl] = false;
                }

                /* next timestamp value depends on IQ pack mode setting */
                if (packed)
                {
                    next_ts[rx_hdl] = curr_ts[rx_hdl] + SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK((data_len / 4) - SKIQ_RX_HEADER_SIZE_IN_WORDS);
                }
                else
                {
                    next_ts[rx_hdl] = curr_ts[rx_hdl] + (data_len / 4) - SKIQ_RX_HEADER_SIZE_IN_WORDS;
                }
                num_pkts[rx_hdl]++;
            }
            else
            {
                fprintf(stderr, "Error: out-of-range receive handle %u provided by skiq_receive()\n", rx_hdl);
            }
            num_bytes += data_len;
            pthread_mutex_unlock(&lock);
        }
    }

    /* stop receive streaming */
    printf("Info: stopping %u Rx interface(s) on card %u\n", nr_handles, card);
    skiq_stop_rx_streaming_multi_immediate( card, handles, nr_handles );

    /* wait for the monitor thread to complete */
    pthread_join(monitor_thread,NULL);

sidekiq_exit:
    if( p_temp_log != NULL )
    {
        fclose( p_temp_log );
    }
    skiq_exit();

    if ( target_is_set && ( throughput < target) )
    {
        fprintf(stderr, "Error: Measured throughput (%u MB/s) did not meet target (%u MB/s)\n",
                throughput, target );
        return (1);
    }

    for ( i = 0; i < nr_handles; i++ )
    {
        if ( threshold_is_set && ( ts_gaps[handles[i]] >= threshold ) )
        {
            fprintf(stderr, "Error: Number of timestamp gaps (%" PRIu64 ") on handle %s exceeded "
                    "specified threshold (%" PRIu64 ")\n", ts_gaps[handles[i]],
                    rx_hdl_cstr(handles[i]), threshold);
            return (1);
        }
    }

    return (0);
}

