/*! \file rx_samples.c
 * \brief This file contains a basic application for acquiring
 * a contiguous block of I/Q sample pairs in the most efficient
 * manner possible.
 *  
 * <pre>
 * Copyright 2012-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#if (defined __MINGW32__)
#define OUTPUT_PATH_MAX                 260
#else
#include <linux/limits.h>
#define OUTPUT_PATH_MAX                 PATH_MAX
#endif

#include "sidekiq_api.h"
#include "arg_parser.h"

/* a simple pair of MACROs to round up integer division */
#define _ROUND_UP(_numerator, _denominator)    (_numerator + (_denominator - 1)) / _denominator
#define ROUND_UP(_numerator, _denominator)     (_ROUND_UP((_numerator),(_denominator)))

/* MACRO used to swap elements, used depending on the IQ order mode before verify_data() */
#define SWAP(a, b) do { typeof(a) t; t = a; a = b; b = t; } while(0)

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

#define CHECK_TIMESTAMPS 1

#define NUM_USEC_IN_MS (1000)

#define CAL_MODE_STRLEN (10)
    
#define CAL_TYPE_DELIM (",")
#define CAL_TYPE_STRLEN (50)

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- capture Rx data";
static const char* p_help_long =
"\
Tune to the user-specified Rx frequency and acquire the specified number of\n\
words at the requested sample rate. Additional features such as gain, \n\
channel path, and warp voltage may be configured prior to data collection.\n\
Upon capturing the required number of samples, the data will be stored to\n\
a file for post analysis.\n\
\n\
The data is stored in the file as 16-bit I/Q pairs, with an option to specify \n\
the ordering of the pairs.  By default, the 'Q' sample occurs first, followed by the \n\
'I' sample, resulting in the following format:\n\
\n\
\n\
              skiq_iq_order_qi: (default)                skiq_iq_order_iq:\n\
            -15--------------------------0-       -15--------------------------0-\n\
            |         12-bit Q0_A1        |       |         12-bit I0_A1        |\n\
  index 0   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |\n\
            -------------------------------       -------------------------------\n\
            |         12-bit I0_A1        |       |         12-bit Q0_A1        |\n\
  index 1   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |\n\
            -------------------------------       -------------------------------\n\
            |         12-bit Q1_A1        |       |         12-bit I1_A1        |\n\
  index 2   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |\n\
            -------------------------------       -------------------------------\n\
            |         12-bit I1_A1        |       |         12-bit Q1_A1        |\n\
  index 3   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |\n\
            -------------------------------       -------------------------------\n\
            |             ...             |       |             ...             |\n\
            -------------------------------       -------------------------------\n\
            |             ...             |       |             ...             |\n\
            -15--------------------------0-       -15--------------------------0-\n\
\n\
Each sample is little-endian, twos-complement, signed, and sign-extended\n\
from 12 to 16-bits (when appropriate for the product).\n\
\n\
NOTE: --packed and --low-latency modes conflict with one another\n\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --frequency=850000000\n\
  --handle=A1\n\
  --rate=1000000\n\
  --words=100000\
";

static const char* p_file_suffix[skiq_rx_hdl_end] = { ".a1", ".a2", ".b1", ".b2", ".c1", ".d1" };
/* storage for all cmd line args */
static uint32_t num_payload_words_to_acquire = 100000;
static uint64_t lo_freq = 850000000;
static uint32_t sample_rate = 1000000;
static uint32_t bandwidth = UINT32_MAX;
static uint16_t warp_voltage = UINT16_MAX;
static uint8_t rx_gain = UINT8_MAX;
static bool rx_gain_is_set = false;
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static char* p_hdl = "A1";
static char* p_file_path = NULL;
static char *p_trigger_src = "immediate";
static skiq_trigger_src_t trigger_src = skiq_trigger_src_immediate;
static char* p_pps_source = NULL;
static skiq_1pps_source_t pps_source = skiq_1pps_source_unavailable;
static char* p_rfic_file_path = NULL;
static bool use_counter = false;
static bool include_meta = false;
static bool packed = false;
static bool blocking_rx = false;
static bool low_latency = false;
static bool balanced = false;
static bool disable_dc_corr = false;
static bool align_samples = false;
/* variable used to signal force quit of application */
static bool running = true;
static int32_t rf_port_int = (int32_t)(skiq_rf_port_unknown);
static uint32_t settle_time = 0;
static bool iq_swap = false;
static skiq_iq_order_t  iq_order_mode = skiq_iq_order_qi;
static char *p_cal_mode = "auto";
static skiq_rx_cal_mode_t cal_mode = skiq_rx_cal_mode_auto;
static char *p_cal_type= "all";
static bool cal_mode_is_set = false;
static bool rfic_pin_enable = false;
static uint32_t retries_on_ts_err = 0;

static char p_filename[OUTPUT_PATH_MAX];
static ssize_t filename_len = 0;


