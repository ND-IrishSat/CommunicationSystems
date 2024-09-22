/*! \file tx_samples_async.c
 * \brief This file contains a basic application for transmiting
 * sample data using the asynchronous transmit mode.
 *
 * <pre>
 * Copyright 2012 - 2019 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#include <sidekiq_api.h>
#include <arg_parser.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

#ifndef DEFAULT_TIMESTAMP_BASE
#   define DEFAULT_TIMESTAMP_BASE   "rf"
#endif

#ifndef DEFAULT_TIMESTAMP_VALUE
#   define DEFAULT_TIMESTAMP_VALUE  (100000)
#endif

#ifndef PYTEST_EVENT
#   define PYTEST_EVENT  "test Tx handles"
#endif
/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- transmit I/Q data, using async mode";
static const char* p_help_long =
"\
Configure the Tx lineup according to the specified parameters and transmit\n\
the entire contents of a provided file. The file should contain 16-bit\n\
signed twos-complement little-endian I/Q samples formatted as follows:\n\
\n\
    <16-bit Q0> <16-bit I0> <16-bit Q1> <16-bit I1> ... etc\n\
\n\
Note that unless an initial timestamp is provided, Sidekiq will transmit in\n\
asynchronous mode. This will cause the FPGA to begin transmission as soon as\n\
it obtains new I/Q samples. If a timestamp is provided, Sidekiq will run in\n\
timestamp mode with the FPGA starting I/Q transmission once the given \n\
timestamp has been reached. The application will automatically increment\n\
the timestamp value and add it to the I/Q data as it is being transmitted\n\
such that there are no gaps in transmission. The selection of an initial\n\
timestamp value depends upon a given system, but generally any value on the\n\
order of 100000 has been known to work.\n\
\n\
The '--late' option can be used to enable support for transmitting data with\n\
late timestamps (when using bitfiles that support this feature); this feature\n\
can be enabled standalone or with the '--timestamp' option.\n\
\n\
Defaults:\n\
  --attenuation=100\n\
  --block-size=1020\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --frequency=850000000\n\
  --handle=A1\n\
  --rate=1000000\n\
  --timestamp-base=" xstr(DEFAULT_TIMESTAMP_BASE) "\n\
  --repeat=0\n\
  --cal-mode=auto\n\
  --force-cal=false\n\
  --threads=4\n\
  --priority=-1";

/* variables used for all command line arguments */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static uint16_t attenuation = 100;
static uint16_t act_attenuation = 0;
static uint16_t block_size_in_words = 1020;
static uint64_t lo_freq = 850000000;
static uint32_t sample_rate = 1000000;
static uint32_t bandwidth = 0;
static bool bandwidth_present = 0;
static uint64_t timestamp = 0;
static int32_t repeat = 0;
static char* p_file_path = NULL;
static char* p_hdl = "A1";
static char* p_timestamp_base = DEFAULT_TIMESTAMP_BASE;
static bool immediate_mode = false;
static bool packed = false;
static bool iq_swap = false;
static skiq_iq_order_t  iq_order_mode = skiq_iq_order_qi;
static uint8_t num_threads = 4;
static int32_t priority = -1;

/* application variables  */
static skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
static skiq_tx_hdl_t hdl = skiq_tx_hdl_A1;
static skiq_tx_timestamp_base_t timestamp_base = skiq_tx_rf_timestamp;
static FILE *input_fp = NULL;
static skiq_tx_block_t **p_tx_blocks = NULL; /* reference to an array of transmit block references */
static uint8_t mult_factor = 1;
static uint32_t num_blocks = 0;
static bool running = true;
static bool late_timestamps = false;
static bool test_tx_handles = false;
static char *p_cal_mode = "auto";
static skiq_tx_quadcal_mode_t cal_mode = skiq_tx_quadcal_mode_auto;
static bool force_cal = false;
static char* p_rfic_file_path = NULL;
static uint32_t complete_count=0;    // counter for number of packets completed
int32_t *p_tx_status = NULL;

