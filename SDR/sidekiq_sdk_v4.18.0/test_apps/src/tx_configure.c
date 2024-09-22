/*! \file tx_configure.c
 * \brief This file contains a basic application for transmitting a
 * continuous tone at the specified frequency.
 *
 * <pre>
 * Copyright 2014 - 2019 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <strings.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <sidekiq_api.h>
#include "arg_parser.h"

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

#ifndef DEFAULT_TX_FREQUENCY
#   define DEFAULT_TX_FREQUENCY  850000000
#endif

#ifndef DEFAULT_HOP_DWELL_TIME
#   define DEFAULT_HOP_DWELL_TIME (1)
#endif

#define DUAL_TX_BLOCKSIZE (1022)

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- transmit test tone";
static const char* p_help_long =
"\
Tune to the user-specified Tx frequency and transmit a test tone \n\
using the RF IC's built in test tone.  The test tone is transmitted \n\
for the user specified duration (or indefinitely by default).\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --frequency=" xstr(DEFAULT_TX_FREQUENCY) "\n\
  --handle=A1\n\
  --attenuation=0\n\
  --quadcal-mode=auto\n\
  --dwell-time=" xstr(DEFAULT_HOP_DWELL_TIME) "\n\
  --hop-on-ts=false\n\
  --reset-on-1pps=false\n\
  --hop-ts-offset=0\
";

static bool running=true;
static uint8_t card=UINT8_MAX;
static char* p_serial = NULL;
static uint64_t lo_freq = DEFAULT_TX_FREQUENCY;
static uint16_t tx_atten = 0;
static uint32_t num_secs = 0;
static char* p_hdl = "A1";
static uint32_t sample_rate = UINT32_MAX;
static uint32_t bandwidth = UINT32_MAX;
static char* p_quadcal = "auto";
static int32_t test_freq_offset = INT32_MAX;
static uint32_t dwell_time = DEFAULT_HOP_DWELL_TIME;
static char* p_hop_filename;
static bool hop_on_timestamp = false;
static bool reset_on_1pps = false;
static uint64_t hop_timestamp_offset = 0;
static bool rfic_pin_enable = false;

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
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to receive samples at in Hertz",
                "Hz",
                &lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("attenuation",
                'a',
                "Output attenuation in quarter dB steps",
                "dB",
                &tx_atten,
                UINT16_VAR_TYPE),
    APP_ARG_OPT("time",
                't',
                "Duration of test tone transmission",
                "seconds",
                &num_secs,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("handle",
                0,
                "Tx handle to use, either A1, A2, B1, or B2",
                "Tx",
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
                "Bandwidth in Hertz",
                "Hz",
                &bandwidth,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("quadcal-mode",
                'q',
                "TX quadrature calibration mode (auto or manual)",
                "quadcal",
                &p_quadcal,
                STRING_VAR_TYPE),
    APP_ARG_OPT("test-freq-offset",
                'o',
                "Frequency offset (in Hz) of test tone from LO (not available for all products)",
                "Hz",
                &test_freq_offset,
                INT32_VAR_TYPE),
    APP_ARG_OPT("dwell-time",
                0,
                "Time to dwell at a specific frequency hop index (only if the hopping list and hopping immediate is specified)",
                "seconds",
                &dwell_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("freq-hop-list",
                0,
                "Filename containing frequency hopping list (1 frequency per line in the file)",
                "{Hz}",
                &p_hop_filename,
                STRING_VAR_TYPE),
    APP_ARG_OPT("hop-on-ts",
                0,
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
                "Timestamp offset between hops",
                NULL,
                &hop_timestamp_offset,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("rfic-pin-control",
                0,
                "RFIC Tx/Rx enabled by gpio pins",
                NULL,
                &rfic_pin_enable,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR
};

/* local functions */

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
            printf("Info: hopping freq[%u]=%" PRIu64 "\n", *p_num_hop_freqs, freq_list[*p_num_hop_freqs]);
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

static const char *
tx_hdl_cstr( skiq_rx_hdl_t hdl )
{
    const char* p_hdl_str =
        (hdl == skiq_rx_hdl_A1) ? "A1" :
        (hdl == skiq_rx_hdl_A2) ? "A2" :
        (hdl == skiq_rx_hdl_B1) ? "B1" :
        (hdl == skiq_rx_hdl_B2) ? "B2" :
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
    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);
    running = false;
}

