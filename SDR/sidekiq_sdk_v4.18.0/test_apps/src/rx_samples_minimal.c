/*! \file rx_samples_minimal.c
 * \brief This file contains a basic application for acquiring
 * a contiguous block of I/Q sample pairs in the most efficient
 * manner possible.
 *  
 * <pre>
 * Copyright 2012-2020 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>  //for usleep
#include <errno.h>
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

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

#ifndef DEFAULT_LO_FREQUENCY
#   define DEFAULT_LO_FREQUENCY         850000000
#endif

#ifndef DEFAULT_HANDLE
#   define DEFAULT_HANDLE               A1
#endif

#ifndef DEFAULT_SAMPLE_RATE
#   define DEFAULT_SAMPLE_RATE          1000000
#endif

#ifndef DEFAULT_BANDWIDTH
#   define DEFAULT_BANDWIDTH            800000
#endif

#ifndef DEFAULT_CAPTURE_SAMPLES
#   define DEFAULT_CAPTURE_SAMPLES      1000000
#endif

#define NUM_USEC_IN_MS (1000)

/* Delimeter used when parsing lists provided as input */
#define TOKEN_LIST ","

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- capture Rx data";
static const char* p_help_long =
"\
Tune to the user-specifed Rx frequency and acquire the specified number of\n\
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
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --frequency=" xstr(DEFAULT_LO_FREQUENCY) "\n\
  --handle=" xstr(DEFAULT_HANDLE) "\n\
  --rate=" xstr(DEFAULT_SAMPLE_RATE) "\n\
  --words=" xstr(DEFAULT_CAPTURE_SAMPLES) "\
";

static const char* p_file_suffix[skiq_rx_hdl_end] = { ".a1", ".a2", ".b1", ".b2", ".c1", ".d1" };
/* storage for all cmd line args */
static uint32_t num_payload_words_to_acquire = DEFAULT_CAPTURE_SAMPLES;
static uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
static uint32_t bandwidth = DEFAULT_BANDWIDTH;
static uint32_t rx_gain = UINT32_MAX;
static uint8_t card = UINT8_MAX;
static char* p_hdl = xstr(DEFAULT_HANDLE);
static char* p_rates = xstr(DEFAULT_SAMPLE_RATE);
static bool rate_list_specified = false;
static char* p_bws = xstr(DEFAULT_BANDWIDTH);
static bool bw_list_specified = false;
static char *p_freqs = xstr(DEFAULT_LO_FREQUENCY);
static bool freq_list_specified = false;
static char* p_file_path = NULL;
static bool align_samples = true;
/* variable used to signal force quit of application */
static bool running = true;
static uint32_t settle_time = 0;