/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("bandwidth",
                'b',
                "Bandwidth in hertz",
                "Hz",
                &bandwidth,
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
    APP_ARG_REQ("destination",
                'd',
                "Output file to store Rx data",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to receive samples at in Hertz",
                "Hz",
                &lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT_PRESENT("gain",
                        'g',
                        "Manually configure the gain by index rather than using automatic",
                        "index",
                        &rx_gain,
                        UINT8_VAR_TYPE,
                        &rx_gain_is_set),
    APP_ARG_OPT("warp",
                0,
                "Configure the TCVCXO warp voltage (0..1023) rather than using factory preset",
                "DAC",
                &warp_voltage,
                UINT16_VAR_TYPE),
    APP_ARG_OPT("handle",
                0,
                "Rx handle to use, either A1, A2, B1, B2, C1, D1, or ALL",
                "Rx",
                &p_hdl,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("words",
                'w',
                "Number of I/Q sample words to acquire",
                "N",
                &num_payload_words_to_acquire,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("counter",
                0,
                "Receive counter data",
                NULL,
                &use_counter,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("meta",
                0,
                "Save metadata with samples (increases output file size)",
                NULL,
                &include_meta,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("packed",
                0,
                "Use packed mode for I/Q samples",
                NULL,
                &packed,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("blocking",
                0,
                "Perform blocking during skiq_receive call",
                NULL,
                &blocking_rx,
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
    APP_ARG_OPT("rf-port",
                0,
                "RX RF port (configurability dependent on product)",
                NULL,
                &rf_port_int,
                INT32_VAR_TYPE),
    APP_ARG_OPT("trigger-src",
                0,
                "Source of start / stop streaming trigger (1pps, immediate, synced)",
                NULL,
                &p_trigger_src,
                STRING_VAR_TYPE),
    APP_ARG_OPT("pps-source",
                0,
                "Source of 1PPS signal (external or host), only valid when --trigger-src=1pps",
                NULL,
                &p_pps_source,
                STRING_VAR_TYPE),
    APP_ARG_OPT("disable-dc",
                0,
                "Disable DC offset correction",
                NULL,
                &disable_dc_corr,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("align-samples",
                0,
                "Align samples prior to storing to a file",
                NULL,
                &align_samples,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("rfic-config",
                0,
                "Input filename of RFIC configuration",
                NULL,
                &p_rfic_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("settle-time",
                0,
                "Amount of time (in ms) after configuring radio prior to receiving samples",
                NULL,
                &settle_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("sample-order-iq",
                0,
                "Configure sample ordering iq",
                NULL,
                &iq_swap,
                BOOL_VAR_TYPE),
    APP_ARG_OPT_PRESENT("cal-mode",
                        0,
                        "Calibration mode, either auto or manual",
                        NULL,
                        &p_cal_mode,
                        STRING_VAR_TYPE,
                        &cal_mode_is_set),
    APP_ARG_OPT("cal-type",
                0,
                "Comma-separate list of calibration types (all | dc-offset | quadrature)",
                NULL,
                &p_cal_type,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rfic-pin-control",
                0,
                "RFIC Tx/Rx enabled by gpio pins",
                NULL,
                &rfic_pin_enable,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("retries-on-ts-err",
                0,
                "Number of times to restart a sample capture if a timestamp error occurs",
                NULL,
                &retries_on_ts_err,
                UINT32_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

/* local functions */
static void print_block_contents( skiq_rx_block_t* p_block,
                                  int32_t block_size_in_bytes );
static int32_t verify_data( int16_t* p_data,
                            uint32_t num_samps,
                            int32_t block_size_in_bytes );
static void unpack_data( int16_t *packed_data,
                         int16_t *unpacked_data,
                         uint32_t num_unpacked_samples,
                         int32_t block_size_in_words );
static int16_t sign_extend( int16_t in );
static skiq_rf_port_t map_int_to_rf_port( uint32_t port );
static void close_open_files( FILE **p_files, uint8_t nr_handles );

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
/** This is the main function for executing the rx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    skiq_xport_init_level_t level = skiq_xport_init_level_full;
    FILE* output_fp[skiq_rx_hdl_end] = { NULL };
    skiq_rf_port_t rf_port = skiq_rf_port_unknown;
    skiq_rx_gain_t gain_mode;
    skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
    skiq_rx_stream_mode_t stream_mode = skiq_rx_stream_mode_high_tput;
    int32_t block_size_in_words = 0;
    int num_blocks = 0;
    uint32_t total_num_payload_words_acquired[skiq_rx_hdl_end];
    uint64_t curr_ts[skiq_rx_hdl_end];
    uint64_t next_ts[skiq_rx_hdl_end];
    uint64_t first_ts[skiq_rx_hdl_end];
    uint32_t rx_block_cnt[skiq_rx_hdl_end];
    int32_t status=0;
    skiq_rx_status_t rx_status;
    uint32_t* p_next_write[skiq_rx_hdl_end];
    uint32_t num_words_written=0;
    uint32_t num_words_read=0;
    skiq_rx_block_t* p_rx_block;
    uint32_t len;
    uint32_t* p_rx_data[skiq_rx_hdl_end];
    uint32_t* p_rx_data_start[skiq_rx_hdl_end];
    skiq_rx_hdl_t curr_rx_hdl;
    bool first_block[skiq_rx_hdl_end];
    bool last_block[skiq_rx_hdl_end];
    uint8_t num_hdl_rcv = 0;
    bool rx_overload[skiq_rx_hdl_end];
    uint8_t gain_meta[skiq_rx_hdl_end];
    uint8_t curr_gain=0;
    uint64_t min_lo_freq, max_lo_freq;
    bool all_chans=false;
    uint8_t max_gain_index, min_gain_index;
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    pid_t owner = 0;
    FILE *p_rfic_file = NULL;
    uint32_t cal_mask = (uint32_t)(skiq_rx_cal_type_none);
    uint32_t retry_count = 0;

    /* keep track of the specified handles */
    skiq_rx_hdl_t handles[skiq_rx_hdl_end];
    uint8_t nr_handles = 0;
    uint8_t num_handles_started=0;
    uint8_t i;

    uint32_t payload_words=0;
    uint32_t words_received[skiq_rx_hdl_end];

    bool cal_type_all = false;
    bool rfic_ctrl_out = false;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize all the handles to be invalid */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        first_block[curr_rx_hdl] = true;
        last_block[curr_rx_hdl] = false;
        rx_overload[curr_rx_hdl] = false;
        gain_meta[curr_rx_hdl] = UINT8_MAX;
        words_received[curr_rx_hdl] = 0;
    }

    /* initialize everything based on the arguments provided */
    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    if( strncasecmp(p_cal_type, "all", CAL_TYPE_STRLEN) == 0 )
    {
        cal_type_all = true;
    }
    else
    {
        cal_type_all = false;
        // initialize to none and build it up
        cal_mask = (uint32_t)(skiq_rx_cal_type_none);

        // parse the list
        char *cal_type = strtok( p_cal_type, CAL_TYPE_DELIM );
        while( (cal_type != NULL) &&
               (status == 0) )
        {
            if( strncasecmp( cal_type, "dc-offset", CAL_TYPE_STRLEN ) == 0 )
            {
                cal_mask |= skiq_rx_cal_type_dc_offset;
            }
            else if( strncasecmp( cal_type, "quadrature", CAL_TYPE_STRLEN ) == 0 )
            {
                cal_mask |= skiq_rx_cal_type_quadrature;
            }
            else
            {
                printf("Error: invalid calibration type %s\n", cal_type);
                    return (-1);
            }
            cal_type = strtok(NULL, CAL_TYPE_DELIM);
        }
    }

    if( cal_mode_is_set == true )
    {
        if( 0 == strncasecmp(p_cal_mode, "auto", CAL_MODE_STRLEN) )
        {
            cal_mode = skiq_rx_cal_mode_auto;
        }
        else if( 0 == strncasecmp(p_cal_mode, "manual", CAL_MODE_STRLEN) )
        {
            cal_mode = skiq_rx_cal_mode_manual;
        }
        else
        {
            fprintf(stderr, "Error: invalid calibration mode (%s)\n", p_cal_mode);
            status = -EINVAL;
            return (status);
        }
    }

    if( balanced == true && low_latency == true )
    {
        printf("Error: cannot specify both balanced and low latency stream mode\n");
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

    /* map argument values to sidekiq specific variable values */
    if( 0 == strcasecmp(p_hdl, "A1") )
    {
       handles[nr_handles++] = skiq_rx_hdl_A1;
        printf("Info: using Rx handle A1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "A2") )
    {
        handles[nr_handles++] = skiq_rx_hdl_A2;
        chan_mode = skiq_chan_mode_dual;
        printf("Info: using Rx handle A2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B1") )
    {
        handles[nr_handles++] = skiq_rx_hdl_B1;
        printf("Info: using Rx handle B1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B2") )
    {
        handles[nr_handles++] = skiq_rx_hdl_B2;
        printf("Info: using Rx handle B2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "C1") )
    {
        handles[nr_handles++] = skiq_rx_hdl_C1;
        printf("Info: using Rx handle C1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "D1") )
    {
        handles[nr_handles++] = skiq_rx_hdl_D1;
        printf("Info: using Rx handle D1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "ALL") )
    {
        all_chans = true;
        printf("Info: using all Rx handles\n");
    }
    else
    {
        printf("Error: invalid handle specified\n");
        return(-1);
    }

    if ( packed && low_latency )
    {
        fprintf(stderr, "Error: either --packed OR --low-latency may be specified, not both\n");
        return(-1);
    }

    if( align_samples && include_meta )
    {
        fprintf(stderr, "Error: either --meta OR --align-samples may be specified, not both\n");
        return(-1);
    }

    if( align_samples && packed )
    {
        fprintf(stderr, "Error: either --packed OR --align-samples may be specified, not both\n");
        return(-1);
    }

    if ( 0 == strcasecmp( p_trigger_src, "immediate" ) )
    {
        trigger_src = skiq_trigger_src_immediate;
    }
    else if ( 0 == strcasecmp( p_trigger_src, "1pps" ) )
    {
        trigger_src = skiq_trigger_src_1pps;
    }
    else if ( 0 == strcasecmp( p_trigger_src, "synced" ) )
    {
        trigger_src = skiq_trigger_src_synced;
    }
    else
    {
        fprintf(stderr, "Error: invalid trigger source '%s' specified\n", p_trigger_src);
        return (-1);
    }

    if ( p_pps_source != NULL )
    {
        if ( trigger_src == skiq_trigger_src_1pps )
        {
            if ( 0 == strcasecmp( p_pps_source, "host" ) )
            {
                pps_source = skiq_1pps_source_host;
            }
            else if ( 0 == strcasecmp( p_pps_source, "external" ) )
            {
                pps_source = skiq_1pps_source_external;
            }
            else
            {
                fprintf(stderr, "Error: invalid 1PPS source '%s' specified", p_pps_source);
                return (-1);
            }
        }
        else
        {
            fprintf(stderr, "Error: cannot use --pps-source without specifying '1pps' with the "
                    "--trigger-src option\n");
            return (-1);
        }
    }

    /* map the RF port if it was specified */
    if( rf_port_int != skiq_rf_port_unknown )
    {
        rf_port = map_int_to_rf_port( rf_port_int );
        if( rf_port == skiq_rf_port_unknown )
        {
            printf("Error: unknown RF port specified\n");
            return (-1);
        }
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    status = skiq_init(skiq_xport_type_auto, level, &card, 1);
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
    printf("Info: initialized card %d\n", card);

    // reset the timestamps if align requested
    if( align_samples == true )
    {
        printf("Info: resetting all timestamps!\n");
        skiq_reset_timestamps(card);
    }

    if (iq_swap == true)
    {
        iq_order_mode = skiq_iq_order_iq;
    }


    /* configure the 1PPS source for each of the cards */
    if ( pps_source != skiq_1pps_source_unavailable )
    {
        status = skiq_write_1pps_source( card, pps_source );
        if ( status != 0 )
        {
            printf("Error: unable to configure PPS source to %s for card %u (status=%d)\n",
                   p_pps_source, card, status );
            skiq_exit();
            return (-1);
        }
        else
        {
            printf("Info: configured 1PPS source to %u\n", pps_source);
        }
    }

    status = skiq_write_iq_order_mode(card, iq_order_mode);
    if (0 != status)
    {
        printf("Error: failed to set iq_order_mode on"
               " card %" PRIu8 "  with status %"
               PRIi32 "\n", card, status);
        skiq_exit();
        return (-1);
    }

    if( all_chans == true )
    {
        uint8_t hdl_idx;
        skiq_param_t params;

        status = skiq_read_parameters( card, &params );
        if (0 != status)
        {
            fprintf(stderr, "Error: failed to read parameters on card %" PRIu8 " with status %"
                    PRIi32 "\n", card, status);
            skiq_exit();
            return (-1);
        }

        for ( hdl_idx = 0; hdl_idx < params.rf_param.num_rx_channels; hdl_idx++ )
        {
            bool safe_to_add = true;
            uint8_t hdl_conflict_idx=0;
            skiq_rx_hdl_t hdl_conflicts[skiq_rx_hdl_end] = { [0 ... skiq_rx_hdl_end-1] = skiq_rx_hdl_end };
            uint8_t num_conflicts=0;

            /* Assign the receive handle associated with the hdl_idx index */
            curr_rx_hdl = params.rf_param.rx_handles[hdl_idx];

            /* Determine which handles conflict with the current handle */
            status = skiq_read_rx_stream_handle_conflict(card, curr_rx_hdl, hdl_conflicts,&num_conflicts);
            if (status != 0)
            {
                printf("Error: failed to read rx_stream_handle_conflict on"
                       " card %" PRIu8 "  with status %"
                       PRIi32 "\n", card, status);
                skiq_exit();
                return (-1);
            }

            /* Add the current handle if none of the conflicting handles have already been added */
            for( hdl_conflict_idx=0; (hdl_conflict_idx<num_conflicts) && (status==0) && (safe_to_add); hdl_conflict_idx++ )
            {
                uint8_t configured_hdls_idx = 0;
                for ( configured_hdls_idx=0;configured_hdls_idx<nr_handles; configured_hdls_idx++)
                {
                    if (handles[configured_hdls_idx] == hdl_conflicts[hdl_conflict_idx])
                    {
                        safe_to_add = false;
                        break;
                    }
                }
            }
            if (safe_to_add)
            {
                handles[nr_handles++] = curr_rx_hdl;
            }
        }
        printf("Info: using all Rx handles (total number of channels is %" PRIu8
               ")\n", nr_handles);
        chan_mode = skiq_chan_mode_dual;
    }

    /* iterate over all of the specified handles, opening a file for each one */
    for ( i = 0; i < nr_handles; i++ )
    {
        curr_rx_hdl = handles[i];

        if (rfic_pin_enable)
        {
            status = skiq_write_rx_rfic_pin_ctrl_mode(card, curr_rx_hdl, skiq_rfic_pin_control_fpga_gpio );
            if (0 != status)
            {
                printf("Error: failed to set rfic pin control mode on"
                    " card %" PRIu8 "  with status %"
                    PRIi32 "\n", card, status);
                skiq_exit();
                return (-1);
            }
        }

        strncpy( p_filename, p_file_path, OUTPUT_PATH_MAX-1 );
        filename_len = (ssize_t)strlen(p_filename);

        /*
          Append a suffix to the given filename unless the user specifies
          a file in the "/dev/" directory (most likely, "/dev/null").
        */
        const char *dev_prefix = "/dev/";
        ssize_t compare_len = strlen(dev_prefix);
        if (filename_len < compare_len)
        {
            compare_len = filename_len;
        }
        if (0 != strncasecmp(p_filename, dev_prefix, compare_len))
        {
            strncpy( &(p_filename[filename_len]),
                     p_file_suffix[curr_rx_hdl],
                     (OUTPUT_PATH_MAX-1) - filename_len );
        }

        output_fp[curr_rx_hdl] = fopen(p_filename, "wb");
        if (output_fp[curr_rx_hdl] == NULL)
        {
            printf("Error: unable to open output file %s\n",p_filename);
            skiq_exit();
            return(-1);
        }
        printf("Info: opened file %s for output\n",p_filename);
    }

    if( p_rfic_file_path != NULL )
    {
        p_rfic_file = fopen(p_rfic_file_path, "r");
        if( p_rfic_file == NULL )
        {
            printf("Error: unable to open specified RFIC configuration file %s (errno %d)\n",
                   p_rfic_file_path, errno);
            skiq_exit();
            close_open_files( output_fp, nr_handles );
            return(-1);
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
            close_open_files( output_fp, nr_handles );
            fclose( p_rfic_file );
            return(-1);
        }
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
            return(-1);
        }
    }

    /* set the receive streaming mode to either depending on command line */
    if ( low_latency )
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
        printf("Error: unable to set RX stream mode with status %d\n", status);
        close_open_files( output_fp, nr_handles );
        skiq_exit();
        return (-1);
    }

    /* initialize the warp voltage here to allow time for it to settle */
    if ( warp_voltage != UINT16_MAX )
    {
        if( (status=skiq_write_tcvcxo_warp_voltage(card,warp_voltage)) != 0 )
        {
            printf("Error: unable to set the warp voltage, using previous value\n");
        }
        printf("Info: tcvcxo warp voltage configured to %u\n", warp_voltage);
    }
    else
    {
        printf("Info: tcvcxo warp voltage left at factory setting\n");
    }

    if (disable_dc_corr)
    {
        uint8_t j;

        printf("Info: disabling DC offset correction\n");
        for (j = 0; j < nr_handles; j++)
        {
            status = skiq_write_rx_dc_offset_corr(card, handles[j], false);
            if (0 != status)
            {
                printf("Error: failed to disable DC offset correction on"
                        " card %" PRIu8 " handle %" PRIu8 " with status %"
                        PRIi32 "\n", card, j, status);
                skiq_exit();
                return (-1);
            }
        }
    }

    /* set the gain_mode based on rx_gain from the arguments */
    if (rx_gain_is_set)
    {
        gain_mode = skiq_rx_gain_manual;
    }
    else
    {
        gain_mode = skiq_rx_gain_auto;
    }

    if (bandwidth == UINT32_MAX)
    {
        bandwidth = sample_rate;
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
            fprintf(stderr, "Error: unable to set the packed mode with status %d\n", status);
        }
        skiq_exit();
        close_open_files( output_fp, nr_handles );
        return (-1);
    }
    printf("Info: packed mode %s\n", (packed == true) ? "enabled" : "disabled");

    status = skiq_read_rx_LO_freq_range( card, &max_lo_freq, &min_lo_freq );
    if ( 0 == status )
    {
        printf("Info: tunable RX LO frequency range = %" PRIu64 "Hz to %" PRIu64 "Hz\n", min_lo_freq, max_lo_freq);
    }

    /* write the channel mode (dual if A2 is being used) */
    status = skiq_write_chan_mode(card, chan_mode);
    if ( status != 0 )
    {
        printf("Error: failed to set Rx channel mode to %u with status %d (%s)\n", chan_mode, status, strerror(abs(status)));
    }

    for ( i = 0; i < nr_handles; i++ )
    {
        curr_rx_hdl = handles[i];

        /* configure the RF port */
        if( rf_port != skiq_rf_port_unknown )
        {
            uint8_t num_fixed_ports;
            skiq_rf_port_t fixed_port_list[skiq_rf_port_max];
            uint8_t num_trx_ports;
            skiq_rf_port_t trx_port_list[skiq_rf_port_max];
            bool port_found=false;
            skiq_rf_port_config_t port_config = skiq_rf_port_config_fixed;
            // note that only specific ports are available in certain modes.  In order
            // to figure out if we need to switch the RF port mode, we first must see
            // what ports are available vs. what we requested
            if( skiq_read_rx_rf_ports_avail_for_hdl( card,
                                                     curr_rx_hdl,
                                                     &num_fixed_ports,
                                                     &(fixed_port_list[0]),
                                                     &num_trx_ports,
                                                     &(trx_port_list[0]) ) == 0 )
            {
                uint8_t j;

                // look for the port in the fixed list
                for( j=0; (j<num_fixed_ports) && (port_found==false); j++ )
                {
                    if( fixed_port_list[j] == rf_port )
                    {
                        port_config = skiq_rf_port_config_fixed;
                        port_found = true;
                    }
                }
                // now look for the port in the TRX list
                for( j=0; (j<num_trx_ports) && (port_found==false); j++ )
                {
                    if( trx_port_list[j] == rf_port )
                    {
                        port_config = skiq_rf_port_config_trx;
                        port_found = true;
                    }
                }
                // if we found the user requested port, we need to make sure our
                // RF port config is set to the write mode
                if( port_found == true )
                {
                    if( skiq_write_rf_port_config( card, port_config ) == 0 )
                    {
                        // if we're in TRx, this port could be set for either RX or TX
                        // so make sure that we force it to RX since we're only receiving
                        if( port_config == skiq_rf_port_config_trx )
                        {
                            if( skiq_write_rf_port_operation( card, false ) != 0 )
                            {
                                printf("Error: unable to configure RF port mode\n");
                                skiq_exit();
                                close_open_files( output_fp, nr_handles );
                                return(-1);
                            }
                        }
                        printf("Info: successfully configured RF port and operation\n");
                    }
                    else
                    {
                        printf("Error: unable to configure RF port / operation\n");
                        skiq_exit();
                        close_open_files( output_fp, nr_handles );
                        return(-1);
                    }
                }
                else
                {
                    printf("Error: unable to find port requested\n");
                    return(-1);
                }
                // now configure the RF port for the handle
                printf("Info: configuring RF port to J%d\n", rf_port_int);
                if( (status=skiq_write_rx_rf_port_for_hdl(card, curr_rx_hdl, rf_port )) != 0 )
                {
                    printf("Error: unable to configure the RX RF port to J%d\n", rf_port_int);
                    skiq_exit();
                    close_open_files( output_fp, nr_handles );
                    return(-1);
                }
            }
        }

        /* initialize requested handle */
        num_hdl_rcv++;

        /* set the control output bits to include the gain */
        status = skiq_enable_rfic_control_output_rx_gain(card, curr_rx_hdl);
        if ( 0 == status )
        {
            rfic_ctrl_out = true;
        }
        else
        {
            printf("Error: unable to configure the RF IC control output (status=%d)\n", status);
        }

        status = skiq_read_rx_gain_index_range( card,
                                                curr_rx_hdl,
                                                &min_gain_index,
                                                &max_gain_index );
        if ( (0 == status) && (gain_mode == skiq_rx_gain_manual) )
        {
            printf("Info: gain index range = %d to %d\n", min_gain_index, max_gain_index);
        }

        /* set the calibration mode */
        if( cal_mode_is_set == true )
        {
            status = skiq_write_rx_cal_mode( card, curr_rx_hdl, cal_mode );
            if( status != 0 )
            {
                if( status != -ENOTSUP )
                {
                    printf("Error: failed to configure RX calibration mode with %" PRIi32 "\n", status);
                    skiq_exit();
                    close_open_files( output_fp, nr_handles );
                    return (-1);
                }
                else
                {
                    printf("Warning: calibration mode %s unsupported with product\n",
                           p_cal_mode);
                }
            }
        }

        /* configure the calibration types */
        if( cal_type_all == true )
        {
            status = skiq_read_rx_cal_types_avail( card, curr_rx_hdl, &cal_mask );
            if( status != 0 )
            {
                printf("Error: unable to read calibration types available (status=%d)\n",
                       status);
                skiq_exit();
                close_open_files( output_fp, nr_handles );
                return (-1);
            }
        }
        status = skiq_write_rx_cal_type_mask( card, curr_rx_hdl, cal_mask );
        if( status != 0 )
        {
            printf("Error: failed to configure RX calibration mask to 0x%x\n", cal_mask);
            skiq_exit();
            close_open_files( output_fp, nr_handles );
            return (-1);
        }
        else
        {
            uint32_t read_cal_mask=0;
            if( (status=skiq_read_rx_cal_type_mask( card, curr_rx_hdl, &read_cal_mask )) == 0 )
            {
                if( read_cal_mask != cal_mask )
                {
                    printf("Error: read calibration mask (0x%x) does not match what was written (0x%x)\n",
                           read_cal_mask, cal_mask);
                }
                printf("Info: RX calibration mask configured as 0x%x\n", cal_mask);
            }
            else
            {
                printf("Error: unable to read calibration mask (status=%d)\n", status);
            }
        }

        /* set the sample rate and bandwidth */
        if( p_rfic_file == NULL )
        {
            status=skiq_write_rx_sample_rate_and_bandwidth(card, curr_rx_hdl, sample_rate, bandwidth);
            if (status != 0)
            {
                printf("Error: failed to set Rx sample rate or bandwidth(using default from last config file)...status is %d\n",
                       status);
            }
        }
        else
        {
            printf("Info: RFIC configuration provided, skipping sample rate / bandwidth configuration\n");
        }

        status=skiq_read_rx_sample_rate_and_bandwidth(card,
                                                      curr_rx_hdl,
                                                      &read_sample_rate,
                                                      &actual_sample_rate,
                                                      &read_bandwidth,
                                                      &actual_bandwidth);
        if( status == 0 )
        {
            printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
                   actual_sample_rate, actual_bandwidth);
        }

        /* tune the Rx chain to the requested freq */
        status = skiq_write_rx_LO_freq(card, curr_rx_hdl, lo_freq);
        if (status != 0)
        {
            printf("Error: failed to set LO freq (using previous LO freq)...status is %d\n",
                   status);
        }
        printf("Info: configured Rx LO freq to %" PRIu64 " Hz\n", lo_freq);

        /* now that the Rx freq is set, set the gain mode and gain */
        status = skiq_write_rx_gain_mode(card, curr_rx_hdl, gain_mode);
        if (status != 0)
        {
            printf("Error: failed to set Rx gain mode\n");
        }
        printf("Info: configured %s gain mode\n",
               gain_mode == skiq_rx_gain_auto ? "auto" : "manual");

        if( gain_mode == skiq_rx_gain_manual )
        {
            status=skiq_write_rx_gain(card, curr_rx_hdl, rx_gain);
            if (status != 0)
            {
                printf("Error: failed to set gain index to %" PRIu8 "\n",rx_gain);
            }
            gain_meta[curr_rx_hdl] = rx_gain;
            printf("Info: set gain index to %" PRIu8 "\n", rx_gain);
        }        
    }

    /* read the expected RX block size and convert to number of words: ( words = bytes / 4 )*/
    block_size_in_words = skiq_read_rx_block_size( card, stream_mode ) / 4;
    if ( block_size_in_words < 0 )
    {
        fprintf(stderr, "Error: Failed to read RX block size for specified stream mode with status %d\n", block_size_in_words);
        skiq_exit();
        close_open_files( output_fp, nr_handles );
        return 1;
    }

    if( packed == true )
    {
        payload_words = SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK((block_size_in_words-SKIQ_RX_HEADER_SIZE_IN_WORDS));
    }
    else
    {
        payload_words = block_size_in_words - SKIQ_RX_HEADER_SIZE_IN_WORDS;
    }
    printf("Info: acquiring %d words at %d words per block\n", num_payload_words_to_acquire, payload_words);

    /* set up the # of blocks to acquire according to the cmd line args */
    num_blocks = ROUND_UP(num_payload_words_to_acquire, payload_words);
    printf("Info: num blocks to acquire is %d\n",num_blocks);

    // determine how large a block of data is based on whether we're including metadata
    if ( include_meta == true )
    {
        printf("Info: including metadata in capture output\n");
    }
    else
    {
        block_size_in_words -= SKIQ_RX_HEADER_SIZE_IN_WORDS;
    }

    /**********************configure the Rx interface**************************/

    /* default is iq data mode, so change to counter if specified on the
       cmd line */
    if (use_counter == true)
    {
        for ( i = 0; i < nr_handles; i++ )
        {
            skiq_write_rx_data_src(card, handles[i], skiq_data_src_counter);
        }
        printf("Info: configured for counter data mode\n");
    }
    else
    {
        printf("Info: configured for I/Q data mode\n");
    }

    /************************* buffer allocation ******************************/
    /* allocate memory to hold the data when it comes in */
    for ( i = 0; i < nr_handles; i++ )
    {
        curr_rx_hdl = handles[i];
        p_rx_data[curr_rx_hdl] = (uint32_t*)calloc(block_size_in_words * num_blocks, sizeof(uint32_t));
        if (p_rx_data[curr_rx_hdl] == NULL)
        {
            printf("Error: didn't successfully allocate %d words to hold"
                   " unpacked iq\n", block_size_in_words*num_blocks);
            return(-3);
        }
        memset( p_rx_data[curr_rx_hdl], 0, block_size_in_words * num_blocks * sizeof(uint32_t) );
        p_next_write[curr_rx_hdl] = p_rx_data[curr_rx_hdl];
        p_rx_data_start[curr_rx_hdl] = p_rx_data[curr_rx_hdl];
    }

    /************************** start Rx data flowing *************************/

    /* begin streaming on the Rx interface */
    for ( i = 0; i < nr_handles; i++ )
    {
        curr_rx_hdl = handles[i];
        next_ts[curr_rx_hdl] = (uint64_t) 0;
        rx_block_cnt[curr_rx_hdl] = 0;
        total_num_payload_words_acquired[curr_rx_hdl] = 0;
    }

    if( settle_time != 0 )
    {
        printf("Info: waiting %" PRIu32 " ms prior to streaming\n", settle_time);
        usleep(NUM_USEC_IN_MS*settle_time);
    }

    printf( "Info: starting %u Rx interface(s)\n", nr_handles );
    status = skiq_start_rx_streaming_multi_on_trigger(card, handles, nr_handles, trigger_src, 0);
    if ( status != 0 )
    {
        printf("Error: receive streaming failed to start with status code %d\n", status);
        running = false;
    }

    /* loop through and acquire the requested # of data words, verifying
       that the timestamp (ts) increments as expected */
    while ( (num_hdl_rcv > 0) && (running==true) )
    {
        rx_status = skiq_receive(card, &curr_rx_hdl, &p_rx_block, &len);
        if ( skiq_rx_status_success == rx_status )
        {
            if ( ( ( curr_rx_hdl < skiq_rx_hdl_end ) && ( output_fp[curr_rx_hdl] == NULL ) ) ||
                 ( curr_rx_hdl >= skiq_rx_hdl_end ) )
            {
                printf("Error: received unexpected data from unspecified hdl %u\n", curr_rx_hdl);
                print_block_contents(p_rx_block, len);
                app_cleanup(0);
                skiq_exit();
                close_open_files( output_fp, nr_handles );
                exit(-4);
            }
            if ( NULL != p_rx_block )
            {
                /* verify timestamp and data */
                curr_ts[curr_rx_hdl] = p_rx_block->rf_timestamp;

                if( rfic_ctrl_out == true )
                {
                    /* grab the RFIC control data as receive gain index */
                    curr_gain = p_rx_block->rfic_control;
                    if( (curr_gain != gain_meta[curr_rx_hdl]) ||
                        (first_block[curr_rx_hdl] == true) )
                    {
                        double g;
                        if ( 0 == skiq_read_rx_cal_offset_by_gain_index( card, curr_rx_hdl,
                                                                         curr_gain, &g ) )
                        {
                            printf("New gain for handle %u is %u (RX cal"
                                   " offset: %.10f)\n", curr_rx_hdl, curr_gain, g);
                        }
                        else
                        {
                            printf("Gain for handle %u is %u\n", curr_rx_hdl, curr_gain);
                        }
                        if( first_block[curr_rx_hdl] != true )
                        {
                            printf("Previous gain for handle %u was %u\n",
                                   curr_rx_hdl, gain_meta[curr_rx_hdl]);
                        }
                    }
                    gain_meta[curr_rx_hdl] = curr_gain;
                }
                /* check for the overload condition */
                if( rx_overload[curr_rx_hdl] == true )
                {
                    /* see if we're still in overload */
                    if ( p_rx_block->overload == 0 )
                    {
                        printf("Info: overload condition no longer detected on"
                                " hdl %u\n", curr_rx_hdl);
                        rx_overload[curr_rx_hdl] = false;
                    }
                }
                else if ( p_rx_block->overload != 0 )
                {
                    printf("Info: overload condition detected on hdl %u!\n", curr_rx_hdl);
                    rx_overload[curr_rx_hdl] = true;
                }

#if CHECK_TIMESTAMPS
                if (first_block[curr_rx_hdl] == true)
                {
                    uint64_t max_ts=0;
                    uint32_t samples_to_copy=0;
                    first_block[curr_rx_hdl] = false;
                    first_ts[curr_rx_hdl] = curr_ts[curr_rx_hdl];
                    printf("Got first timestamp %" PRIu64 " for handle %u\n",
                           first_ts[curr_rx_hdl], curr_rx_hdl);
                    /*
                        will be incremented properly below for next time through
                    */
                    next_ts[curr_rx_hdl] = curr_ts[curr_rx_hdl];
                    num_handles_started++;
                    if( (num_handles_started == nr_handles) && (align_samples==true) )
                    {
                        printf("Info: all streams started!\n");
                        // find the greatest first timestamp received
                        for( i=0; i<nr_handles; i++ )
                        {
                            printf("Timestamp for handle %u is %" PRIu64 ", first %" PRIu64 "\n",
                                   i, curr_ts[i], first_ts[i]);
                            if( first_ts[i] > max_ts )
                            {
                                max_ts = first_ts[i];
                                printf("New max starting timestamp %" PRIu64 "\n", max_ts);
                            }
                        }
                        // determine how much data to discard for each handle based on the greatest
                        // last timestamp received
                        for( i=0; i<nr_handles; i++ )
                        {
                            printf("Need to discard %" PRIu64 " samples for handle %u, total is %u\n",
                                   (max_ts - first_ts[i]), i, total_num_payload_words_acquired[i]);
                            samples_to_copy = total_num_payload_words_acquired[i] - (max_ts - first_ts[i]);
                            printf("Need to copy %u samples for handle %u\n", samples_to_copy, i);
                            // we need the tail of the samples to the beginning of the buffer
                            if( samples_to_copy > 0 )
                            {
                                printf("Copying starting at %p to %p\n",
                                       p_rx_data_start[i] + (max_ts-first_ts[i]),
                                       p_rx_data_start[i] );
                                memmove( p_rx_data_start[i],
                                         p_rx_data_start[i] + (max_ts-first_ts[i]),
                                         samples_to_copy*4 );
                                // now update tot words acquired after discard
                                total_num_payload_words_acquired[i] = samples_to_copy;
                                // update our write pointer
                                p_next_write[i] = p_rx_data_start[i] + total_num_payload_words_acquired[i];
                                printf("Next write is now %p for %u\n", p_next_write[i], i);
                                words_received[i] = total_num_payload_words_acquired[i];
                            }
                        }
                    }
                }
                else if ( !last_block[curr_rx_hdl] &&
                          ( curr_ts[curr_rx_hdl] != next_ts[curr_rx_hdl] ) )
                {
                    /* Check for timestamp errors only if loop is still collecting blocks for
                       `curr_rx_hdl` (e.g. last block has not yet occurred) */

                    printf("Error: timestamp error in block %d for %u..."
                            "expected 0x%016" PRIx64 " but got 0x%016" PRIx64
                            " (delta %" PRId64 ")\n", \
                           rx_block_cnt[curr_rx_hdl], curr_rx_hdl,
                           next_ts[curr_rx_hdl], curr_ts[curr_rx_hdl],
                           curr_ts[curr_rx_hdl] - next_ts[curr_rx_hdl]);
                    // we've exceeded our retry count, exit
                    if( retry_count >= retries_on_ts_err )
                    {
                        print_block_contents(p_rx_block, len);
                        app_cleanup(0);
                        skiq_exit();
                        close_open_files( output_fp, nr_handles );
                        return(-1);
                    }
                    else
                    {
                        printf("Retrying sample capture: max attempts is %u, current is %u\n", 
                            retries_on_ts_err, retry_count );

                        // we need to retry no dump all of current data and go back to the beginning of the receive loop
                        for ( i = 0; i < nr_handles; i++ )
                        {
                            curr_rx_hdl = handles[i];

                            first_block[curr_rx_hdl] = true;
                            last_block[curr_rx_hdl] = false;
                            rx_overload[curr_rx_hdl] = false;
                            gain_meta[curr_rx_hdl] = UINT8_MAX;
                            words_received[curr_rx_hdl] = 0;

                            next_ts[curr_rx_hdl] = (uint64_t) 0;
                            rx_block_cnt[curr_rx_hdl] = 0;
                            total_num_payload_words_acquired[curr_rx_hdl] = 0;
                            num_handles_started=0;

                            p_next_write[curr_rx_hdl] = p_rx_data_start[curr_rx_hdl];
                        }
                        retry_count++;
                        continue;
                    }
                }
#endif
                num_words_read = len/4; /* len is in bytes */

                /* copy over all the data if this isn't the last block */
                if( (total_num_payload_words_acquired[curr_rx_hdl] + payload_words) < num_payload_words_to_acquire )
                {
                    if( include_meta == true )
                    {
                        memcpy(p_next_write[curr_rx_hdl],((uint32_t*)p_rx_block),
                               (num_words_read)*sizeof(uint32_t));
                        p_next_write[curr_rx_hdl] += num_words_read;
                    }
                    else
                    {
                        num_words_read = num_words_read - SKIQ_RX_HEADER_SIZE_IN_WORDS;
                        memcpy(p_next_write[curr_rx_hdl],
                               (void *)p_rx_block->data,
                               (num_words_read)*sizeof(uint32_t));
                        p_next_write[curr_rx_hdl] += (num_words_read);
                    }
                    // update the # of words received and num samples received
                    words_received[curr_rx_hdl] += num_words_read;
                    total_num_payload_words_acquired[curr_rx_hdl] += \
                        payload_words;
                    rx_block_cnt[curr_rx_hdl]++;
                }
                else
                {
                    if( last_block[curr_rx_hdl] == false )
                    {
                        /* determine the number of words still remaining */
                        uint32_t last_block_num_payload_words = \
                            num_payload_words_to_acquire - total_num_payload_words_acquired[curr_rx_hdl];
                        uint32_t num_words_to_copy=0;

                        // if running in packed mode the # words to copy does
                        // not match the # words received
                        if( packed == true )
                        {
                            // every 3 words contains 4 samples when packed
                            num_words_to_copy = SKIQ_NUM_WORDS_IN_PACKED_BLOCK(last_block_num_payload_words);
                        }
                        else
                        {
                            num_words_to_copy = last_block_num_payload_words;
                        }

                        // if the metadata is included, make sure to increment
                        // # words to copy and update the offset into the last
                        // block of data
                        if (include_meta)
                        {
                            num_words_to_copy += SKIQ_RX_HEADER_SIZE_IN_WORDS;
                            memcpy(p_next_write[curr_rx_hdl],
                                   ((uint32_t*)p_rx_block),
                                   num_words_to_copy*sizeof(uint32_t));
                            p_next_write[curr_rx_hdl] += num_words_to_copy;
                        }
                        else
                        {
                            memcpy(p_next_write[curr_rx_hdl],
                                   (void *)p_rx_block->data,
                                   num_words_to_copy*sizeof(uint32_t));
                            p_next_write[curr_rx_hdl] += num_words_to_copy;
                        }

                        total_num_payload_words_acquired[curr_rx_hdl] += \
                            last_block_num_payload_words;
                        rx_block_cnt[curr_rx_hdl]++;

                        num_hdl_rcv--;
                        last_block[curr_rx_hdl] = true;

                        words_received[curr_rx_hdl] += num_words_to_copy;
                    }
                }
                next_ts[curr_rx_hdl] += (payload_words);
            }
        }
    }

    /* all done, so stop streaming */
    printf( "Info: stopping %u Rx interface(s)\n", nr_handles );
    skiq_stop_rx_streaming_multi_immediate(card, handles, nr_handles);

    /* verify data if a counter was used instead of real I/Q data */
    if ( (use_counter == true) && (running==true) )
    {
        int16_t *unpacked_data = NULL;
        int32_t tmp_status = 0;

        for ( i = 0; i < nr_handles; i++ )
        {
            curr_rx_hdl = handles[i];
            if( packed == true )
            {
                uint32_t num_samples=total_num_payload_words_acquired[curr_rx_hdl];
                uint32_t header_words=0;
                if( include_meta == true )
                {
                    header_words = rx_block_cnt[curr_rx_hdl]*SKIQ_RX_HEADER_SIZE_IN_WORDS;
                }
                // allocate the memory
                unpacked_data = calloc( num_samples + header_words, sizeof(int32_t) );
                if( unpacked_data != NULL )
                {
                    unpack_data( (int16_t*)(p_rx_data_start[curr_rx_hdl]), unpacked_data, num_samples, block_size_in_words );
                    tmp_status = verify_data( unpacked_data, num_samples, block_size_in_words );
                    free(unpacked_data);
                    if( (tmp_status != 0) && (status == 0) )
                    {
                        /*
                            the verification failed; only overwrite the status code if it's marked
                            as a success
                        */
                        status = tmp_status;
                    }
                }
                else
                {
                    printf("Error: unable to allocate space for unpacking samples\n");
                }
            }
            else
            {
                tmp_status = verify_data( (int16_t*)(p_rx_data_start[curr_rx_hdl]),
                                           words_received[curr_rx_hdl],
                                           block_size_in_words );
                if( (tmp_status != 0) && (status == 0) )
                {
                    /*
                        the verification failed; only overwrite the status code if it's marked
                        as a success
                    */
                    status = tmp_status;
                }
            }
        }
    }

    /********************* write output file and clean up **********************/
    /* all file data has been verified, write off to our output file */
    for ( i = 0; (i < nr_handles) && running; i++ )
    {
        curr_rx_hdl = handles[i];
        printf("Info: done receiving, start write to file for hdl %u\n",
               curr_rx_hdl);
        num_words_written = fwrite(p_rx_data_start[curr_rx_hdl],
                                   sizeof(uint32_t),
                                   words_received[curr_rx_hdl],
                                   output_fp[curr_rx_hdl]);
        if( num_words_written < words_received[curr_rx_hdl] )
        {
            printf("Info: attempted to write %" PRIu32 " words to output file but only"
                   " wrote %" PRIu32 "\n", words_received[curr_rx_hdl],
                   num_words_written);
        }
        free(p_rx_data_start[curr_rx_hdl]);
        p_rx_data_start[curr_rx_hdl] = NULL;
    }
    close_open_files( output_fp, nr_handles );
    if( p_rfic_file != NULL )
    {
        fclose(p_rfic_file);
        p_rfic_file = NULL;
    }

    if( status == 0 )
    {
        printf("Info: Done without errors!\n");
    }
    else
    {
        printf("Error: Test failed!\n");
    }

     /* clean up */
    skiq_exit();

    return(status);
}

/*****************************************************************************/
/** This function verifies that the received sample data is a monotonically
    increasing counter.

    @param p_data: a pointer to the first 32-bit sample in the buffer to
    be verified
    @param num_samps: the number of samples to verify
    @param block_size_in_words: the number of samples in a block (in words)
    @return: void
*/
int32_t verify_data(int16_t* p_data, uint32_t num_samps, int32_t block_size_in_words)
{
    int32_t status=0;
    uint32_t offset=0;
    int16_t last_data=0;
    uint8_t rx_resolution;
    int16_t max_data=0;

    skiq_read_rx_iq_resolution( card, &rx_resolution );
    max_data = (1 << (rx_resolution-1))-1;

    printf("Info: verifying data contents, num_samps %u (RX resolution %u max"
            " ADC value %d)...\n", num_samps, rx_resolution, max_data);

    if( include_meta == true )
    {
        offset = (SKIQ_RX_HEADER_SIZE_IN_WORDS*2);
    }

    /* Check the IQ ordering mode.  If IQ ordering, use the SWAP macro on the 
    received array so the original verify_data() function can be used. */
    if (iq_swap == true)
    {
        for(uint32_t temp_offset=offset; temp_offset<(num_samps*2); temp_offset+=2 )
        {
            SWAP(p_data[temp_offset], p_data[temp_offset+1]);
        }
    }

    last_data = p_data[offset]+1;
    offset++;
    for( ; (offset<(num_samps*2)) && (status == 0); offset++ )
    {
        if( (include_meta == true) &&
            ((offset % (block_size_in_words * 2))==0) &&
            (packed == false) )
        {
            /*
                If the metadata is included in the sample data and we've reached the end of
                a block, skip over the metadata header
            */
            offset += (SKIQ_RX_HEADER_SIZE_IN_WORDS*2);
        }

        if( last_data != p_data[offset] )
        {
            printf("Error: at sample %u, expected 0x%x but got 0x%x\n",
                   offset, (uint16_t)last_data, (uint16_t)p_data[offset]);
            status = -EINVAL;
            continue;
        }
        last_data = p_data[offset]+1;
        /* see if we need to wrap the data */
        if( last_data == (max_data+1) )
        {
            last_data = -(max_data+1);
        }
    }
    printf("done\n");
    printf("-------------------------\n");

    return(status);
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

/*****************************************************************************/
/** This function performs sign extension for the 12-bit value passed in.

    @param in: 12-bit value to be sign extended
    @return: sign extended representation of value passed in
*/
int16_t sign_extend( int16_t in )
{
    int16_t out=0;

    if( in & 0x800 )
    {
        out = in | 0xF000;
    }
    else
    {
        out = in;
    }

    return out;
}

/*****************************************************************************/
/** This unpacks the sample data, packed as 12-bits and unpacked to 16-bits.

    @param packed_data: pointer to packed data
    @param unpacked_data: pointer to unpacked data
    @param num_unpacked_samples: number of samples contained in sample data
    @return: sign extended representation of value passed in
*/
static void unpack_data( int16_t *packed_data,
                         int16_t *unpacked_data,
                         uint32_t num_unpacked_samples,
                         int32_t block_size_in_words )
{
    int16_t i0;
    int16_t q0;
    int16_t i1;
    int16_t q1;
    int16_t i2;
    int16_t q2;
    int16_t q3;
    int16_t i3;

    uint32_t num_samples=0;
    uint32_t packed_offset=0;

    int32_t *data;

    if( include_meta == true )
    {
        packed_offset += SKIQ_RX_HEADER_SIZE_IN_WORDS;
    }

    // loop through all the samples and unpack them
    for( num_samples=0; num_samples < (num_unpacked_samples*2); num_samples+=8 )
    {
        // determine if the metadata needs to be skipped over in the unpacking
        if( (include_meta == true) && ((packed_offset % (block_size_in_words-1))==0) )
        {
            // increment over the header
            packed_offset += SKIQ_RX_HEADER_SIZE_IN_WORDS;
        }
        data = (int32_t*)(&(packed_data[packed_offset*2]));

        // unpack the data
        i0 = (data[0] & 0xFFF00000) >> 20;
        i0 = sign_extend(i0);
        q0 = (data[0] & 0x000FFF00) >> 8;
        q0 = sign_extend(q0);
        i1 = ((data[0] & 0x000000FF) << 4) | ((data[1] & 0xF0000000)>>28);
        i1 = sign_extend(i1);
        q1 = (data[1] & 0x0FFF0000) >> 16;
        q1 = sign_extend(q1);
        i2 = (data[1] & 0x0000FFF0) >> 4;
        i2 = sign_extend(i2);
        q2 = ((data[1] & 0x0000000F) << 8) | ((data[2] & 0xFF000000) >> 24);
        q2 = sign_extend(q2);
        i3 = ((data[2] & 0x00FFF000) >> 12);
        i3 = sign_extend(i3);
        q3 = (data[2] & 0x00000FFF);
        q3 = sign_extend(q3);

        // save the unpacked data
        unpacked_data[num_samples] = q0;
        unpacked_data[num_samples+1] = i0;
        unpacked_data[num_samples+2] = q1;
        unpacked_data[num_samples+3] = i1;
        unpacked_data[num_samples+4] = q2;
        unpacked_data[num_samples+5] = i2;
        unpacked_data[num_samples+6] = q3;
        unpacked_data[num_samples+7] = i3;

        // every 3 words of data contain 4 samples in packed mode
        packed_offset += 3;
    }

}

skiq_rf_port_t map_int_to_rf_port( uint32_t port )
{
    skiq_rf_port_t rf_port = skiq_rf_port_unknown;

    switch( port )
    {
        case 1:
            rf_port = skiq_rf_port_J1;
            break;
        case 2:
            rf_port = skiq_rf_port_J2;
            break;
        case 3:
            rf_port = skiq_rf_port_J3;
            break;
        case 4:
            rf_port = skiq_rf_port_J4;
            break;
        case 5:
            rf_port = skiq_rf_port_J5;
            break;
        case 6:
            rf_port = skiq_rf_port_J6;
            break;
        case 7:
            rf_port = skiq_rf_port_J7;
            break;
        case 300:
            rf_port = skiq_rf_port_J300;
            break;
        case 8:
            rf_port = skiq_rf_port_J8;
            break;
        default:
            rf_port = skiq_rf_port_unknown;
            break;
    }
    return (rf_port);
}


void close_open_files( FILE **p_files, uint8_t nr_handles )
{
    uint8_t i=0;
    for( i=0; i<nr_handles; i++ )
    {
        if ( p_files[i] != NULL )
        {
            fclose( p_files[i] );
        }
        p_files[i] = NULL;
    }
}
