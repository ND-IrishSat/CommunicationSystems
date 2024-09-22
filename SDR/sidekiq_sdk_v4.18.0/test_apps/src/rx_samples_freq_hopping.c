/*! \file rx_samples_freq_hopping.c
 * \brief This file contains a basic application for acquiring
 * a contiguous block of I/Q sample pairs for each frequency
 * specified in the hopping list.
 *  
 * <pre>
 * Copyright 2012-2019 Epiq Solutions, All Rights Reserved
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
#include <time.h>
#include <math.h>

#if (defined __MINGW32__)
#define OUTPUT_PATH_MAX                 260
#else
#include <linux/limits.h>
#define OUTPUT_PATH_MAX                 PATH_MAX
#endif

#define FREQ_CHAR_MAX_LEN               (21)

#include "sidekiq_api.h"
#include "arg_parser.h"

/* a simple pair of MACROs to round up integer division */
#define _ROUND_UP(_numerator, _denominator)    (_numerator + (_denominator - 1)) / _denominator
#define ROUND_UP(_numerator, _denominator)     (_ROUND_UP((_numerator),(_denominator)))

#define NUM_USEC_IN_MS (1000)

#define NUM_NANOSEC_IN_SEC (1000000000)

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- capture Rx data for each frequency specified";
static const char* p_help_long =
"\
Tune to the user-specifed Rx frequencies and acquire the specified number of\n\
words at the requested sample rate. Additional features such as gain, \n\
may be configured prior to data collection. Upon capturing the required \n\
number of samples, the data will be stored to a file for post analysis.\n\
\n\
The data is stored in the file as 16-bit I/Q pairs with 'Q' sample occurring\n\
first, followed by the 'I' sample, resulting in the following format:\n\
\n\
             -15--------------------------0-\n\
             |            Q0_A1            |\n\
   index 0   | (sign extended to 16 bits)  |\n\
             -------------------------------\n\
             |            I0_A1            |\n\
   index 1   | (sign extended to 16 bits)  |\n\
             -------------------------------\n\
             |            Q1_A1            |\n\
   index 2   | (sign extended to 16 bits)  |\n\
             -------------------------------\n\
             |            I1_A1            |\n\
   index 3   | (sign extended to 16 bits)  |\n\
             -------------------------------\n\
             |             ...             |\n\
             -------------------------------\n\
             |             ...             |\n\
             -15--------------------------0-\n\
\n\
Each sample is little-endian, twos-complement, signed, and sign-extended\n\
from 12 to 16-bits (when appropriate for the product).\n\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --handle=A1\n\
  --rate=1000000\n\
  --words_per_hop=100000\n\
  --hop-on-ts=false\n\
  --reset-on-1pps=false\n\
  --hop-ts-offset=0\
";