static char p_filename[OUTPUT_PATH_MAX];
static ssize_t filename_len = 0;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("card",
                'c',
                "Specify Sidekiq by card index",
                "ID",
                &card,
                UINT8_VAR_TYPE),
    APP_ARG_REQ("destination",
                'd',
                "Output file to store Rx data",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT_PRESENT("frequency",
                        'f',
                        "Comma delimited list of frequencies in Hz corresponding to the handle list",
                        "Hz",
                        &p_freqs,
                        STRING_VAR_TYPE,
                        &freq_list_specified),
    APP_ARG_OPT("gain",
                'g',
                "Manually configure the gain by index rather than using automatic",
                "index",
                &rx_gain,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("handle",
                0,
                "Comma delimited list of Rx handles to enable",
                "Rx",
                &p_hdl,
                STRING_VAR_TYPE),
    APP_ARG_OPT_PRESENT("rate",
                        'r',
                        "Comma delimited list of sample rates corresponding to the handle list",
                        "Hz",
                        &p_rates,
                        STRING_VAR_TYPE,
                        &rate_list_specified),
    APP_ARG_OPT_PRESENT("bandwidth",
                        'b',
                        "Comma delimited list of bandwiths corresponding to the handle list",
                        "Hz",
                        &p_bws,
                        STRING_VAR_TYPE,
                        &bw_list_specified),
    APP_ARG_OPT("words",
                'w',
                "Number of I/Q sample words to acquire",
                "N",
                &num_payload_words_to_acquire,
                UINT32_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

/* local functions */
static void print_block_contents( skiq_rx_block_t* p_block,
                                  int32_t block_size_in_bytes );
static void close_open_files( FILE **p_files, uint8_t nr_handles );
static int32_t parse_hdl_list( char *handle_str,
                               skiq_rx_hdl_t rx_handles[],
                               uint8_t *p_nr_handles,
                               skiq_chan_mode_t *p_chan_mode );
static int32_t parse_rate_list( char *rate_str,
                                uint32_t rx_rates[],
                                uint8_t *p_nr_rates);

static int32_t parse_bandwidth_list( char *bw_str,
                                     uint32_t rx_bandwidths[],
                                     uint8_t *p_nr_bws);

static int32_t parse_freq_list( char *freq_str,
                                uint64_t rx_freqs[],
                                uint8_t *p_nr_freqs);

static skiq_rx_hdl_t str2hdl( const char *str )
{
    return ( 0 == strcasecmp( str, "A1" ) ) ? skiq_rx_hdl_A1 :
        ( 0 == strcasecmp( str, "A2" ) ) ? skiq_rx_hdl_A2 :
        ( 0 == strcasecmp( str, "B1" ) ) ? skiq_rx_hdl_B1 :
        ( 0 == strcasecmp( str, "B2" ) ) ? skiq_rx_hdl_B2 :
        ( 0 == strcasecmp( str, "C1" ) ) ? skiq_rx_hdl_C1 :
        ( 0 == strcasecmp( str, "D1" ) ) ? skiq_rx_hdl_D1 :
        skiq_rx_hdl_end;
}

static const char *
hdl_cstr( skiq_rx_hdl_t hdl )
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


/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
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
    skiq_rx_gain_t gain_mode;
    skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
    skiq_rx_stream_mode_t stream_mode = skiq_rx_stream_mode_high_tput;
    int32_t block_size_in_words = 0;
    int num_blocks = 0;
    uint32_t total_num_payload_words_acquired[skiq_rx_hdl_end];
    uint32_t rx_block_cnt[skiq_rx_hdl_end];
    int32_t status=0;
    skiq_rx_status_t rx_status;
    uint32_t* p_next_write[skiq_rx_hdl_end];
    uint32_t num_words_written;
    uint32_t num_words_read=0;
    skiq_rx_block_t* p_rx_block;
    uint32_t len;
    uint32_t* p_rx_data[skiq_rx_hdl_end] = {NULL};
    uint32_t* p_rx_data_start[skiq_rx_hdl_end] = {NULL};
    skiq_rx_hdl_t curr_rx_hdl = skiq_rx_hdl_end;
    bool last_block[skiq_rx_hdl_end];
    uint8_t num_hdl_rcv = 0;
    bool rx_overload[skiq_rx_hdl_end];
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    pid_t owner = 0;

    /* keep track of the specified handles */
    skiq_rx_hdl_t handles[skiq_rx_hdl_end];
    uint8_t nr_handles = 0;
    uint32_t rates[skiq_rx_hdl_end];
    uint32_t bandwidths[skiq_rx_hdl_end];
    uint64_t freqs[skiq_rx_hdl_end];
    uint8_t nr_rates, nr_bws, nr_freqs = 0;
    uint8_t i;

    uint32_t payload_words=0;
    uint32_t words_received[skiq_rx_hdl_end];

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize all the handles to be invalid */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        last_block[curr_rx_hdl] = false;
        rx_overload[curr_rx_hdl] = false;
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

    if (UINT8_MAX == card)
    {
        card = DEFAULT_CARD_NUMBER;
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        printf("Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return (-1);
    }

    /* map argument values to Sidekiq specific variable values */
    status = parse_hdl_list( p_hdl, handles, &nr_handles, &chan_mode );
    if ( status == 0 )
    {
        if ( nr_handles == 0 )
        {
            fprintf(stderr, "Error: invalid number of handles specified (must be greater than zero)\n");
            status = -1;
            goto cleanup;
        }
    }
    else
    {
        fprintf(stderr, "Error: invalid handle list specified: '%s'\n", p_hdl);
        goto cleanup;
    }

    /* map argument values to Sidekiq specific variable values */
    if ( rate_list_specified )
    {
        status = parse_rate_list( p_rates, rates, &nr_rates);
        if ( status == 0 )
        {
            if ( nr_rates != nr_handles )
            {
                fprintf(stderr, "Error: for each handle, a rate must be specified\n");
                status = -1;
                goto cleanup;
            }
        }
        else
        {
            fprintf(stderr, "Error: invalid rate list specified: '%s'\n", p_rates);
            goto cleanup;
        }
    }
    else
    {
        /* if there's no rate list on the command line, fill out `rates` with the
         * `DEFAULT_SAMPLE_RATE` */
        nr_rates = nr_handles;
        for ( i = 0; i < nr_rates; i++ )
        {
            rates[i] = DEFAULT_SAMPLE_RATE;
        }
    }

    if ( bw_list_specified )
    {
        status = parse_bandwidth_list( p_bws, bandwidths, &nr_bws);
        if ( status == 0 )
        {
            if ( nr_bws != nr_handles )
            {
                fprintf(stderr, "Error: for each handle, a bandwidth must be specified\n");
                status = -1;
                goto cleanup;
            }
        }
        else
        {
            fprintf(stderr, "Error: invalid bandwidth list specified: '%s'\n", p_bws);
            goto cleanup;
        }
    }
    else
    {
        /* if there's no bandwidth list on the command line, fill out `bandwidths` with the
         * `DEFAULT_BANDWIDTH` */
        nr_bws = nr_handles;
        for ( i = 0; i < nr_bws; i++ )
        {
            bandwidths[i] = DEFAULT_BANDWIDTH;
        }
    }

    if ( freq_list_specified )
    {
        status = parse_freq_list( p_freqs, freqs, &nr_freqs);
        if ( status == 0 )
        {
            if ( nr_freqs != nr_handles )
            {
                fprintf(stderr, "Error: for each handle, a frequency must be specified\n");
                status = -1;
                goto cleanup;
            }
        }
        else
        {
            fprintf(stderr, "Error: invalid frequency list specified: '%s'\n", p_freqs);
            goto cleanup;
        }
    }
    else
    {
        /* if there's no frequency list on the command line, fill out `freqs` with the
         * `DEFAULT_LO_FREQUENCY` */
        nr_freqs = nr_handles;
        for ( i = 0; i < nr_freqs; i++ )
        {
            freqs[i] = DEFAULT_LO_FREQUENCY;
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
        goto cleanup;
    }
    printf("Info: initialized card %d\n", card);

    // reset the timestamps if align requested
    if( align_samples == true )
    {
        printf("Info: resetting all timestamps!\n");
        skiq_reset_timestamps(card);
    }

    /* iterate over all of the specified handles, opening a file for each one */
    for ( i = 0; i < nr_handles; i++ )
    {
        curr_rx_hdl = handles[i];
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
            exit(-1);
        }
        printf("Info: opened file %s for output\n",p_filename);
    }

    /* set the gain_mode based on rx_gain from the arguments */
    if (rx_gain != UINT32_MAX)
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

    /* write the channel mode (dual if A2 is being used) */
    status = skiq_write_chan_mode(card, chan_mode);
    if ( status != 0 )
    {
        printf("Error: failed to set Rx channel mode to %u with status %d (%s)\n", chan_mode, status, strerror(abs(status)));
    }

    status=skiq_write_rx_sample_rate_and_bandwidth_multi(card, handles, nr_handles, rates, bandwidths);
    if (status != 0)
    {
        printf("Error: failed to set Rx sample rate or bandwidth...status is %d\n",
                status);
    }

    for ( i = 0; i < nr_handles; i++ )
    {
        curr_rx_hdl = handles[i];

        /* initialize requested handle */
        num_hdl_rcv++;

        status=skiq_read_rx_sample_rate_and_bandwidth(card,
                                                      curr_rx_hdl,
                                                      &read_sample_rate,
                                                      &actual_sample_rate,
                                                      &read_bandwidth,
                                                      &actual_bandwidth);

        if( status == 0 )
        {
            printf("Info: requested sample rate is %u, requested bandwidth is %u\n",
                   rates[i], bandwidths[i]);
            printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
                   actual_sample_rate, actual_bandwidth);
        }

        if ((actual_sample_rate != rates[i]) || (actual_bandwidth < bandwidths[i]))
        {
            printf("Sample rate or bandwidth does not match the requested value.\n" );
            goto cleanup;
        }


        /* tune the Rx chain to the requested freq */
        status = skiq_write_rx_LO_freq(card, curr_rx_hdl, freqs[i]);
        if (status != 0)
        {
            printf("Error: failed to set LO freq (using previous LO freq)...status is %d\n",
                   status);
        }
        printf("Info: configured Rx LO freq to %" PRIu64 " Hz\n", freqs[i]);

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
            printf("Info: set gain index to %" PRIu8 "\n", rx_gain);
        }
    }

    /* read the expected RX block size and convert to number of words: ( words = bytes / 4 )*/
    block_size_in_words = skiq_read_rx_block_size( card, stream_mode ) / 4;
    if ( block_size_in_words < 0 )
    {
        fprintf(stderr, "Error: Failed to read RX block size for specified stream mode with status %d\n", block_size_in_words);
        goto cleanup;
    }

    //not using packed mode
    payload_words = block_size_in_words - SKIQ_RX_HEADER_SIZE_IN_WORDS;

    printf("Info: acquiring %d words at %d words per block\n", num_payload_words_to_acquire, payload_words);

    /* set up the # of blocks to acquire according to the cmd line args */
    num_blocks = ROUND_UP(num_payload_words_to_acquire, payload_words);
    printf("Info: num blocks to acquire is %d\n",num_blocks);

    // we're not including metadata
    block_size_in_words -= SKIQ_RX_HEADER_SIZE_IN_WORDS;


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
            goto cleanup;
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
        rx_block_cnt[curr_rx_hdl] = 0;
        total_num_payload_words_acquired[curr_rx_hdl] = 0;
    }

    if( settle_time != 0 )
    {
        printf("Info: waiting %" PRIu32 " ms prior to streaming\n", settle_time);
        usleep(NUM_USEC_IN_MS*settle_time);
    }


    printf( "Info: starting %u Rx interface(s)\n", nr_handles );
    status = skiq_start_rx_streaming_multi_immediate(card, handles, nr_handles);
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
                goto cleanup;
            }
            if ( NULL != p_rx_block )
            {
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

                num_words_read = len/4; /* len is in bytes */

                /* copy over all the data if this isn't the last block */
                if( (total_num_payload_words_acquired[curr_rx_hdl] + payload_words) < num_payload_words_to_acquire )
                {

                    num_words_read = num_words_read - SKIQ_RX_HEADER_SIZE_IN_WORDS;
                    memcpy(p_next_write[curr_rx_hdl],
                            (void *)p_rx_block->data,
                            (num_words_read)*sizeof(uint32_t));
                    p_next_write[curr_rx_hdl] += (num_words_read);

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

                       //not running in packed mode
                        uint32_t num_words_to_copy = last_block_num_payload_words;

                        // metadata is not included
                        memcpy(p_next_write[curr_rx_hdl],
                                (void *)p_rx_block->data,
                                num_words_to_copy*sizeof(uint32_t));
                        p_next_write[curr_rx_hdl] += num_words_to_copy;
                        total_num_payload_words_acquired[curr_rx_hdl] += \
                            last_block_num_payload_words;
                        rx_block_cnt[curr_rx_hdl]++;

                        num_hdl_rcv--;
                        last_block[curr_rx_hdl] = true;

                        words_received[curr_rx_hdl] += num_words_to_copy;
                    }
                }
            }
        }
    }

    /* all done, so stop streaming */
    printf( "Info: stopping %u Rx interface(s)\n", nr_handles );
    skiq_stop_rx_streaming_multi_immediate(card, handles, nr_handles);

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
            printf("Info: attempt to write %d words to output file but only"
                   " wrote %d\n", words_received[curr_rx_hdl],
                   num_words_written);
        }
    }
    printf("Info: Done without errors!\n");