/*****************************************************************************/
/** This is the a function that blocks until a specific timestamp is reached.

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
                status = skiq_read_curr_tx_timestamp(card_, hdl_, &curr_ts);
            }

            while( (curr_ts < rf_ts_) && (running==true) && (status==0) )
            {
                // just sleep 1 uS and read the timestamp again...we should be close based on the initial sleep calculation
                usleep(1);
                status = skiq_read_curr_tx_timestamp(card_, hdl_, &curr_ts);
            }
        }
        printf("Timestamp reached (curr=%" PRIu64 ")\n", curr_ts);
    }

    return (status);
}


/*****************************************************************************/
/** This is the main function for executing the tx_configure app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    uint64_t read_test_freq = 0;
    skiq_tx_hdl_t hdl, hdl_other = skiq_tx_hdl_end;
    int32_t status = 0;
    pid_t owner = 0;
    skiq_tx_quadcal_mode_t tx_cal_mode;
    skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
    uint16_t block_size=1020;
    double actual_rate;
    uint32_t actual_bandwidth;
    uint64_t freq_list[SKIQ_MAX_NUM_FREQ_HOPS] = {0};
    uint16_t num_hop_freqs=0;
    skiq_freq_tune_mode_t tune_mode = skiq_freq_tune_mode_hop_immediate;
    uint64_t curr_ts = 0;
    uint64_t base_ts = 0;
    uint64_t hop_ts = 0;

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
        if (0 != status)
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
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return (-1);
    }

    /* map argument values to sidekiq specific variable values */
    if( 0 == strcasecmp(p_hdl, "A1") )
    {
        hdl = skiq_tx_hdl_A1;
        printf("Info: using Tx handle A1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "A2") )
    {
        hdl = skiq_tx_hdl_A2;
        hdl_other = skiq_tx_hdl_A1;
        chan_mode = skiq_chan_mode_dual;
        block_size = DUAL_TX_BLOCKSIZE;
        printf("Info: using Tx handle A2\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B1") )
    {
        hdl = skiq_tx_hdl_B1;
        printf("Info: using Tx handle B1\n");
    }
    else if( 0 == strcasecmp(p_hdl, "B2") )
    {
        hdl = skiq_tx_hdl_B2;
        hdl_other = skiq_tx_hdl_B1;
        chan_mode = skiq_chan_mode_dual;
        block_size = DUAL_TX_BLOCKSIZE;
        printf("Info: using Tx handle B2\n");
    }
    else
    {
        fprintf(stderr, "Error: invalid handle specified (%s)\n", p_hdl);
        exit(-1);
    }

    if( 0 == strcasecmp(p_quadcal, "auto") )
    {
        tx_cal_mode = skiq_tx_quadcal_mode_auto;
    }
    else if( 0 == strcasecmp(p_quadcal, "manual") )
    {
        tx_cal_mode = skiq_tx_quadcal_mode_manual;
    }
    else
    {
        fprintf(stderr, "Error: invalid TX quadcal mode specified (%s)\n", p_quadcal);
        exit(-1);
    }

    if ( dwell_time == 0 )
    {
        fprintf(stderr, "Error: --dwell-time must be at least 1\n");
        exit(-1);
    }

    // parse the frequency hopping file (if provided)
    if( p_hop_filename != NULL )
    {
        if( parse_freq_hop_file( freq_list, &num_hop_freqs ) != 0 )
        {
            fprintf(stderr, "Error: unable to parse frequency hopping file (%s)\n", p_hop_filename);
            exit(-1);
        }
    }
    else if( hop_on_timestamp == true )
    {
        fprintf(stderr, "Error: must specify hopping frequencies if hopping on timestamp\n");
        exit(-1);
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full,
                &card, 1);
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
        return (-1);
    }

    if (rfic_pin_enable)
    {
        status = skiq_write_tx_rfic_pin_ctrl_mode(card, hdl, skiq_rfic_pin_control_fpga_gpio );
        if (0 != status)
        {
            printf("Error: failed to set rfic pin control mode on"
                " card %" PRIu8 "  with status %"
                PRIi32 "\n", card, status);
            skiq_exit();
            return (-1);
        }
    }

    /* write the channel mode (dual if A2/B2 is being used) */
    status = skiq_write_chan_mode(card, chan_mode);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: failed to set Rx channel mode to %u (result code %" PRIi32 ")\n", chan_mode, status);
        goto cleanup;
    }

    status = skiq_write_tx_block_size( card, hdl, block_size );
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure block size (result code %"
               PRIi32 ")\n", status);
        goto cleanup;
    }

    // configure the TX quadcal mode
    status = skiq_write_tx_quadcal_mode( card, hdl, tx_cal_mode );
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to configure TX quadcal mode (result code %" PRIi32 ")\n", status);
        goto cleanup;
    }

    /* set the transmit quadcal mode on the other associated handle when using dual channel mode
     * (i.e. other handle is valid ) */
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_write_tx_quadcal_mode( card, hdl_other, tx_cal_mode );
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure TX quadcal mode (result code %" PRIi32 ")\n", status);
            goto cleanup;
        }
    }

    // bw / sample rate
    if( (sample_rate != UINT32_MAX) ||
        (bandwidth != UINT32_MAX) )
    {
        if( sample_rate == UINT32_MAX )
        {
            sample_rate = bandwidth;
        }
        if( bandwidth == UINT32_MAX )
        {
            bandwidth = (uint32_t)(0.8 * sample_rate);
        }
        status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl, sample_rate,
                                                         bandwidth);
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure sample rate (%u) and bandwidth (%u), using current configuration (status=%d)\n",
                   sample_rate, bandwidth, status);
        }
    }

    status=skiq_read_tx_sample_rate_and_bandwidth( card,
                                                   hdl,
                                                   &sample_rate,
                                                   &actual_rate,
                                                   &bandwidth,
                                                   &actual_bandwidth );
    if( status == 0 )
    {
        printf("Using current rate of %f (requested %u Hz) / bandwidth %u Hz (requested %u)\n",
               actual_rate, sample_rate, actual_bandwidth, bandwidth);
    }
    else
    {
        fprintf(stderr, "Error: unable to read current sample rate and bandwidth (status=%d)\n", status);
        goto cleanup;
    }

    if( num_hop_freqs == 0 )
    {
        status = skiq_write_tx_LO_freq(card, hdl, lo_freq );
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure LO frequency (result code %" PRIi32
                   ")\n", status);
            goto cleanup;
        }
    }
    else
    {
        // if hop_on_timestamp set, update tune mode
        if( hop_on_timestamp == true )
        {
            tune_mode = skiq_freq_tune_mode_hop_on_timestamp;
        }

        // configure to use frequency hopping
        if( (status=skiq_write_tx_freq_tune_mode( card, hdl, tune_mode )) == 0 )
        {
            printf("Info: successfully configured tune mode\n");
        }
        else
        {
            fprintf(stderr, "Error: failed to set tune mode status=%d\n", status);
            goto cleanup;
        }
        // configure the hopping list
        if( (status=skiq_write_tx_freq_hop_list( card, hdl, num_hop_freqs, freq_list, 0 )) == 0 )
        {
            printf("successfully set hop list\n");
        }
        else
        {
            printf("failed to set hop list\n");
            goto cleanup;
        }
    }

    // set the Tx attenuation level
    if( tx_atten != UINT16_MAX )
    {
        status = skiq_write_tx_attenuation(card, hdl, tx_atten);
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure attenuation (result code %" PRIi32
                   ")\n", status);
            goto cleanup;
        }

        /* set the transmit attenuation on the other associated handle when using dual channel
         * mode (i.e. other handle is valid) */
        if ( hdl_other != skiq_tx_hdl_end )
        {
            status = skiq_write_tx_attenuation(card, hdl_other, tx_atten);
            if( status != 0 )
            {
                fprintf(stderr, "Error: unable to configure attenuation on other hdl (result code %" PRIi32
                       ")\n", status);
                goto cleanup;
            }
        }
    }

    // set the test frequency offset (if requested)
    if( test_freq_offset != INT32_MAX )
    {
        status = skiq_write_tx_tone_freq_offset( card,
                                                 hdl,
                                                 test_freq_offset );
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure TX test tone offset to %d Hz, status %d\n",
                   test_freq_offset, status);
            goto cleanup;
        }

        /* set the transmit tone frequency on the other associated handle when using dual channel
        * mode (i.e. other handle is valid) */
        if ( hdl_other != skiq_tx_hdl_end )
        {
            status = skiq_write_tx_tone_freq_offset( card,
                                                     hdl_other,
                                                     test_freq_offset );
            if( status != 0 )
            {
                fprintf(stderr, "Error: unable to configure TX test tone offset to %d Hz on other hdl, "
                       "status %d\n", test_freq_offset, status);
                goto cleanup;
            }
        }
    }

    // enable the tone
    status = skiq_enable_tx_tone(card, hdl);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to enable tone (result code %" PRIi32 ")\n",
                status);
        goto cleanup;
    }

    // enable the tone on the other associated handle when using dual channel mode (i.e. other
    // handle is valid)
    if ( hdl_other != skiq_tx_hdl_end )
    {
        status = skiq_enable_tx_tone(card, hdl_other);
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to enable tone on other hdl (result code %" PRIi32 ")\n",
                   status);
            goto cleanup;
        }
    }

    // start streaming
    status = skiq_start_tx_streaming(card, hdl);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to start streaming (result code %" PRIi32 ")\n",
                status);
        goto cleanup;
    }

    // reset the timestamps and wait for the reset to complete
    skiq_read_curr_tx_timestamp( card, skiq_tx_hdl_A1, &base_ts );
    printf("Resetting timestamps (base=%" PRIu64 ")\n", base_ts);
    if( reset_on_1pps == true )
    {
        printf("Resetting on 1PPS\n");
        skiq_write_timestamp_reset_on_1pps( card, 0 );
    }
    else
    {
        printf("Resetting async\n");
        skiq_reset_timestamps( card );
    }
    skiq_read_curr_tx_timestamp( card, skiq_tx_hdl_A1, &curr_ts );

    printf("Waiting for reset complete (base=%" PRIu64 "), (curr=%" PRIu64 ")\n", base_ts, curr_ts);
    while( (base_ts < curr_ts) && (running==true) )
    {
        // just sleep 100 uS and read the timestamp again...we should be close based on the initial sleep calculation
        usleep(100);
        skiq_read_curr_tx_timestamp(card, hdl, &curr_ts);
    }
    printf("Resetting timestamp complete (current=%" PRIu64 ")\n", curr_ts);

    // read the test tone frequency once here if we're not hopping frequencies.
    if (num_hop_freqs == 0)
    {
        status = skiq_read_tx_tone_freq( card, hdl, &read_test_freq );
        if( status == 0 )
        {
            printf("Info: TX test tone located at freq %" PRIu64 " Hz \n", read_test_freq);
        }
        else
        {
            printf("Warning: failed to read TX tone frequency (result code %"
                    PRIi32 ")\n", status);
        }
    }

    if( (num_secs != 0) && (running==true) )
    {
        printf("Info: sleeping for %" PRIu32 " seconds...\n", num_secs);
        sleep(num_secs);
    }
    else
    {
        uint16_t hop_index=1;

        printf("Info: sleeping indefinitely...\n");
        

        /* sleep forever, waking every second or hop dwell time */
        while (running==true)
        {
            if( num_hop_freqs > 0 )
            {
                if( (status=skiq_write_next_tx_freq_hop( card, hdl, hop_index )) == 0 )
                {
                    hop_index++;
                    if( hop_index >= num_hop_freqs )
                    {
                        hop_index = 0;
                    }
                }
                else
                {
                    printf("failed to write hop with status %d\n (freq %" PRIu64 ", hop index %u)",
                           status, freq_list[hop_index], hop_index);
                }
                if( (status=skiq_perform_tx_freq_hop( card, hdl, hop_ts )) != 0 )
                {
                    fprintf(stderr, "Error: failed to hop with status %d\n", status);
                }
                else
                {
                    uint64_t curr_hop_freq;
                    uint16_t curr_hop_index;
                    // hop completed successfully, display currently configured frequency
                    if( skiq_read_curr_tx_freq_hop( card,
                                                    hdl,
                                                    &curr_hop_index,
                                                    &curr_hop_freq ) == 0 )
                    {
                        printf("Info: hopped to LO freq %" PRIu64 "Hz at index %" PRIu16 "\n",
                               curr_hop_freq, curr_hop_index);
                    }
                    else
                    {
                        printf("Error: unable to read current hop information\n");
                    }
                }
                status = skiq_read_tx_tone_freq( card, hdl, &read_test_freq );
                if( status == 0 )
                {
                    printf("Info: TX test tone located at freq %" PRIu64 " Hz \n", read_test_freq);
                }
                else
                {
                    printf("Warning: failed to read TX tone frequency for handle %s on card %" PRIu8 " (result code %"
                            PRIi32 ")\n", tx_hdl_cstr(hdl), card, status);
                }
                if( tune_mode == skiq_freq_tune_mode_hop_on_timestamp )
                {
                    // if we're hopping on timestamp, wait until the previous hop as occurred before scheduling the next one
                    hop_ts += hop_timestamp_offset;
                    wait_until_after_rf_ts( card, hdl, hop_ts );
                }
                else
                {
                    if( running == true )
                    {
                        sleep(dwell_time);
                    }
                }
            }
            else
            {
                if( running == true )
                {
                    sleep(1);
                }
            }
        }
    }

cleanup:
    printf("Info: shutting down...\n");

    // stop streaming
    skiq_stop_tx_streaming(card, hdl);
    // disable the tone
    skiq_disable_tx_tone(card, hdl);

    if ( hdl_other != skiq_tx_hdl_end )
    {
        // disable the other handle's tone, if hdl_other is well defined
        skiq_disable_tx_tone(card, hdl_other);
    }

    skiq_exit();

    return status;
}