/* storage for all cmd line args */
static uint32_t num_payload_words_to_acquire = 100000;
static uint32_t sample_rate = 1000000;
static uint32_t bandwidth = UINT32_MAX;
static uint32_t rx_gain = UINT32_MAX;
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static char* p_hdl = "A1";
static char* p_file_path = NULL;
static uint32_t settle_time = 0; 
static char* p_hop_filename;
static bool hop_on_timestamp = false;
static bool reset_on_1pps = false;
static uint64_t hop_timestamp_offset = 0;
/* variable used to signal force quit of application */
static bool running = true;

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
    APP_ARG_REQ("destination",
                'd',
                "Prefix of files created to store RX samples created at each frequecny",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("gain",
                'g',
                "Manually configure the gain by index rather than using automatic",
                "index",
                &rx_gain,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("handle",
                0,
                "Rx handle to use, either A1, A2, B1, B2, C1, D1",
                "Rx",
                &p_hdl,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("bandwidth",
                'b',
                "Bandwidth in hertz",
                "Hz",
                &bandwidth,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("words",
                'w',
                "Number of sample words to acquire",
                "N",
                &num_payload_words_to_acquire,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("settle-time",
                0,
                "Amount of time (in ms) after hopping prior to receiving samples",
                NULL,
                &settle_time,
                UINT32_VAR_TYPE),
    APP_ARG_REQ("freq-hop-list",
                0,
                "Filename containing frequency hopping list (1 frequency per line in the file)",
                "{Hz}",
                &p_hop_filename,
                STRING_VAR_TYPE),
    APP_ARG_OPT("hop-on-ts",
                't',
                "Hop on timestamp",
                NULL,
                &hop_on_timestamp,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("reset-on-1pps",
                0,
                "Reset timestamps on 1PPS",
                NULL,
                &reset_on_1pps,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("hop-ts-offset",
                0,
                "Timestamp offset (added to number of samples per hop) before completing hop",
                NULL,
                &hop_timestamp_offset,
                UINT64_VAR_TYPE),    
    APP_ARG_TERMINATOR,
};

// parse the frequency hopping file
static int32_t parse_freq_hop_file( uint64_t freq_list[SKIQ_MAX_NUM_FREQ_HOPS], uint16_t *p_num_hop_freqs )
{
    FILE *p_hop_file = NULL;
    int32_t status=0;

    printf("Info: parsing frequency hopping file %s\n", p_hop_filename);
    p_hop_file = fopen( p_hop_filename, "r" );
    if( p_hop_file != NULL )
    {
        *p_num_hop_freqs = 0;
        // create an array of hopping frequencies from the file
        while ( ( 1 == fscanf(p_hop_file, "%" PRIu64, &(freq_list[*p_num_hop_freqs])) ) &&
	        (*p_num_hop_freqs<SKIQ_MAX_NUM_FREQ_HOPS) )
        {
            printf("Info: hopping freq[%u]=%" PRIu64" \n", *p_num_hop_freqs, freq_list[*p_num_hop_freqs]);
            *p_num_hop_freqs = (*p_num_hop_freqs) + 1;
        }
        fclose( p_hop_file );
    }
    else
    {
        status = -ENOENT;
    }

    return (status);
}

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
/** This is the a function that blocks until a specific RF timestamp is reached.

    @param card_: Sidekiq card of intereset
    @param hdl_: Sidekiq handle of interest
    @param rf_ts_: RF timestamp to wait for

    @return 0 upon success
*/
int32_t wait_until_after_rf_ts( uint8_t card_,
                                skiq_tx_hdl_t hdl_,
                                uint64_t rf_ts_ )
{
    int32_t status=0;
    uint64_t curr_ts=0;
    struct timespec ts;
    uint64_t num_nanosecs=0;
    uint32_t num_wait_secs=0;

    if( (status=skiq_read_curr_rx_timestamp(card_, hdl_, &curr_ts)) == 0 )
    {
        // if our current timestamp is already larger than what was requested,
        // just disable the stream immediately
        if( curr_ts < rf_ts_ )
        {
            // calculate how long to sleep for before checking our timestamp
            num_nanosecs = ceil((double)((double)(rf_ts_ - curr_ts) / (double)(sample_rate)) * NUM_NANOSEC_IN_SEC);
            while (num_nanosecs >= NUM_NANOSEC_IN_SEC)
            {
                num_nanosecs -= NUM_NANOSEC_IN_SEC;
                num_wait_secs++;
            }
            ts.tv_sec = num_wait_secs;
            ts.tv_nsec = (uint32_t)(num_nanosecs);
            while( (nanosleep(&ts, &ts) == -1) && (running==true) )
            {
                if ( (errno == ENOSYS) || (errno == EINVAL) ||
                     (errno == EFAULT) )
                {
                    fprintf(stderr, "Error: catastrophic timer failure (errno"
                        " %d)!\n", errno);
                    status = -errno;
                    break;
                }
            }

            if( status == 0 )
            {
                status = skiq_read_curr_rx_timestamp(card_, hdl_, &curr_ts);
            }

            while( (curr_ts < rf_ts_) && (running==true) && (status==0) )
            {
                // just sleep 1 uS and read the timestamp again...we should be close based on the initial sleep calculation
                usleep(1);
                status = skiq_read_curr_rx_timestamp(card_, hdl_, &curr_ts);
            }
        }
        if( status == 0 )
        {
            printf("Timestamp reached (curr=%" PRIu64 ")\n", curr_ts);
        }
    }

    return (status);
}

/*****************************************************************************/
/** This is the main function for executing the rx_samples_freq_hopping app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    skiq_rx_hdl_t hdl = skiq_rx_hdl_end;
    int32_t status = 0;
    pid_t owner = 0;
    skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
    uint64_t freq_list[SKIQ_MAX_NUM_FREQ_HOPS] = {0};
    uint16_t num_hop_freqs=0;
    skiq_rx_gain_t gain_mode;
    int32_t block_size_in_words = 0;
    int num_blocks = 0;
    uint32_t total_num_payload_words_acquired;
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    uint32_t* p_next_write;
    uint32_t num_words_written;
    uint32_t num_words_read=0;
    skiq_rx_status_t rx_status;
    skiq_rx_hdl_t curr_rx_hdl = skiq_rx_hdl_end;
    skiq_rx_block_t* p_rx_block;
    uint32_t len;
    uint32_t* p_rx_data;
    uint32_t* p_rx_data_start;
    uint64_t curr_ts;
    uint64_t next_ts;
    uint32_t rx_block_cnt;
    uint16_t i=0;
    char p_filename[OUTPUT_PATH_MAX];
    FILE *p_output_fp;
    skiq_freq_tune_mode_t tune_mode = skiq_freq_tune_mode_hop_immediate;
    uint64_t base_ts = 0;
    uint64_t hop_ts=0;
    uint16_t next_hop_index=0;
    uint64_t next_freq=0;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize everything based on the arguments provided */
    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    /*********************************************************/
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
        hdl = skiq_rx_hdl_A1;
        printf("Info: using Rx handle A1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "A2") )
    {
        hdl = skiq_rx_hdl_A2;
        chan_mode = skiq_chan_mode_dual;
        printf("Info: using Rx handle A2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B1") )
    {
        hdl = skiq_rx_hdl_B1;
        printf("Info: using Rx handle B1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B2") )
    {
        hdl = skiq_rx_hdl_B2;
        chan_mode = skiq_chan_mode_dual;
        printf("Info: using Rx handle B2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "C1") )
    {
        hdl = skiq_rx_hdl_C1;
        printf("Info: using Rx handle C1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "D1") )
    {
        hdl = skiq_rx_hdl_D1;
        printf("Info: using Rx handle D1\n");
    }
    else
    {
        printf("Error: invalid handle specified\n");
        exit(-1);
    }

    // if hop_on_timestamp set, update tune mode
    if( hop_on_timestamp == true )
    {
        tune_mode = skiq_freq_tune_mode_hop_on_timestamp;
    }

    status = parse_freq_hop_file( freq_list, &num_hop_freqs );
    if( status != 0 )
    {
        printf("Error: unable to process hop file properly\n");
        exit(-1);
    }

    // gain mode
    if (rx_gain != UINT32_MAX)
    {
        gain_mode = skiq_rx_gain_manual;
    }
    else
    {
        gain_mode = skiq_rx_gain_auto;
    }

    // bandwidth
    if (bandwidth == UINT32_MAX)
    {
        bandwidth = sample_rate;
    }

    // make sure the filename isn't too long, we'll append with the frequency value
    if( ( strlen(p_file_path) + FREQ_CHAR_MAX_LEN ) > OUTPUT_PATH_MAX )
    {
        printf("Error: filename is too long, must be less than %u\n",
               OUTPUT_PATH_MAX - FREQ_CHAR_MAX_LEN);
        exit(-1);
    }

    /*********************************************************/


    /*********************************************************/

    // initialize the card
    printf("Info: initializing card %" PRIu8 "...\n", card);
    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, &card, 1);
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

    // set the channel mode 
    status = skiq_write_chan_mode(card, chan_mode);
    if ( status != 0 )
    {
        printf("Error: failed to set Rx channel mode to %u with status %d (%s)\n", chan_mode, status, strerror(abs(status)));
    }

    // set the gain 
    status = skiq_write_rx_gain_mode(card, hdl, gain_mode);
    if (status != 0)
    {
        printf("Error: failed to set Rx gain mode\n");
    }
    else
    {
        printf("Info: configured %s gain mode\n",
               gain_mode == skiq_rx_gain_auto ? "auto" : "manual");
    }
    if( gain_mode == skiq_rx_gain_manual )
    {
        status=skiq_write_rx_gain(card, hdl, rx_gain);
        if (status != 0)
        {
            printf("Error: failed to set gain index to %" PRIu8 "\n",rx_gain);
        }
        else
        {
            printf("Info: set gain index to %" PRIu8 "\n", rx_gain);
        }
    }

    /* set the sample rate and bandwidth */
    status=skiq_write_rx_sample_rate_and_bandwidth(card, hdl, sample_rate, bandwidth);
    if (status != 0)
    {
        printf("Error: failed to set Rx sample rate or bandwidth(using default configuration)...status is %d\n",
               status);
    }
    status=skiq_read_rx_sample_rate_and_bandwidth(card,
                                                  hdl,
                                                  &read_sample_rate,
                                                  &actual_sample_rate,
                                                  &read_bandwidth,
                                                  &actual_bandwidth);
    if( status == 0 )
    {
        printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
               actual_sample_rate, actual_bandwidth);
    }

    /* read the expected RX block size and convert to number of words: ( words = bytes / 4 )*/
    block_size_in_words = skiq_read_rx_block_size( card, skiq_rx_stream_mode_high_tput ) / 4;
    block_size_in_words -= SKIQ_RX_HEADER_SIZE_IN_WORDS; 

    /* set up the # of blocks to acquire according to the cmd line args */
    num_blocks = ROUND_UP(num_payload_words_to_acquire, block_size_in_words);
    printf("Info: num blocks to acquire is %d with block size of %u\n",
           num_blocks, block_size_in_words);

    // buffer allocation
    p_rx_data = (uint32_t*)calloc(block_size_in_words * num_blocks, sizeof(uint32_t));
    if (p_rx_data == NULL)
    {
        printf("Error: didn't successfully allocate %d words to hold"
               " unpacked iq\n", block_size_in_words*num_blocks);
        skiq_exit();
        return(-3);
    }
    memset( p_rx_data, 0, block_size_in_words * num_blocks * sizeof(uint32_t) );
    p_next_write = p_rx_data;
    p_rx_data_start = p_rx_data;
    /*********************************************************/

    // configure frequency hopping
    if( (status=skiq_write_rx_freq_tune_mode( card, hdl, tune_mode )) == 0 )
    {
        printf("Info: successfully configured tune mode to %s\n",
               tune_mode == skiq_freq_tune_mode_hop_immediate ? "immediate" : "on timestamp");
    }
    else
    {
        printf("Error: failed to set tune mode status=%d\n", status);
        free( p_rx_data );
        skiq_exit();
        return (-1);
    }
    // configure the hopping list
    if( (status=skiq_write_rx_freq_hop_list( card, hdl, num_hop_freqs, freq_list, 0 )) == 0 )
    {
        printf("Info: successfully set hop list\n");
    }
    else
    {
        printf("Error: failed to set hop list %d\n", status);
        skiq_exit();
        free( p_rx_data );
        return (-1);
    }

    // reset the timestamps and wait for the reset to complete
    if( skiq_read_curr_rx_timestamp( card, hdl, &base_ts ) == 0 )
    {
        printf("Resetting timestamps (base=%" PRIu64 ")\n", base_ts);
        if( reset_on_1pps == true )
        {
            printf("Resetting on 1PPS\n");
            if( skiq_write_timestamp_reset_on_1pps( card, 0 ) != 0 )
            {
                printf("Error: unable to reset timestamp on 1PPS\n");
            }
        }
        else
        {
            printf("Resetting async\n");
            if( skiq_reset_timestamps( card ) != 0 )
            {
                printf("Error: unable to reset timestamps\n");
            }
        }
        if( skiq_read_curr_rx_timestamp( card, hdl, &curr_ts ) != 0 )
        {
            printf("Error: unable to read current timestamp\n");
        }
    }

    printf("Waiting for reset complete (base=%" PRIu64 "), (curr=%" PRIu64 ")\n", base_ts, curr_ts);
    while( (base_ts < curr_ts) && (running==true) )
    {
        // just sleep 100 uS and read the timestamp again...we should be close based on the initial sleep calculation
        usleep(100);
        skiq_read_curr_rx_timestamp(card, hdl, &curr_ts);
    }
    printf("Resetting timestamp complete (current=%" PRIu64 ")\n", curr_ts);
    
    // receive data for each frequency
    for( i=0; (i<num_hop_freqs) && (running==true); i++ )
    {
        if( (i+1) < num_hop_freqs )
        {
            if( (status=skiq_write_next_rx_freq_hop( card, hdl, i+1 )) != 0 )
            {
                printf("Error: failed to write hop with status %d\n", status);
            }
        }
        else
        {
            // just prime "next" at 0
            if( (status=skiq_write_next_rx_freq_hop( card, hdl, 0 )) != 0 )
            {
                printf("Error: unable to write next hop with status %d\n", status);
            }
        }

        if( skiq_read_next_rx_freq_hop( card,
                                        hdl,
                                        &next_hop_index,
                                        &next_freq ) == 0 )
        {
            printf("Info: next hop frequency is %" PRIu64 " Hz at index %" PRIu16 "\n",
                   next_freq, next_hop_index);
        }
        
        // this executes the hop for the current i (configured on last loop iteration)
        hop_ts += hop_timestamp_offset;
        if( (status=skiq_perform_rx_freq_hop( card, hdl, hop_ts )) == 0 )
        {
            printf("Info: successfully performed hop\n");
        }
        else
        {
            printf("Error: failed to hop with status %d\n", status);
        }

        // read the current filter path
        skiq_filt_t filt;
        if( skiq_read_rx_preselect_filter_path( card, hdl, &filt ) == 0 )
        {
            printf("Info: current filter is %s\n", SKIQ_FILT_STRINGS[filt]);
        }
        else
        {
            printf("Error: unable to read current filter configuration\n");
        }

        if( settle_time != 0 )
        {
            printf("Info: waiting %" PRIu32 " ms prior to streaming\n", settle_time);
            usleep(NUM_USEC_IN_MS*settle_time);
        }

        // wait until we've reached our timestamp before starting streaming
        if( wait_until_after_rf_ts( card, hdl, hop_ts ) != 0 )
        {
            printf("Never received timestamp %" PRIu64 "\n", hop_ts);
            _exit(-1);
        }

        // start streaming
        status = skiq_start_rx_streaming_multi_immediate( card, &hdl, 1 );
        if( status != 0 )
        {
            printf("Error: receive streaming failed to start with status code %d\n", status);
            running = false;
        }

        if ( running )
        {
            // reset variables to accumulate for the file
            printf("Resetting data capture variables for next frequency\n");
            next_ts = (uint64_t) 0;
            rx_block_cnt = 0;
            total_num_payload_words_acquired = 0;
            p_next_write = p_rx_data_start;
        }

        /* loop through and acquire the requested # of data words, verifying
           that the timestamp (ts) increments as expected */
        while( (running==true) && (total_num_payload_words_acquired<num_payload_words_to_acquire) )
        {
            rx_status = skiq_receive(card, &curr_rx_hdl, &p_rx_block, &len);
            if ( skiq_rx_status_success == rx_status )
            {
                if( curr_rx_hdl != hdl )
                {
                    printf("Error: received unexpected data from unspecified hdl %u\n", curr_rx_hdl);
                    skiq_exit();
                    free( p_rx_data );
                    exit(-4);
                }
                if ( NULL != p_rx_block )
                {
                    /* verify timestamp and data */
                    curr_ts = p_rx_block->rf_timestamp;
                    
                    if (next_ts == 0)
                    {
                        printf("Got first timestamp %" PRIu64 " for handle %u\n",
                               curr_ts, curr_rx_hdl);
                        /*
                          will be incremented properly below for next time through
                        */
                        next_ts = curr_ts;
                    }
                    else if (curr_ts != next_ts)
                    {
                        printf("Error: timestamp error in block %d for %u..."
                               "expected 0x%016" PRIx64 " but got 0x%016" PRIx64
                               " (delta %" PRId64 ")\n",        \
                               rx_block_cnt, curr_rx_hdl,
                               next_ts, curr_ts,
                               curr_ts - next_ts);
                        skiq_exit();
                        free( p_rx_data );
                        exit(-1);
                    }
                    num_words_read = len/4; /* len is in bytes */
                    
                    /* copy over all the data if this isn't the last block */
                    if( (total_num_payload_words_acquired + block_size_in_words) < num_payload_words_to_acquire )
                    {
                        num_words_read = num_words_read - SKIQ_RX_HEADER_SIZE_IN_WORDS;
                        memcpy(p_next_write,
                               (void *)p_rx_block->data,
                               (num_words_read)*sizeof(uint32_t));
                        p_next_write += (num_words_read);
                        // update the # of words received and num samples received
                        total_num_payload_words_acquired += num_words_read;
                        rx_block_cnt++;
                    }
                    else
                    {
                        /* determine the number of words still remaining */
                        uint32_t last_block_num_payload_words =
                            num_payload_words_to_acquire - total_num_payload_words_acquired;
                        uint32_t num_words_to_copy=0;
                        
                        num_words_to_copy = last_block_num_payload_words;
                        
                        memcpy(p_next_write,
                               (void *)p_rx_block->data,
                           num_words_to_copy*sizeof(uint32_t));
                        p_next_write += num_words_to_copy;

                        total_num_payload_words_acquired += last_block_num_payload_words;
                        rx_block_cnt++;
                    }
                    // increment the next expected timestamp
                    next_ts += (num_words_read);
                } // end p__block != NULL
            } // rx_status == success
        } // end receiving samples
        skiq_stop_rx_streaming_multi_immediate(card, &hdl, 1);

        // open the file to save the samples
        snprintf( p_filename, OUTPUT_PATH_MAX, "%s_%" PRIu64, p_file_path, freq_list[i] );
        p_output_fp = fopen(p_filename, "wb");
        if( (p_output_fp != NULL) && (running==true) )
        {
            num_words_written=0;
            num_words_written = fwrite(p_rx_data_start,
                                       sizeof(uint32_t),
                                       total_num_payload_words_acquired,
                                       p_output_fp);
            if( num_words_written < total_num_payload_words_acquired )
            {
                printf("Info: attempt to write %d words to output file but only wrote %d\n",
                       total_num_payload_words_acquired,
                       num_words_written);
                status = -EIO;
            }
            else
            {
                printf("Info: successfully saved %u samples to file %s\n\n",
                       num_words_written, p_filename);
                status = 0;
            }
            fclose(p_output_fp);
        }
        else
        {
            printf("Error: unable to open file %s (errno=%d) to save samples or application interrupted\n",
                   p_filename, errno );
            status = -EIO;
        }
        hop_ts += total_num_payload_words_acquired;
    } // end of frequency hopping list

    skiq_exit();
    free(p_rx_data);

    return (status);
}