cleanup:
    /* clean up */
    skiq_exit();
    close_open_files( output_fp, nr_handles );
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        if (p_rx_data_start[curr_rx_hdl] != NULL)
        {
            free(p_rx_data_start[curr_rx_hdl]);
        }
    }
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

static int32_t
parse_hdl_list( char *handle_str,
                skiq_rx_hdl_t rx_handles[],
                uint8_t *p_nr_handles,
                skiq_chan_mode_t *p_chan_mode )
{
    bool handle_requested[skiq_rx_hdl_end];
    skiq_rx_hdl_t rx_hdl;
    char *token, *handle_str_dup;

    *p_nr_handles = 0;

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
            fprintf(stderr, "Error: invalid handle specified: %s\n",hdl_cstr(rx_hdl));
            free( handle_str_dup );
            return -EPROTO;
        }
        else
        {
            if (handle_requested[rx_hdl] == true)
            {
                fprintf(stderr, "Error: handle specified multiple times: %s\n",hdl_cstr(rx_hdl));
                free( handle_str_dup );
                return -EPROTO;
            }
            else
            {
                handle_requested[rx_hdl] = true;
            }
            
            if ((*p_nr_handles) < skiq_rx_hdl_end)
            {
                rx_handles[(*p_nr_handles)++] = rx_hdl;
            }
        }

        token = strtok( NULL, TOKEN_LIST );
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
}

