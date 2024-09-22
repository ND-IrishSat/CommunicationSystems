/*! \file sweep_receive.c
 * \brief This file contains a basic application for sweeping a
 * specified frequency range and performing basic metrics on sweep
 * and flush times.
 *
 * <pre>
 * Copyright 2015 - 2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>
#include <ctype.h>

#include "arg_parser.h"
#include "sidekiq_api.h"
#include "elapsed.h"

#define SET_RX_LO_FREQ(_card,_hdl,_freq)                        \
    ({                                                          \
        int32_t _status;                                        \
        elapsed_start(&tune_time);                              \
        _status = skiq_write_rx_LO_freq( _card, _hdl, _freq );  \
        elapsed_end(&tune_time);                                \
        _status;                                                \
    })

#define START_STREAM(_card,_hdl)                                \
    ({                                                          \
        int32_t _status;                                        \
        elapsed_start(&start_stream_time);                      \
        _status = skiq_start_rx_streaming(_card, _hdl);         \
        elapsed_end(&start_stream_time);                        \
        _status;                                                \
    })

#define STOP_STREAM(_card,_hdl)                                 \
    ({                                                          \
        int32_t _status;                                        \
        elapsed_start(&stop_stream_time);                       \
        _status = skiq_stop_rx_streaming(_card, _hdl);          \
        elapsed_end(&stop_stream_time);                         \
        _status;                                                \
    })

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef MIN
#define MIN(a,b)                                \
    ({ __typeof__ (a) _a = (a);                 \
        __typeof__ (b) _b = (b);                \
        _a < _b ? _a : _b; })
#endif

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- sweep LO and receive samples";
static const char* p_help_long = "\
Receives the number of blocks specified at the configured sample rate and\n\
then stops streaming. The LO frequency is updated and streaming is restarted\n\
and data is received for the number of iterations specified. Various metrics\n\
are reported.\n\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --blocks=100\n\
  --rate=10000000\n\
  --start=75000000\n\
  --stop=6000000000\n\
  --step=30000000\n\
";

/* command line argument variables */
static uint8_t card = UINT8_MAX ;
static char* p_serial = NULL;
static bool blocking_rx = false;
static uint32_t num_blocks = 100;
static uint32_t sample_rate = 10000000;
static uint64_t start_freq = 75000000;
static uint64_t stop_freq = 6000000000;
static uint64_t step_size = 30000000;
static uint32_t repeat = 0;
/* boolean used to track status of application */
static bool running = true;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("blocks",
                0,
                "Number of Rx sample blocks to acquire",
                "N",
                &num_blocks,
                UINT32_VAR_TYPE),
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
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("repeat",
                0,
                "Sweep frequency an additional N times",
                "N",
                &repeat,
                INT32_VAR_TYPE),
    APP_ARG_OPT("start",
                0,
                "Starting LO frequency",
                "Hz",
                &start_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("stop",
                0,
                "End LO frequency",
                "Hz",
                &stop_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("step",
                0,
                "LO frequency step size",
                "Hz",
                &step_size,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("blocking",
                0,
                "Perform blocking during skiq_receive call",
                NULL,
                &blocking_rx,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,
};


/***** LOCAL FUNCTIONS *****/

static void app_cleanup(int signum);
static int32_t receive_data( uint32_t num_blocks );
static void print_block_contents( skiq_rx_block_t* p_block,
                                  int32_t block_size_in_bytes );


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
/** This is the main function for executing the sweep_receive app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    int32_t status=0;
    uint32_t curr_iteration=0;
    uint64_t curr_freq;
    uint32_t num_receive_errors=0;

    ELAPSED(app_time);
    ELAPSED(tune_time);
    ELAPSED(start_stream_time);
    ELAPSED(stop_stream_time);
    ELAPSED(capture_time);

    pid_t owner = 0;

    /* install a signal handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    if( (UINT8_MAX != card) && (NULL != p_serial) )
    {
        printf("Error: must specify EITHER card ID or serial number, not"
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
        if (0 != status)
        {
            printf("Error: cannot find card with serial number %s (result"
                    " code %" PRIi32 ")\n", p_serial, status);
            return (-1);
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        printf("Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return (-1);
    }

    // initialize the start frequency
    curr_freq = start_freq;

    printf("Info: initializing card %" PRIu8 "...\n", card);

    // initialize libsidekiq
    status = skiq_init(skiq_xport_type_auto,
                       skiq_xport_init_level_full,
                       &card,
                       1);
    if( status != 0 )
    {
        if ( ( EBUSY == status ) &&
             ( 0 != skiq_is_card_avail(card, &owner) ) )
        {
            printf("Error: card %" PRIu8 " is already in use (by process ID"
                    " %u); cannot initialize card.\n", card,
                    (unsigned int) owner);
        }
        else if ( -EINVAL == status )
        {
            printf("Error: unable to initialize libsidekiq; was a valid card"
                    " specified? (result code %" PRIi32 ")\n", status);
        }
        else
        {
            printf("Error: unable to initialize libsidekiq with status %" PRIi32
                    "\n", status);
        }
        return (-1);
    }

    if ( blocking_rx )
    {
        /* set a modest rx timeout */
        status = skiq_set_rx_transfer_timeout( card, 10000 );
        if( status != 0 )
        {
            printf("Error: unable to set RX transfer timeout with status %d\n",
                    status);
            skiq_exit();
            return (-1);
        }
    }

    // set the sample rate
    status = skiq_write_rx_sample_rate_and_bandwidth(card, skiq_rx_hdl_A1,
                sample_rate, sample_rate);
    if( status != 0 )
    {
        printf("Error: unable to write Rx sample rate (result code %" PRIi32
                ")\n", status);
        skiq_exit();
        return (-4);
    }

    // set the frequency to some default value
    status = SET_RX_LO_FREQ(card, skiq_rx_hdl_A1, curr_freq);
    if( status != 0 )
    {
        printf("Error: unable to write Rx LO frequency (result code %" PRIi32
                ")\n", status);
        skiq_exit();
        return (-3);
    }

    // set counter mode for easier debug if it breaks
    status = skiq_write_rx_data_src(card, skiq_rx_hdl_A1,
                skiq_data_src_counter);
    if( status != 0 )
    {
        printf("Error: unable to set counter mode (result code %" PRIi32 ")\n",
                status);
        skiq_exit();
        return (-5);
    }

    // tune to the "end" frequency to start without metrics
    SET_RX_LO_FREQ(card, skiq_rx_hdl_A1, stop_freq);

    printf("Starting sweep\n");
    elapsed_start(&app_time);

    // sweep the frequency range the # of times specified
    for( curr_iteration=0;
         (curr_iteration <= repeat) && (running==true);
         curr_iteration++ )
    {
        while( (curr_freq <= stop_freq) && (running==true) )
        {
            status = SET_RX_LO_FREQ(card, skiq_rx_hdl_A1, curr_freq);
            if ( 0 != status )
            {
                printf("Error: failed to change RX LO frequency to %" PRIu64 " Hz (result code %"
                       PRIi32 ")\n", curr_freq, status);
                skiq_exit();
                return (-6);
            }

            // start streaming
            elapsed_start(&capture_time);
            status = START_STREAM(card, skiq_rx_hdl_A1);
            if ( 0 != status )
            {
                printf("Error: failed to start RX streaming (result code %"
                        PRIi32 ")\n", status);
                skiq_exit();
                return (-6);
            }

            // grab some data
            status = receive_data( num_blocks );
            if ( 0 != status )
            {
                num_receive_errors++;            }

            // stop streaming to ensure we don't get any "stale" data after
            // retuning
            status = STOP_STREAM(card, skiq_rx_hdl_A1);
            elapsed_end(&capture_time);
            if ( 0 != status )
            {
                printf("Error: failed to stop RX streaming (result code %"
                        PRIi32 ")\n", status);
                skiq_exit();
                return (-6);
            }

            curr_freq += step_size;
        }

        // print out a status
        if( ((curr_iteration % 500) == 0) && (curr_iteration != 0) )
        {
            printf("Completed %u iterations, current tune average ", curr_iteration);
            print_average(&tune_time);
        }

        curr_freq = start_freq;
    }
    elapsed_end(&app_time);

    printf("======================================================================\n");
    printf("            Total time for RX LO tuning: "),print_total(&tune_time);
    printf("          Total number of RX LO retunes: "),print_nr_calls(&tune_time);
    printf(" Minimum time for a single RX LO retune: "),print_minimum(&tune_time);
    printf(" Average time for a single RX LO retune: "),print_average(&tune_time);
    printf(" Maximum time for a single RX LO retune: "),print_maximum(&tune_time);

    printf("======================================================================\n");
    printf("        Total time for starting streaming: "),print_total(&start_stream_time);
    printf(" Total number of starting streaming calls: "),print_nr_calls(&start_stream_time);
    printf("   Minimum time for a single start stream: "),print_minimum(&start_stream_time);
    printf("   Average time for a single start stream: "),print_average(&start_stream_time);
    printf("   Maximum time for a single start stream: "),print_maximum(&start_stream_time);

    printf("======================================================================\n");
    printf("       Total time for stopping streaming: "),print_total(&stop_stream_time);
    printf(" Total number of stoping streaming calls: "),print_nr_calls(&stop_stream_time);
    printf("   Minimum time for a single stop stream: "),print_minimum(&stop_stream_time);
    printf("   Average time for a single stop stream: "),print_average(&stop_stream_time);
    printf("   Maximum time for a single stop stream: "),print_maximum(&stop_stream_time);

    printf("======================================================================\n");
    printf("         Total time for capturing samples: "),print_total(&capture_time);
    printf("Total number of capturing sample sessions: "),print_nr_calls(&capture_time);
    printf("       Minimum time for a capture session: "),print_minimum(&capture_time);
    printf("       Average time for a capture session: "),print_average(&capture_time);
    printf("       Maximum time for a capture session: "),print_maximum(&capture_time);

    // determine the total run time of the sweep across multiple iterations
    printf("Application run time is %3" PRId64 ".%09lu seconds, number of sweeps is %u (%"
           PRIu64 " Hz - %" PRIu64 " Hz), number of receive errors %" PRIu32 "\n",
           (uint64_t)app_time.total.tv_sec, app_time.total.tv_nsec,
           curr_iteration, start_freq, stop_freq, num_receive_errors);

    skiq_exit();

    return (0);
}

/*****************************************************************************/
/** This function receives data and verifies the timestamp increment.

    @param num_blocks number of blocks to receive
    @return int-0 if received successfully, negative if there was an error
*/
static int32_t receive_data( uint32_t num_rx_blocks )
{
    uint32_t curr_num_blocks=0;
    skiq_rx_hdl_t hdl;
    uint32_t len=0;
    uint64_t curr_ts=0;
    uint64_t next_ts=0;
    int32_t status=0;
    skiq_rx_status_t rx_status;
    skiq_rx_block_t *p_rx_block;
    uint32_t ts_offset = \
        SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS-SKIQ_RX_HEADER_SIZE_IN_WORDS;

    // receive the number of blocks requested per iteration
    while( (curr_num_blocks<num_rx_blocks) && (running==true) )
    {
        rx_status = skiq_receive(card, &hdl, &p_rx_block, &len);
        if ( rx_status == skiq_rx_status_success )
        {
            // Ensure that the handle is correct
            if( hdl != skiq_rx_hdl_A1 )
            {
                printf("Error: invalid handle %u returned at block %u\n", (int) hdl,
                       curr_num_blocks);
                print_block_contents(p_rx_block, MIN(len, 60));
                skiq_exit();
                _exit(-1);
            }

            // Ensure the correct length was returned
            if( len != SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES )
            {
                printf("Error: wrong data length of %u received (expected %" PRIu32 ") at block "
                       "%u\n", len, (uint32_t) SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES, curr_num_blocks);
                print_block_contents(p_rx_block, MIN(SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES, 60) );
                skiq_exit();
                _exit(-2);
            }

            // Ensure that the timestamp is non-zero
            curr_ts = p_rx_block->rf_timestamp;

            if( curr_ts == 0 )
            {
                printf("Error: rx timestamp is 0 (%" PRIu64 ", %" PRIu64 ") at block %u\n",
                       p_rx_block->rf_timestamp, curr_ts, curr_num_blocks);
                print_block_contents(p_rx_block, MIN(len, 60));
                skiq_exit();
                _exit(-3);
            }

            // look at the RF pair timestamp to make sure we didn't drop data
            if( (curr_num_blocks != 0) && (curr_ts != next_ts) )
            {
                printf("Error: timestamp error in block %d....expected"
                        " 0x%016" PRIx64 " but got 0x%016" PRIx64
                        "\n", curr_num_blocks, next_ts, curr_ts);
                status = -1;
                return (status);
            }
            next_ts = curr_ts + ts_offset;
            curr_num_blocks++;
        }
        else if ( rx_status == skiq_rx_status_error_overrun )
        {
            printf("Warning: overrun detected on block %" PRIu32 " of %"
                    PRIu32 " (result code %" PRIi32 "); continuing.\n",
                    curr_num_blocks, num_rx_blocks, (int32_t) rx_status);
        }
        else if ( rx_status == skiq_rx_status_error_generic )
        {
            printf("Warning: possible RX error detected on block %" PRIu32
                    " of %" PRIu32 " (result code %" PRIi32 "); continuing.\n",
                    curr_num_blocks, num_rx_blocks, (int32_t) rx_status);
        }
        else if ( rx_status == skiq_rx_status_error_packet_malformed )
        {
            printf("Error: failed to receive data on block %" PRIu32 " of %"
                    PRIu32 " (result code %" PRIi32 ")\n", curr_num_blocks,
                    num_rx_blocks, (int32_t) rx_status);
            skiq_exit();
            _exit(-4);
        }
        else if ( ( rx_status != skiq_rx_status_no_data ) && ( rx_status != skiq_rx_status_error_not_streaming ) )
        {
            printf("Warning: unknown error detected on block %" PRIu32
                    " of %" PRIu32 " (result code %" PRIi32 "); continuing.\n",
                    curr_num_blocks, num_rx_blocks, (int32_t) rx_status);
        }
    }

    return (status);
}


/*****************************************************************************/
/** This function prints contents of raw data

    @param p_data: a reference to raw bytes
    @param length: the number of bytes references by p_data
    @return: void
*/
static void hex_dump(uint8_t* p_data, uint32_t length)
{
    unsigned int i, j;
    uint8_t c;

    for (i = 0; i < length; i += 16)
    {
        /* print offset */
        printf("%06X:", i);

        /* print HEX */
        for (j = 0; j < 16; j++)
        {
            if ( ( j % 2 ) == 0 ) printf(" ");
            if ( ( j % 8 ) == 0 ) printf(" ");
            if ( i + j < length )
                printf("%02X", p_data[i + j]);
            else
                printf("  ");
        }
        printf("    ");

        /* print ASCII (if printable) */
        for (j = 0; j < 16; j++)
        {
            if ( ( j % 8 ) == 0 ) printf(" ");
            if ( i + j < length )
            {
                c = p_data[i + j];
                if ( 0 == isprint(c) )
                    printf(".");
                else
                    printf("%c", c);
            }
        }
        printf("\n");
    }
}


/*****************************************************************************/
/** This function prints contents of a receive block

    @param p_block: a reference to the receive block
    @param num_samples: the # of 32-bit samples to print
    @return: void
*/
static void print_block_contents( skiq_rx_block_t* p_block,
                                  int32_t block_size_in_bytes )
{
    printf("    RF Timestamp: %20" PRIu64 " (0x%016" PRIx64 ")\n",
            p_block->rf_timestamp, p_block->rf_timestamp);
    printf("System Timestamp: %20" PRIu64 " (0x%016" PRIx64 ")\n",
            p_block->sys_timestamp, p_block->sys_timestamp);
    printf(" System Metadata: %20u (0x%06" PRIx32 ")\n",
            p_block->system_meta, p_block->system_meta);
    printf("    RFIC Control: %20u (0x%04" PRIx32 ")\n",
            p_block->rfic_control, p_block->rfic_control);
    printf("     RF Overload: %20u\n", p_block->overload);
    printf("       RX Handle: %20u\n", p_block->hdl);
    printf("   User Metadata: %20u (0x%08" PRIx32 ")\n",
            p_block->user_meta, p_block->user_meta);

    printf("Header:\n");
    hex_dump( (uint8_t*)p_block, SKIQ_RX_HEADER_SIZE_IN_BYTES );

    printf("Samples:\n");
    hex_dump( (uint8_t*)p_block->data,
               block_size_in_bytes - SKIQ_RX_HEADER_SIZE_IN_BYTES );
}
