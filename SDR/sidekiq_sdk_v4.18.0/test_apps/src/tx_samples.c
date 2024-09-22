/*! \file tx_samples.c
 * \brief This file contains a basic application for transmitting
 * sample data.
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

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- transmit I/Q data";
static const char* p_help_long =
"\
Configure the Tx lineup according to the specified parameters and transmit\n\
the entire contents of a provided file. The file should contain 16-bit\n\
signed twos-complement little-endian I/Q samples formatted as follows:\n\
\n\
    <16-bit Q0> <16-bit I0> <16-bit Q1> <16-bit I1> ... etc\n\
\n\
Note that unless an initial timestamp is provided, Sidekiq will transmit in\n\
immediate mode. This will cause the FPGA to begin transmission as soon as\n\
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
  --timestamp-base=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --repeat=0\n\
  --cal-mode=auto\n\
  --force-cal=false";

/* variables used for all command line arguments */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static uint16_t attenuation = 100;
static uint16_t block_size_in_words = 1020;
static uint64_t lo_freq = 850000000;
static uint32_t sample_rate = 1000000;
static uint32_t bandwidth = 0;
static uint64_t timestamp = 0;
static int32_t repeat = 0;
static char* p_file_path = NULL;
static char* p_hdl = "A1";
static char* p_timestamp_base = DEFAULT_TIMESTAMP_BASE;
static bool immediate_mode = false;
static bool packed = false;
static bool iq_swap = false;
static skiq_iq_order_t  iq_order_mode = skiq_iq_order_qi;

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
static char *p_cal_mode = "auto";
static skiq_tx_quadcal_mode_t cal_mode = skiq_tx_quadcal_mode_auto;
static bool force_cal = false;
static char* p_rfic_file_path = NULL;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("attenuation",
                'a',
                "Output attenuation in quarter dB steps",
                "dB",
                &attenuation,
                UINT16_VAR_TYPE),
    APP_ARG_OPT("bandwidth",
                'b',
                "Bandwidth in Hertz",
                "Hz",
                &bandwidth,
                UINT32_VAR_TYPE),
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
    APP_ARG_TERMINATOR,

};