// mutex to protect updates to the tx buffer
pthread_mutex_t tx_buf_mutex = PTHREAD_MUTEX_INITIALIZER;
// mutex and condition variable to signal when the tx queue may have room available
pthread_mutex_t space_avail_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t space_avail_cond = PTHREAD_COND_INITIALIZER;


/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("attenuation",
                'a',
                "Output attenuation in quarter dB steps",
                "dB",
                &attenuation,
                UINT16_VAR_TYPE),
    APP_ARG_OPT_PRESENT("bandwidth",
                'b',
                "Bandwidth in Hertz",
                "Hz",
                &bandwidth,
                UINT32_VAR_TYPE, 
                &bandwidth_present),
    APP_ARG_OPT("block-size",
                0,
                "Number of samples to transmit per block",
                "N",
                &block_size_in_words,
                UINT16_VAR_TYPE),
    APP_ARG_OPT("card",
                'c',
                "Specify Sidekiq by card index",
                "ID",
                &card,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to transmit samples at in Hertz",
                "Hz",
                &lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("handle",
                0,
                "Tx handle to use, either A1 or A2 (or B1 if available)",
                "Tx",
                &p_hdl,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("timestamp-base",
                0,
                "Timestamps based on rf or system free running clock, either 'rf' or 'system'",
                NULL,
                &p_timestamp_base,
                STRING_VAR_TYPE),
    APP_ARG_OPT("repeat",
                0,
                "Transmit the file N additional times",
                "N",
                &repeat,
                INT32_VAR_TYPE),
    APP_ARG_REQ("source",
                's',
                "Input file to source for I/Q data",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_OPT("timestamp",
                't',
                "Initial timestamp value",
                "N",
                &timestamp,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("immediate",
                0,
                "Ignore timestamps and transmit as soon as data is received",
                NULL,
                &immediate_mode,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("packed",
                0,
                "Transmit packed mode data",
                NULL,
                &packed,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("late",
                'l',
                "Attempt to use late timestamps",
                NULL,
                &late_timestamps,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("cal-mode",
                0,
                "Calibration mode, either auto or manual",
                NULL,
                &p_cal_mode,
                STRING_VAR_TYPE),
    APP_ARG_OPT("force-cal",
                0,
                "Force calibration to run",
                NULL,
                &force_cal,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("rfic-config",
                0,
                "Input filename of RFIC configuration",
                NULL,
                &p_rfic_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("sample-order-iq",
                0,
                "Configure sample ordering iq",
                NULL,
                &iq_swap,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("threads",
                0,
                "Transmit asynchronously using 'N' threads",
                "N",
                &num_threads,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("priority",
                0,
                "Thread priority of asynchronous TX threads",
                "p",
                &priority,
                INT32_VAR_TYPE),
    APP_ARG_OPT("pytest",
                0,
                "Pytest Tx handles",
                NULL,
                &test_tx_handles,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,

};

/* local functions */
static int32_t init_tx_buffer(void);

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);
    running=false;
}

/*****************************************************************************/
/** This is the callback function for once the data has completed being sent.
    There is no guarantee that the complete callback will be in the order that
    the data was sent, this function just increments the completion count and
    signals the main thread that there is space available to send more packets.

    @param status status of the transmit packet completed
    @param p_block reference to the completed transmit block
    @param p_user reference to the user data
    @return void
*/
void tx_complete( int32_t status, skiq_tx_block_t *p_data, void *p_user )
{
    if( status != 0 )
    {
        fprintf(stderr, "Error: packet %" PRIu32 " failed with status %d\n",
                complete_count, status);
    }

    // increment the packet completed count
    complete_count++;

    pthread_mutex_lock( &tx_buf_mutex );
    // update the in use status of the packet just completed
    if (p_user)
    {
        *(int32_t*)p_user = 0;
    }
     pthread_mutex_unlock( &tx_buf_mutex );

    // signal to the other thread that there may be space available now that a
    // packet send has completed
    pthread_mutex_lock( &space_avail_mutex );
    pthread_cond_signal(&space_avail_cond);
    pthread_mutex_unlock( &space_avail_mutex );

}


/*****************************************************************************/
/** This is the main function for executing the tx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ASCII string arguments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    skiq_xport_init_level_t level = skiq_xport_init_level_full;
    skiq_tx_flow_mode_t tx_mode;
    skiq_tx_hdl_t hdl_other = skiq_tx_hdl_end;
    uint32_t curr_block=0;
    int32_t status=0;
    uint32_t errors=0;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint64_t min_lo_freq, max_lo_freq;
    uint32_t timestamp_increment=0;
    pid_t owner = 0;
    bool skiq_initialized = false;
    FILE *p_rfic_file = NULL;
    uint32_t send_count = 0;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize everything based on the arguments provided */
    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -1;
        goto cleanup;
    }

    /* determine the tx transfer mode */
    if( num_threads <= 1 )
    {
        fprintf(stderr, "Error: threads are expected to be > 1.  Please"
                "use tx_samples if skiq_tx_transfer_mode_sync is desired.\n");
    }

    if ( (0 != timestamp) && (false != immediate_mode) )
    {
        fprintf(stderr, "Error: cannot set both timestamp and immediate"
                " mode.\n");
        status = -1;
        goto cleanup;
    }

    if (iq_swap == true)
    {
        iq_order_mode = skiq_iq_order_iq;
    }

    if( (UINT8_MAX != card) && (NULL != p_serial) )
    {
        fprintf(stderr, "Error: must specify EITHER card ID or serial number, not"
                " both\n");
        status = -1;
        goto cleanup;
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
            fprintf(stderr, "Error: cannot find card with serial number %s"
                    " (status %" PRIi32 ")\n", p_serial, status);
            status = -1;
            goto cleanup;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    /* map argument values to Sidekiq specific variable values */
    if( 0 == strcasecmp(p_hdl, "A1") )
    {
        hdl = skiq_tx_hdl_A1;
        /* set chan mode to single */
        chan_mode = skiq_chan_mode_single;
        mult_factor = 1;
        printf("Info: using Tx handle A1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "A2") )
    {
        hdl = skiq_tx_hdl_A2;
        /* set chan mode to dual */
        chan_mode = skiq_chan_mode_dual;
        hdl_other = skiq_tx_hdl_A1;
        mult_factor = 2;
        printf("Info: using Tx handle A1 and A2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B1") )
    {
        hdl = skiq_tx_hdl_B1;
        /* set chan mode to dual */
        chan_mode = skiq_chan_mode_dual;
        hdl_other = skiq_tx_hdl_A1;
        mult_factor = 2;
        printf("Info: using Tx handle A1 and B1\n");
    }
    else
    {
        fprintf(stderr, "Error: invalid handle specified\n");
        status = -1;
        goto cleanup;
    }

    if((0 == strncasecmp(p_timestamp_base, "rf", 3)) ||
       (0 == strncasecmp(p_timestamp_base, "'rf'", 5)))
    {
        timestamp_base = skiq_tx_rf_timestamp;
        printf("Info: using RF free running clock for transmit timestamp base\n");
    }
    else if((0 == strncasecmp(p_timestamp_base, "system", 7)) ||
            (0 == strncasecmp(p_timestamp_base, "'system'", 9)))
    {
        timestamp_base = skiq_tx_system_timestamp;
        printf("Info: using system free running clock for transmit timestamp base\n");
    }
    else
    {
        fprintf(stderr, "Error: invalid free running clock '%s' specified\n", p_timestamp_base);
        status = -EINVAL;
        goto cleanup;
    }

    if( 0 == strcasecmp(p_cal_mode, "auto") )
    {
        cal_mode = skiq_tx_quadcal_mode_auto;
    }
    else if( 0 == strcasecmp(p_cal_mode, "manual") )
    {
        cal_mode = skiq_tx_quadcal_mode_manual;
    }
    else
    {
        fprintf(stderr, "Error: invalid calibration mode\n");
        status = -1;
        goto cleanup;
    }

    if( late_timestamps )
    {
        tx_mode = skiq_tx_with_timestamps_allow_late_data_flow_mode;
        if( timestamp == 0 )
        {
            timestamp = DEFAULT_TIMESTAMP_VALUE;
            printf("Info: no timestamp value specified with late mode; using"
                    " default value of %" PRIu64 "\n", timestamp);
        }
    }
    else if( timestamp != 0 )
    {
        tx_mode = skiq_tx_with_timestamps_data_flow_mode;
    }
    else
    {
        tx_mode = skiq_tx_immediate_data_flow_mode;
    }

    /* if the user did not set bandwidth, then set it to the sample rate */
    if( !bandwidth_present  )
    {
        bandwidth = sample_rate;
    }

    errno = 0;
    input_fp = fopen(p_file_path, "rb");
    if( input_fp == NULL )
    {
        fprintf(stderr, "Error: unable to open input file %s, errno %d\n", p_file_path, errno);
        status = errno;
        goto cleanup;
    }

    // initialize the transmit buffer
    status = init_tx_buffer();
    if (0 != status)
    {
        status = -1;
        goto cleanup;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    status = skiq_init(skiq_xport_type_auto, level, &card, 1);
    if( status != 0 )
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
                    " valid card specified? (status %" PRIi32 ")\n",
                    status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with"
                    " status %" PRIi32 "\n", status);
        }
        goto cleanup;
    }
    skiq_initialized = true;

    if( p_rfic_file_path != NULL )
    {
        errno = 0;
        p_rfic_file = fopen(p_rfic_file_path, "r");
        if( p_rfic_file == NULL )
        {
            fprintf(stderr, "Error: unable to open specified RFIC configuration file %s (errno %d)\n",
                   p_rfic_file_path, errno);
            status = errno;
            goto cleanup;
        }
        else
        {
            printf("Info: configuring RFIC with configuration from %s\n", p_rfic_file_path);
            status = skiq_prog_rfic_from_file(p_rfic_file,card);
        }
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to program RFIC from file with error %" PRIi32 "\n", status);
            goto cleanup;
        }
    }

    status = skiq_write_iq_order_mode(card, iq_order_mode);
    if (0 != status)
    {
        fprintf(stderr, "Error: failed to set iq_order_mode on"
                " card %" PRIu8 "  with status %"
                PRIi32 "\n", card, status);
        goto cleanup;
    }

    // configure the calibration mode
    status = skiq_write_tx_quadcal_mode( card, hdl, cal_mode );
    if ( 0 != status )
    {
        fprintf(stderr, "Error: unable to configure quadcal mode (status =  %" PRIi32 ")\n", status);
        goto cleanup;
    }

    /* set the transmit quadcal mode on the other associated handle when using dual channel mode
     * (i.e. other handle is valid) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_quadcal_mode( card, hdl_other, cal_mode );
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure quadcal mode (status =  %" PRIi32 ")\n", status);
            goto cleanup;
        }
    }

    status = skiq_read_tx_LO_freq_range( card, &max_lo_freq, &min_lo_freq );
    if ( 0 == status )
    {
        printf("Info: tunable TX LO frequency range = %" PRIu64
                "Hz to %" PRIu64 "Hz\n", min_lo_freq, max_lo_freq);

        /* validate user input frequency range */
        if ((lo_freq < min_lo_freq) || (lo_freq > max_lo_freq))
        {
            fprintf(stderr, "Error: User entered LO Frequency is out of bounds %" PRIi64 "Hz \n", lo_freq);
            goto cleanup;
        }
    }
    else
    {
        printf("Warning: failed to read TX LO frequency range (status "
                " %" PRIi32 ")\n", status);
    }


    /* set the channel mode */
    status = skiq_write_chan_mode(card, chan_mode);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to set channel mode (status %"
                PRIi32 ")\n", status);
        goto cleanup;
    }

    // configure our Tx parameters
    if( p_rfic_file == NULL )
    {
        status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl, sample_rate,
                                                         bandwidth);
        if ( status != 0 )
        {
            fprintf(stderr, "Warning: unable to configure Tx sample rate (status "
                    " %" PRIi32 ")\n", status);
        }

        /* set the transmit quadcal mode on the other associated handle when using dual channel mode
         * (i.e. other handle is valid) */
        if ( hdl_other != skiq_tx_hdl_end )
        {
            status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl_other, sample_rate,
                                                             bandwidth);
            if ( status != 0 )
            {
                fprintf(stderr, "Warning: unable to configure Tx sample rate (status"
                        " %" PRIi32 ")\n", status);
            }
        }
    }
    else
    {
        printf("Info: RFIC configuration provided, skipping sample rate / bandwidth configuration\n");
        fclose( p_rfic_file );
        p_rfic_file = NULL;
    }

    status = skiq_read_tx_sample_rate_and_bandwidth(card, hdl,
                &read_sample_rate, &actual_sample_rate,
                &read_bandwidth, &actual_bandwidth);
    if ( status == 0 )
    {
        printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
               actual_sample_rate, actual_bandwidth);
    }
    else
    {
        printf("Warning: failed to read TX sample rate and bandwidth (status"
                " %" PRIi32 ")\n", status);
    }

    status = skiq_write_tx_LO_freq(card, hdl, lo_freq);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx LO frequency (status"
                " %" PRIi32 ")\n", status);
        goto cleanup;
    }

    /* set the transmit quadcal mode on the other associated handle when using dual channel mode
     * (i.e. other handle is valid) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_LO_freq(card, hdl_other, lo_freq);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx LO frequency (status"
                    " %" PRIi32 ")\n", status);
            goto cleanup;
        }
    }
    printf("Info: configured Tx LO freq to %" PRIu64 " Hz\n", lo_freq);

    status = skiq_write_tx_attenuation(card, hdl, attenuation);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx attenuation (status"
               " %" PRIi32 ")\n", status);
        goto cleanup;
    }
    status = skiq_read_tx_attenuation(card, hdl, &act_attenuation);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to read Tx attenuation (status"
               " %" PRIi32 ")\n", status);
        goto cleanup;
    }
    printf("Info: actual attenuation for first handle is %.2f dB, requested attenuation is %.2f dB\n",
           ((float)act_attenuation / 4.0), ((float)attenuation / 4.0));

    /* set the transmit attenuation on the other associated handle when using dual channel mode
     * (i.e. other handle is valid) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_attenuation(card, hdl_other, attenuation);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx attenuation on other hdl (status"
                    " %" PRIi32 ")\n", status);
            goto cleanup;
        }
        status = skiq_read_tx_attenuation(card, hdl_other, &act_attenuation);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to read Tx attenuation (status"
                   " %" PRIi32 ")\n", status);
            goto cleanup;
        }
        printf("Info: actual attenuation for second handle is %.2f dB, requested attenuation is %.2f dB\n",
               ((float)act_attenuation / 4.0), ((float)attenuation / 4.0));
    }

    if( force_cal == true )
    {
        printf("Info: forcing calibration to run\n");
        status = skiq_run_tx_quadcal( card, hdl );
        if( status != 0 )
        {
            fprintf(stderr, "Error: calibration failed to run properly (status= %" PRIi32 ")\n", status);
            goto cleanup;
        }

        /* force calibration on the other associated handle when using dual channel mode (i.e. other
         * handle is valid) */
        if ( hdl_other != skiq_tx_hdl_end )
        {
            status = skiq_run_tx_quadcal( card, hdl_other );
            if( status != 0 )
            {
                fprintf(stderr, "Error: calibration failed to run properly on other hdl (status= %" PRIi32 ")\n", status);
                goto cleanup;
            }
        }
    }

    status = skiq_write_tx_data_flow_mode(card, hdl,
                (skiq_tx_flow_mode_t)(tx_mode));
    if( status != 0 )
    {
        if( (-ENOTSUP == status) &&
            (skiq_tx_with_timestamps_allow_late_data_flow_mode == tx_mode) )
        {
            fprintf(stderr, "Error: the currently loaded bitfile doesn't"
                    " support late timestamp mode (status %" PRIi32
                    ")\n", status);
        }
        else
        {
            fprintf(stderr, "Error: unable to configure Tx data flow mode"
                    " (status %" PRIi32 ")\n", status);
        }
        goto cleanup;
    }
    if( tx_mode == skiq_tx_immediate_data_flow_mode )
    {
        printf("Info: Using immediate tx data flow mode\n");
    }
    else if( tx_mode == skiq_tx_with_timestamps_allow_late_data_flow_mode )
    {
        printf("Info: Using timestamps tx data flow mode (allowing late"
                " timestamps)\n");
    }
    else if( tx_mode == skiq_tx_with_timestamps_data_flow_mode )
    {
        printf("Info: Using timestamp tx data flow mode\n");
    }
    status=skiq_write_tx_block_size(card, hdl, block_size_in_words);

    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx block size (status"
               " %" PRIi32 ")\n", status);
        goto cleanup;
    }
    printf("Info: block size set to %u words\n", block_size_in_words);
    
    /* set the mode (packed or unpacked) */
    status=skiq_write_iq_pack_mode(card, packed);
    if( status != 0 )
    {
        if ( status == -ENOTSUP )
        {
            fprintf(stderr, "Error: packed mode is not supported on this Sidekiq product\n");
        }
        else
        {
            fprintf(stderr, "Error: unable to set the packed mode (status %" PRIi32 ")\n",
                    status);
        }
        goto cleanup;
    }
    else if( packed == true )
    {
        printf("Info: packed mode is enabled\n");
        timestamp_increment = \
            SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK(block_size_in_words);
    }
    else
    {
        printf("Info: packed mode is disabled\n");
        timestamp_increment = block_size_in_words;
    }

    if(( tx_mode == skiq_tx_with_timestamps_data_flow_mode ) ||
       ( tx_mode == skiq_tx_with_timestamps_allow_late_data_flow_mode ))
    {
        printf("Info:   initial timestamp is %" PRIu64 "\n", timestamp);
        printf("Info: timestamp increment is %u\n", timestamp_increment);
    }

    //Set the timestamp base if flow mode allows for transmit on timestamp
    if(tx_mode != skiq_tx_immediate_data_flow_mode)
    {
        status = skiq_write_tx_timestamp_base(card, timestamp_base);
        if(status != 0)
        {
            fprintf(stderr, "Error: unable to set timestamp base for TX on timestamp on card %" PRIu8
                            " (status %" PRIi32 ")\n", card, status);
            goto cleanup;
        }
    }

    status = skiq_write_tx_transfer_mode(card, hdl, skiq_tx_transfer_mode_async); 
    // set the transfer mode to async
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to set transfer mode to async"
                " (status = %" PRIi32 ")\n", status);
        goto cleanup;
    }

    /* specify the # of threads since we're running in async mode */
    if( skiq_write_num_tx_threads(card, num_threads) != 0 )
    {
        fprintf(stderr, "Error: unable to set # of tx threads\n");
        goto cleanup;
    }

    /* update priority if it's not the default value */
    if( priority != -1 )
    {
        printf("Info: setting priority to %d\n", priority);
        if( skiq_write_tx_thread_priority(card, priority) != 0 )
        {
            fprintf(stderr,"Error: unable to configure TX priority\n");
            goto cleanup;
        }
    }

    // register the callback
    status = skiq_register_tx_complete_callback( card, &tx_complete );
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to register transmit completion callback"
                " (status = %" PRIi32 ")\n", status);
        goto cleanup;
    }

    // reset the timestamp
    status = skiq_reset_timestamps(card); 
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to reset the timestamps"
                " (status = %" PRIi32 ")\n", status);
        goto cleanup;
    }

    // print PYTEST_EVENT Info
     if( test_tx_handles )
     {       
        printf("Info: Start transmitting: %s \n", PYTEST_EVENT);
        fflush(stdout);
     }

    // enable the Tx streaming
    status = skiq_start_tx_streaming(card, hdl);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to start streaming"
                " (status = %" PRIi32 ")\n", status);
        goto cleanup;
    }

    // replay the file the # times specified
    while( (repeat > 0 ) && (running==true) )
    {
        // transmit a block at a time
        while( (curr_block < num_blocks) && (running==true) )
        {
            // need to make sure that we don't update the timestamp of a packet
            // that is already in use
            pthread_mutex_lock( &tx_buf_mutex );
            if( p_tx_status[curr_block] == 0 )
            {
                p_tx_status[curr_block] = 1;
            }
            else
            {
                pthread_mutex_unlock( &tx_buf_mutex );
                pthread_mutex_lock( &space_avail_mutex );
                // wait for a packet to complete
                pthread_cond_wait( &space_avail_cond, &space_avail_mutex );
                pthread_mutex_unlock( &space_avail_mutex );
                continue;
            }
            pthread_mutex_unlock( &tx_buf_mutex );

            skiq_tx_set_block_timestamp( p_tx_blocks[curr_block], timestamp );

            // transmit the data
            status = skiq_transmit(card, hdl, p_tx_blocks[curr_block], &(p_tx_status[curr_block]));
            if( status == SKIQ_TX_ASYNC_SEND_QUEUE_FULL )
            {
                // update the in use status since we didn't actually send it yet
                pthread_mutex_lock( &tx_buf_mutex );
                p_tx_status[curr_block] = 0;
                pthread_mutex_unlock( &tx_buf_mutex );

                // if there's no space left to send, wait until there should be space available
                if( running == true )
                {
                    pthread_mutex_lock( &space_avail_mutex );
                    pthread_cond_wait( &space_avail_cond, &space_avail_mutex );    
                        pthread_mutex_unlock( &space_avail_mutex );
                }
            }
            else if ( status != 0 )
            {
                fprintf(stderr, "Error: skiq_transmit encountered an error."
                " (status = %" PRIi32 ")\n", status);
                goto cleanup;
            }
            else
            {
                curr_block++;
                // update the timestamp
                timestamp += timestamp_increment;
                send_count++;
            }
        }

        if( repeat > 0 )
        {
            printf("Info: transmitting the file %u more times\n", repeat);
        }
        else
        {
            printf("Info: transmit complete\n");
        }

        // see how many underruns we had if we were running in immediate mode
        if( tx_mode == skiq_tx_immediate_data_flow_mode )
        {
            skiq_read_tx_num_underruns(card, hdl, &errors);
            printf("Info: total number of tx underruns is %u\n", errors);
        }
        else
        {
            skiq_read_tx_num_late_timestamps(card, hdl, &errors);
            printf("Info: total number of tx late detected is %u\n", errors);
        }
        repeat--;
        curr_block = 0;
    }

    // wait until we've finished transmitting
    printf("waiting for done...");
    while( (send_count != complete_count) && (running==true) )
    {
        pthread_mutex_lock( &space_avail_mutex );
        pthread_cond_wait( &space_avail_cond, &space_avail_mutex );
        pthread_mutex_unlock( &space_avail_mutex );
    }
    printf("done\n");
    
