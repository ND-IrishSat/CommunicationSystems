/*! \file app_src_file1.c
 *
 *  \brief This file contains the skeleton of an example software application that
 *         utilizes libsidekiq.  It performs a simple configuration of an Rx interface,
 *         and proceeds to read the user-specifed # of I/Q samples from the Rx interface.
 *
 *  For more information about developing with Sidekiq, please refer to the Sidekiq API
 *  documentation and Sidekiq Software Development Manual which are included with the
 *  libsidekiq SDK.
 *
 * <pre>
 * Copyright 2013 - 2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sidekiq_api.h>
#include <arg_parser.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_NUM_BLOCKS
#   define DEFAULT_NUM_BLOCKS       10000
#endif

#ifndef DEFAULT_FREQUENCY_HZ
#   define DEFAULT_FREQUENCY_HZ     850000000
#endif


/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- receive data";
static const char* p_help_long = "\
Configure the Rx interface to its default parameters, and set the Rx LO\n\
frequency to the user-specified value. Once configured, loop through and\n\
read the requested number of I/Q blocks from the Rx interface. In this\n\
example app, the received samples are discarded (though the timestamps are\n\
verified to determine if samples were lost); normal applications would\n\
buffer or process sample blocks after receiving them.\n\
\n\
Defaults:\n\
  --blocks=" xstr(DEFAULT_NUM_BLOCKS) "\n\
  --frequency=" xstr(DEFAULT_FREQUENCY_HZ);

/* storage for all cmd line args */
/*
    this variable stores the initialization level for libsidekiq:
        skiq_xport_init_level_basic - access to basic card functions such as reprogramming and
                                      some information about the Sidekiq card; no RX or TX
                                      capability
        skiq_xport_init_level_full  - full access to the Sidekiq card, including RX & TX capability
                                      (if present)
*/
static skiq_xport_init_level_t level = skiq_xport_init_level_full;

/* this is used to specify which RF handle we want to use for Rx */
static skiq_rx_hdl_t hdl = skiq_rx_hdl_A1;
/* frequency in Hz to tune the local oscillator to */
static uint64_t lo_freq = DEFAULT_FREQUENCY_HZ;
/* the number of Rx blocks of data to acquire */
static uint32_t num_blocks_to_acquire = DEFAULT_NUM_BLOCKS;
/* boolean flag used to force the application to end */
static bool running = true;

/* the command line arguments available with this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("blocks",
                0,
                "Number of samples blocks to collect",
                "N",
                &num_blocks_to_acquire,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to tune to",
                "HZ",
                &lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

/* local functions */
static void app_cleanup(int signum);
static void critical_err_handler( int32_t status, void* user_data );
static void logging_handler( int32_t priority, const char *message );

/******************************************************************************/
/** This is the custom critical error handler.  If there were custom handling
    required to properly shutdown the application, it should be handled here.
    Any future call to libsidekiq after a critical error may result in
    undefined behavior.

    @param signum: the signal number that occurred
    @return void
*/
void critical_err_handler( int32_t status, void* user_data )
{
    printf("A critical error of %" PRIi32 " was encountered, must exit\n", status);

    _exit(-1);
}

/******************************************************************************/
/** This is the custom logging handler.  If there were custom handling
    required for logging messages, it should be handled here.

    @param signum: the signal number that occurred
    @return void
*/
void logging_handler( int32_t priority, const char *message )
{
    printf("<PRIORITY %" PRIi32 "> custom logger: %s", priority, message);
}

/******************************************************************************/
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