/* local functions */
static int32_t init_tx_buffer(void);
static int32_t stop_tx_streaming_after_rf_ts( uint8_t card_,
                                              skiq_tx_hdl_t hdl_,
                                              uint64_t rf_ts_ );

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
    uint32_t j = 0;
    pid_t owner = 0;
    bool skiq_initialized = false;
    FILE *p_rfic_file = NULL;

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
        printf("Error: must specify EITHER card ID or serial number, not"
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
                    " (result code %" PRIi32 ")\n", p_serial, status);
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
        printf("Info: using Tx handle A2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B1") )
    {
        hdl = skiq_tx_hdl_B1;
        /* set chan mode to dual */
        chan_mode = skiq_chan_mode_dual;
        hdl_other = skiq_tx_hdl_A1;
        mult_factor = 2;
        printf("Info: using Tx handle B1\n");
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

    if( bandwidth == 0 )
    {
        bandwidth = sample_rate;
    }

    input_fp = fopen(p_file_path, "rb");
    if( input_fp == NULL )
    {
        fprintf(stderr, "Error: unable to open input file %s\n", p_file_path);
        status = -1;
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
                    " valid card specified? (result code %" PRIi32 ")\n",
                    status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with"
                    " status %" PRIi32 "\n", status);
        }
        fclose(input_fp);
        return (-1);
    }
    skiq_initialized = true;

    if( p_rfic_file_path != NULL )
    {
        p_rfic_file = fopen(p_rfic_file_path, "r");
        if( p_rfic_file == NULL )
        {
            printf("Error: unable to open specified RFIC configuration file %s (errno %d)\n",
                   p_rfic_file_path, errno);
            skiq_exit();
            exit(-1);
        }
        else
        {
            printf("Info: configuring RFIC with configuration from %s\n", p_rfic_file_path);
            status = skiq_prog_rfic_from_file(p_rfic_file,card);
        }
        if( status != 0 )
        {
            printf("Error: unable to program RFIC from file with error %d\n", status);
            skiq_exit();
            fclose( p_rfic_file );
            exit(-1);
        }
    }

    status = skiq_write_iq_order_mode(card, iq_order_mode);
    if (0 != status)
    {
        printf("Error: failed to set iq_order_mode on"
                " card %" PRIu8 "  with status %"
                PRIi32 "\n", card, status);
        skiq_exit();
        exit(-1);
    }

    // configure the calibration mode
    status = skiq_write_tx_quadcal_mode( card, hdl, cal_mode );
    if ( 0 != status )
    {
        fprintf(stderr, "Error: unable to configure quadcal mode with %" PRIi32 "\n", status);
        goto cleanup;
    }

    /* set the transmit quadcal mode on the other associated handle when using dual channel mode
     * (i.e. other handle is valid) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_quadcal_mode( card, hdl_other, cal_mode );
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure quadcal mode on other hdl with %"
                    PRIi32 "\n", status);
            goto cleanup;
        }
    }

    status = skiq_read_tx_LO_freq_range( card, &max_lo_freq, &min_lo_freq );
    if ( 0 == status )
    {
        printf("Info: tunable TX LO frequency range = %" PRIu64
                "Hz to %" PRIu64 "Hz\n", min_lo_freq, max_lo_freq);
    }
    else
    {
        printf("Warning: failed to read TX LO frequency range (result code"
                " %" PRIi32 ")\n", status);
    }

    /* set the channel mode */
    status = skiq_write_chan_mode(card, chan_mode);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to set channel mode (result code %"
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
            fprintf(stderr, "Warning: unable to configure Tx sample rate (result"
                    " code %" PRIi32 ")\n", status);
        }

        /* set the transmit quadcal mode on the other associated handle when using dual channel mode
         * (i.e. other handle is valid) */
        if ( hdl_other != skiq_tx_hdl_end )
        {
            status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl_other, sample_rate,
                                                             bandwidth);
            if ( status != 0 )
            {
                fprintf(stderr, "Warning: unable to configure Tx sample rate (result"
                        " code %" PRIi32 ")\n", status);
            }
        }
    }
    else
    {
        printf("Info: RFIC configuration provided, skipping sample rate / bandwidth configuration\n");
        fclose( p_rfic_file );
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
        printf("Warning: failed to read TX sample rate and bandwidth (result"
                " code %" PRIi32 ")\n", status);
    }

    status = skiq_write_tx_LO_freq(card, hdl, lo_freq);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx LO frequency (result"
                " code %" PRIi32 ")\n", status);
        goto cleanup;
    }

    /* set the transmit quadcal mode on the other associated handle when using dual channel mode
     * (i.e. other handle is valid) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_LO_freq(card, hdl_other, lo_freq);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx LO frequency (result"
                    " code %" PRIi32 ")\n", status);
            goto cleanup;
        }
    }
    printf("Info: configured Tx LO freq to %" PRIu64 " Hz\n", lo_freq);

    status = skiq_write_tx_attenuation(card, hdl, attenuation);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx attenuation (result"
               " code %" PRIi32 ")\n", status);
        goto cleanup;
    }
    printf("Info: actual attenuation is %.2f dB, requested attenuation is %u\n",
           ((float)attenuation / 4.0),
           attenuation);

    /* set the transmit attenuation on the other associated handle when using dual channel mode
     * (i.e. other handle is valid) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_attenuation(card, hdl_other, attenuation);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx attenuation on other hdl (result"
                    " code %" PRIi32 ")\n", status);
            goto cleanup;
        }
    }

    if( force_cal == true )
    {
        printf("Info: forcing calibration to run\n");
        status = skiq_run_tx_quadcal( card, hdl );
        if( status != 0 )
        {
            printf("Error: calibration failed to run properly (%d)\n", status);
            goto cleanup;
        }

        /* force calibration on the other associated handle when using dual channel mode (i.e. other
         * handle is valid) */
        if ( hdl_other != skiq_tx_hdl_end )
        {
            status = skiq_run_tx_quadcal( card, hdl_other );
            if( status != 0 )
            {
                printf("Error: calibration failed to run properly on other hdl (%d)\n", status);
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
                    " support late timestamp mode (result code %" PRIi32
                    ")\n", status);
        }
        else
        {
            fprintf(stderr, "Error: unable to configure Tx data flow mode"
                    " (result code %" PRIi32 ")\n", status);
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

    if( (status=skiq_write_tx_block_size(card, hdl,
                                         block_size_in_words)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx block size (result"
               " code %" PRIi32 ")\n", status);
        goto cleanup;
    }
    printf("Info: block size set to %u words\n", block_size_in_words);
    
    /* set the mode (packed or unpacked) */
    if( (status=skiq_write_iq_pack_mode(card, packed)) != 0 )
    {
        if ( status == -ENOTSUP )
        {
            fprintf(stderr, "Error: packed mode is not supported on this Sidekiq product\n");
        }
        else
        {
            fprintf(stderr, "Error: unable to set the packed mode (result code %" PRIi32 ")\n",
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
                            " (result code %" PRIi32 ")\n", card, status);
            goto cleanup;
        }
    }

    // reset the timestamp
    status = skiq_reset_timestamps(card);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to reset timestamps (result code %"
                PRIi32 ")\n", status);
        goto cleanup;
    }

    // enable the Tx streaming
    status = skiq_start_tx_streaming(card, hdl);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to start streaming (result code %"
                PRIi32 ")\n", status);
        goto cleanup;
    }
    printf("Info: successfully started streaming\n");

    // replay the file the # times specified
    while( (repeat >= 0) && (running==true) )
    {
        // transmit a block at a time
        while( (curr_block < num_blocks) && (running==true) )
        {
            skiq_tx_set_block_timestamp( p_tx_blocks[curr_block], timestamp );

            // transmit the data
            status = skiq_transmit(card, hdl, p_tx_blocks[curr_block], NULL );
            if (0 != status)
            {
                fprintf(stderr, "Error: failed to transmit data (result code %"
                        PRIi32 ")\n", status);
            }
            curr_block++;

            // update the timestamp
            timestamp += timestamp_increment;
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
            printf("Info: total number of tx late detected is %" PRIu32 "\n",
                    errors);
        }

        repeat--;
        curr_block = 0;
    }

    // disable streaming, cleanup and shutdown
    if( tx_mode == skiq_tx_with_timestamps_data_flow_mode )
    {
        // the timestamp variable should contain the timestamp of the next
        // would-be block...so stop after that timestamp occurs
        printf("Info: waiting until timestamp %" PRIu64 " before disabling TX\n", timestamp);
        if( stop_tx_streaming_after_rf_ts( card, hdl, timestamp ) == 0 )
        {
            printf("Info: TX streaming disabled\n");
        }
    }
    else
    {
        skiq_stop_tx_streaming(card, hdl);
    }