cleanup:
    // disable streaming, cleanup and shutdown
    if (skiq_initialized)
    {
        skiq_stop_tx_streaming(card, hdl);

        if (NULL != p_tx_blocks)
        {
            /* cleanup the memory alloc'ed */
            for( curr_block=0; curr_block<num_blocks; curr_block++ )
            {
                skiq_tx_block_free( p_tx_blocks[curr_block] );
            }
            free( p_tx_blocks );
            p_tx_blocks = NULL;
        }

        skiq_exit();
        skiq_initialized = false;
    }

    if (NULL != input_fp)
    {
        fclose(input_fp);
        input_fp = NULL;
    }

    if (NULL != p_tx_status)
    {
        free(p_tx_status);
        p_tx_status = NULL;
    }

    return (int) status;
}

/*****************************************************************************/
/** This function reads the contents of the file into p_tx_buf

    @param none
    @return: void
*/
static int32_t init_tx_buffer(void)
{
    int32_t status = 0;
    uint32_t num_bytes_in_file=0;
    // we need enough room for # blocks + header + 1 word for status info
    int32_t num_bytes_read=1;
    uint32_t i=0;
    uint32_t j = 0;

    // determine how large the file is and how many blocks we'll need to send
    fseek(input_fp, 0, SEEK_END);
    num_bytes_in_file = ftell(input_fp);
    rewind(input_fp);
    num_blocks = (num_bytes_in_file / (block_size_in_words*4));
    // if we don't end on an even block boundary, we need an extra block
    if ( (num_bytes_in_file % (block_size_in_words * 4)) != 0 )
    {
        num_blocks++;
    }
    printf("Info: %u blocks contained in the file\n", num_blocks);

    // allocate for # blocks
    p_tx_blocks = calloc( num_blocks, sizeof( skiq_tx_block_t * ));
    if( p_tx_blocks == NULL )
    {
        fprintf(stderr, "Error: failed to allocate memory for TX blocks.\n");
        status = -1;
        goto finished;
    }

    // allocate for # blocks
    p_tx_status = calloc( num_blocks, sizeof(*p_tx_status) );
    if( p_tx_status == NULL )
    {
        fprintf(stderr, "Error: failed to allocate memory for TX status.\n");
        status = -1;
        goto finished;
    }

    // read in the contents of the file
    for (i = 0; i < num_blocks; i++)
    {
        if ( !feof(input_fp) && (num_bytes_read>0) )
        {
            // allocate the memory for this block
            if ( chan_mode == skiq_chan_mode_dual )
            {
                /* for a dual channel mode, allocate a transmit block by doubling
                 * the number of words */
                p_tx_blocks[i] = skiq_tx_block_allocate( 2 * block_size_in_words );
            }
            else
            {
                /* allocate a transmit block by number of words */
                p_tx_blocks[i] = skiq_tx_block_allocate( block_size_in_words );
            }

            if( p_tx_blocks[i] == NULL )
            {
                fprintf(stderr, "Error: unable to allocate a transmit block\n");
                status = -1;
                goto finished;
            }

            // read another block of samples
            num_bytes_read=fread( p_tx_blocks[i]->data, sizeof(int32_t), block_size_in_words, input_fp );
            if ( chan_mode == skiq_chan_mode_dual )
            {
                /* duplicate the block of samples into the second half of the
                 * transmit block's data array */
                memcpy( &(p_tx_blocks[i]->data[block_size_in_words*2]),
                        p_tx_blocks[i]->data, block_size_in_words * 4 );
                /* populate zeros into A1 buffer on dual-channel mode for pytest: test tx handles */
                if( test_tx_handles )
                {
                    memset( &(p_tx_blocks[i]->data), 0, block_size_in_words * 4 );
                }
            }

            // mark packet as not in use
            p_tx_status[i] = 0;
        }
    }

finished:
    if (0 != status)
    {
        if (NULL != p_tx_blocks)
        {
            for (j = 0; j < i; j++)
            {
                skiq_tx_block_free(p_tx_blocks[j]);
            }

            free(p_tx_blocks);
            p_tx_blocks = NULL;
        }

        if (NULL != p_tx_status)
        {
            free(p_tx_status);
            p_tx_status = NULL;
        }

        if (NULL != input_fp)
        {
            fclose(input_fp);
            input_fp = NULL;
        }
    }
    return status;
}