static int32_t
parse_rate_list( char *rate_str,
                uint32_t rx_rates[],
                uint8_t *p_nr_rates)
{
    char *token, *handle_str_dup;

    *p_nr_rates = 0;

    handle_str_dup = strdup( rate_str );
    if ( handle_str_dup == NULL )
    {
        return -ENOMEM;
    }

    token = strtok( handle_str_dup, TOKEN_LIST );
    while ( token != NULL )
    {
        if ((*p_nr_rates) < skiq_rx_hdl_end)
        {
            rx_rates[(*p_nr_rates)++] = (uint32_t)(strtoul(token,NULL,10));
        }
        token = strtok( NULL, TOKEN_LIST );
    }

    free( handle_str_dup );

    return 0;
}

static int32_t
parse_bandwidth_list( char *bw_str,
                uint32_t rx_bandwidths[],
                uint8_t *p_nr_bws)
{
    char *token, *handle_str_dup;

    *p_nr_bws = 0;

    handle_str_dup = strdup( bw_str );
    if ( handle_str_dup == NULL )
    {
        return -ENOMEM;
    }

    token = strtok( handle_str_dup, TOKEN_LIST );
    while ( token != NULL )
    {
        if ((*p_nr_bws) < skiq_rx_hdl_end )
        {
            rx_bandwidths[(*p_nr_bws)++] = (uint32_t)(strtoul(token,NULL,10));
        }
        token = strtok( NULL, TOKEN_LIST );
    }

    free( handle_str_dup );

    return 0;
}

static int32_t
parse_freq_list( char *freq_str,
                uint64_t rx_freqs[],
                uint8_t *p_nr_freqs)
{
    char *token, *handle_str_dup;

    *p_nr_freqs = 0;

    handle_str_dup = strdup( freq_str );
    if ( handle_str_dup == NULL )
    {
        return -ENOMEM;
    }

    token = strtok( handle_str_dup, TOKEN_LIST );
    while ( token != NULL )
    {
        rx_freqs[(*p_nr_freqs)++] = atoi(token);
        token = strtok( NULL, TOKEN_LIST );
    }

    free( handle_str_dup );

    return 0;
}