cleanup:
    printf("Info: shutting down...\n");

    // disable streaming, cleanup and shutdown
    if (skiq_initialized)
    {
        if (NULL != p_tx_blocks)
        {
            for (j = 0; j < num_blocks; j++)
            {
                skiq_tx_block_free(p_tx_blocks[j]);
            }

            free(p_tx_blocks);
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

    return status;
}


/*****************************************************************************/
/** This function reads the contents of the file into p_tx_blocks.

    @param none
    @return: void
*/
static int32_t init_tx_buffer(void)
{
    int32_t status = 0;
    uint32_t num_bytes_in_file=0;
    int32_t bytes_read=1;
    uint32_t i = 0;
    uint32_t j = 0;
    int result = 0;

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

    // allocate the buffer
    p_tx_blocks = calloc( num_blocks, sizeof( skiq_tx_block_t* ) );
    if( p_tx_blocks == NULL )
    {
        fprintf(stderr, "Error: unable to allocate %u bytes to hold transmit"
                " block descriptors\n",
                (uint32_t)(num_blocks * sizeof( skiq_tx_block_t* )));
        status = -1;
        goto finished;
    }

    // read in the contents of the file, one block at a time
    for (i = 0; i < num_blocks; i++)
    {
        if ( !feof(input_fp) && (bytes_read > 0) )
        {
            if ( chan_mode == skiq_chan_mode_dual )
            {
                /* for a dual channel mode, allocate a transmit block by
                 * doubling the number of words */
                p_tx_blocks[i] = \
                    skiq_tx_block_allocate( 2 * block_size_in_words );
            }
            else
            {
                /* allocate a transmit block by number of words */
                p_tx_blocks[i] = skiq_tx_block_allocate( block_size_in_words );
            }

            if ( p_tx_blocks[i] == NULL )
            {
                fprintf(stderr,
                    "Error: unable to allocate transmit block data\n");
                status = -2;
                goto finished;
            }

            // read a block of samples
            bytes_read = fread( p_tx_blocks[i]->data, sizeof(int32_t),
                                block_size_in_words, input_fp );
            result = ferror(input_fp);
            if ( 0 != result )
            {
                fprintf(stderr,
                    "Error: unable to read from file (result code %d)\n",
                    result);
                status = -3;
                goto finished;
            }
            if ( chan_mode == skiq_chan_mode_dual )
            {
                /* duplicate the block of samples into the second half of the
                 * transmit block's data array */
                memcpy( &(p_tx_blocks[i]->data[block_size_in_words*2]),
                        p_tx_blocks[i]->data, block_size_in_words * 4 );
            }
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

        if (NULL != input_fp)
        {
            fclose(input_fp);
            input_fp = NULL;
        }
    }

    return status;
}

/*****************************************************************************/
/** This function disables the TX streaming interface after the specified
    timestamp is reached.

    @param card_ Sidekiq card of interest
    @param hdl_ TX handle of interest
    @param rf_ts_ RF timestamp that must be exceeded before shutting down the stream
    @return: void
*/
int32_t stop_tx_streaming_after_rf_ts( uint8_t card_,
                                       skiq_tx_hdl_t hdl_,
                                       uint64_t rf_ts_ )
{
    int32_t status=0;
    uint64_t curr_ts=0;
    struct timespec ts;
    uint64_t num_nanosecs=0;
    uint32_t num_secs=0;

    if( (status=skiq_read_curr_tx_timestamp(card_, hdl_, &curr_ts)) == 0 )
    {
        // if our current timestamp is already larger than what was requested,
        // just disable the stream immediately
        if( curr_ts < rf_ts_ )
        {
            // calculate how long to sleep for before checking our timestamp
            num_nanosecs = ceil((double)((double)(rf_ts_ - curr_ts) / (double)(sample_rate)) * 1000000000);
            while (num_nanosecs >= 1000000000)
            {
                num_nanosecs -= 1000000000;
                num_secs++;
            }
            ts.tv_sec = num_secs;
            ts.tv_nsec = (uint32_t)(num_nanosecs);
            while( (nanosleep(&ts, &ts) == -1) && (running==true) )
            {
                if ( (errno == ENOSYS) || (errno == EINVAL) ||
                     (errno == EFAULT) )
                {
                    fprintf(stderr, "Error: catastrophic timer failure (errno"
                        " %d)!\n", errno);
                    break;
                }
            }

            skiq_read_curr_tx_timestamp(card_, hdl_, &curr_ts);
            while( (curr_ts < rf_ts_) && (running==true) )
            {
                // just sleep 1 uS and read the timestamp again...we should be close based on the initial sleep calculation
                usleep(1);
                skiq_read_curr_tx_timestamp(card_, hdl_, &curr_ts);
            }
        }
        printf("Info: Stopping TX streaming\n");
        status = skiq_stop_tx_streaming( card_, hdl_ );
    }

    return (status);
}