/******************************************************************************/
/** This is the main function for executing the app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    int32_t status = 0;
    int32_t tmp_status = 0;
    uint64_t curr_rx_ts = 0;     /* current RF pair timestamp    */
    uint64_t next_rx_ts = 0;     /* next RF pair timestamp       */
    uint64_t curr_sys_ts = 0;    /* current system timestamp     */

    uint32_t curr_block = 0;
    /*
        The handle that I/Q data was received on; the initialization
        value doesn't matter as this will be filled in by `skiq_receive()`
        later on.
    */
    skiq_rx_hdl_t rcvd_hdl = skiq_rx_hdl_end;
    skiq_rx_block_t *p_rx_block;
    uint32_t data_len = 0;

    /*
        Interface with the first Sidekiq card (card 0); a system can support up
        to SKIQ_MAX_NUM_CARDS
    */
    uint8_t sidekiq_card = 0;
    uint8_t num_cards = 1;      /* interfacing with a single Sidekiq card */

    /* Flag to indicate if libsidekiq was successfully initialized */
    bool skiq_initialized = false;

    /* Sample rate & bandwidth settings for the Sidekiq card */
    uint32_t sample_rate_hz = 5000000;
    /*
        Set the bandwidth to 80% of the sample rate (the recommended maximum
        value for most Sidekiq radios).
    */
    uint32_t bandwidth_hz = (uint32_t) (sample_rate_hz * 0.80);

    /*
        Used when checking if a card is already in use (this will be set to the process ID
        of the current owner of the card)
    */
    pid_t owner = 0;

    /* Always install a signal handler to ensure proper cleanup if the user hits Ctrl-C */
    signal(SIGINT, app_cleanup);

    /* Parse the command line arguments */
    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    /* Register our own logging function before initializing the library */
    skiq_register_logging( logging_handler );

    /* Initialize libsidekiq */
    printf("Info: initializing libsidekiq...\n");

    status = skiq_init_without_cards();
    if ( status != 0 )
    {
        fprintf(stderr, "Error: failed to initialize libsidekiq (status = %" PRIi32 ")\n", status);
        return (-2);
    }
    skiq_initialized = true;

    /*
        Enable the specified Sidekiq card - this process claims the Sidekiq so that other
        applications cannot use it and then initializes it
    */
    printf("Info: initializing card %" PRIu8 "...\n", sidekiq_card);

    status = skiq_enable_cards(&sidekiq_card, num_cards, level);
    if( status != 0 )
    {
        if ( ( EBUSY == status ) &&
             ( 0 != skiq_is_card_avail(sidekiq_card, &owner) ) )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is already in use (by process ID"
                    " %u); cannot initialize card.\n", sidekiq_card,
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

        /*
            Override the status code just to provide the calling application some idea of why
            this program shut down
        */
        status = -3;
        goto finished;
    }

    /*
        Register our critical event handler to intercept libsidekiq errors that
        require immediate action
    */
    skiq_register_critical_error_callback(critical_err_handler, NULL);

    /* Configure the frequency and sample rate of the specified Rx handle */
    status = skiq_write_rx_sample_rate_and_bandwidth(sidekiq_card, hdl, sample_rate_hz,
                bandwidth_hz);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to set sample rate (%" PRIu32 " Hz) and/or bandwidth (%"
            PRIu32 " Hz) (status %" PRIi32 ")\n", sample_rate_hz, bandwidth_hz, status);
        status = -4;
        goto finished;
    }

    /*
        Configure the LO frequency (frequency the card is tuned to) for the specified card and
        Rx handle
    */
    status = skiq_write_rx_LO_freq(sidekiq_card, hdl, lo_freq);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to set the frequency to %" PRIu64 " Hz (status %" PRIi32
            ")\n", lo_freq, status);
        status = -5;
        goto finished;
    }

    /*
        Tell the receiver to start streaming samples to the host; these samples will be read
        into this program in the loop below
    */
    status = skiq_start_rx_streaming(sidekiq_card, hdl);
    if( status != 0 )
    {
        fprintf(stderr, "Error: failed to starting streaming samples, status %" PRIi32 "\n",
            status);
        status = -6;
        goto finished;
    }

    while( (curr_block < num_blocks_to_acquire) && (running==true) )
    {
        /* Receive a packet of sample data */
        status = skiq_receive(sidekiq_card, &rcvd_hdl, &p_rx_block, &data_len);
        if( status == 0 )
        {
            /*
                Don't do anything unless it was a valid status and the handle
                indicates the data is from the Rx interface we're interested in
                (`skiq_receive()` returns the handle that the sample data was
                received on)
            */
            if( rcvd_hdl == hdl )
            {
                /* look at the RF pair timestamp to make sure we didn't drop
                   data */
                curr_rx_ts = p_rx_block->rf_timestamp;
                if( (curr_block != 0) && (curr_rx_ts != next_rx_ts) )
                {
                    fprintf(stderr, "Error: timestamp error in block %" PRIu32 "....expected 0x%"
                            PRIx64 " but got 0x%" PRIx64 "\n",
                           curr_block, next_rx_ts, curr_rx_ts);
                    running = false;
                    continue;
                }

                /* set our next expected Rx timestamp...the Rx timestamp
                   increments relative to the sample rate */
                next_rx_ts = curr_rx_ts +
                    SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS -
                    SKIQ_RX_HEADER_SIZE_IN_WORDS;

                /* grab the system timestamp too...the rate at which this
                   increments is independent of the sample rate */
                curr_sys_ts = p_rx_block->sys_timestamp;

                /* This is where the data would either be processed in place or
                   copied to another buffer for subsequent processing tasks.
                   Since we're not doing anything in this example app, we'll
                   just try to receive the next packet */
                curr_block++;
            }
        }
        else if ( status != skiq_rx_status_no_data )
        {
            /*
                Failed to read the sample data - see the values for skiq_rx_status_t
                to understand the failure code.

                `skiq_rx_status_no_data` is the exception as it indicates that the
                card timed out waiting for sample data, which can be completely
                normal when polling the card at slower sample rates.
            */

            fprintf(stderr, "Error: failed to read samples from card %" PRIu8
                " (status = %" PRIi32 ")\n", sidekiq_card, status);
            running = false;
            continue;
        }
    }

    if( status != 0 )
    {
        /*
            It seems there was a failure while reading sample data... override the return
            status code just to provide the calling application some idea of why this
            program shut down
        */
        status = -7;
    }

    /* If the current system timestamp is 0, then no valid system timestamp value was read */
    if( curr_sys_ts != 0 )
    {
        printf("Info: last read system timestamp was %" PRIu64 "\n", curr_sys_ts);
        printf("Info: last read RF     timestamp was %" PRIu64 "\n", curr_rx_ts);
    }

    if( status == 0 )
    {
        printf("Info: completed successfully!\n");
    }
    else
    {
        printf("Info: finished with error(s)!\n");
    }

    /* No more need for the radio, so start shutting down... */

    /* Tell the receiver to stop streaming sample data */
    printf("Info: stopping RX sample streaming...\n");

    tmp_status = skiq_stop_rx_streaming(sidekiq_card, hdl);
    if ( tmp_status != 0 )
    {
        printf("Warning: failed to stop streaming (status = %" PRIi32 "); continuing...\n",
            tmp_status);
    }

    /* Disable (release) the card so that another application may use it */
    printf("Info: releasing card %" PRIu8 "...\n", sidekiq_card);

    tmp_status = skiq_disable_cards(&sidekiq_card, num_cards);
    if( tmp_status != 0 )
    {
        printf("Warning: failed to disable card(s) (status = %" PRIi32 "); should be resolved"
            " with skiq_exit() call but possible resource leak...\n", tmp_status);
    }

finished:
    if (skiq_initialized)
    {
        /* Uninitialize (shutdown) libsidekiq to clean up resources */
        printf("Info: shutting down libsidekiq...\n");

        skiq_exit();
    }

    return (status);
}

