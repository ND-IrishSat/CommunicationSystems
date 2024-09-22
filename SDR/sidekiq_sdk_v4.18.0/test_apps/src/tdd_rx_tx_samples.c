/*! \file tdd_rx_tx_samples.c
 * \brief This file contains a basic application for configuring
 * a Sidekiq to do the following:
 *   -Configure the Rx interface
 *   -Configure the Tx interface
 *   -Start the Rx and Tx interfaces
 *   -Loop N times:
 *      -Flush receive
 *      -Switch to receive
 *      -Receive I/Q samples for a fixed period of time
 *      -Switch to transmit
 *      -Transmit I/Q samples from a file for a fixed period of time
 *
 * Many of the RF configuration parameters are defaulted to sane
 * values for this application to minimize the command line args
 * required to run both rx and tx interfaces.  These can always be
 * tweaked if needed.
 *
 * <pre>
 * Copyright 2014 - 2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <arg_parser.h>
#include <sidekiq_api.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

#ifndef DEFAULT_RX_FREQUENCY
#   define DEFAULT_RX_FREQUENCY 850000000
#endif

#ifndef DEFAULT_SAMPLE_RATE
#   define DEFAULT_SAMPLE_RATE 10000000
#endif

#ifndef DEFAULT_NUM_SAMPLES
#   define DEFAULT_NUM_SAMPLES  100000
#endif

#ifndef DEFAULT_TX_FREQUENCY
#   define DEFAULT_TX_FREQUENCY 850000000
#endif

#ifndef DEFAULT_LOOPS
#   define DEFAULT_LOOPS  10
#endif

#ifndef DEFAULT_RF_PORT_CONFIG
#   define DEFAULT_RF_PORT_CONFIG   "fixed"
#endif

#ifndef DEFAULT_TX_ATTEN
#   define DEFAULT_TX_ATTEN 50
#endif

#ifndef DEFAULT_BLOCK_SIZE
#   define DEFAULT_BLOCK_SIZE 16380
#endif

#define NUM_RX_PAYLOAD_WORDS_IN_BLOCK (SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS-SKIQ_RX_HEADER_SIZE_IN_WORDS)

#define NUM_NANOSEC_IN_SEC (1000000000)

/* local functions */
static int32_t process_cmd_line_args(int argc, char *argv[]);
static int32_t configure_sample_rate(void);
static int32_t prepare_rx(void);
static int32_t prepare_tx(void);
static void recv_samples(void);
static void send_samples(void);
static void flush_receive(void);
static void switch_to_rx(void);
static void switch_to_tx(void);
static int32_t init_tx_buffer(void);

static char* app_name;

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- receive IQ data then transmit the received data";
static const char* p_help_long = "\
Tune the RF receiver to the specified Rx LO frequency, set the specified sample rate,\n\
and receive the requested # of samples to the specified output file.  Once completed, \n\
turn on the RF transmitter, set the specified Tx LO frequency, and transmit the I/Q samples\n\
stored in the specified Tx sample file.  Repeat this Rx->Tx loop the requested # of times.\n\
Note: the RX and TX sample rates must be the same.\n\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --rx-freq=" xstr(DEFAULT_RX_FREQUENCY) "\n\
  --rate=" xstr(DEFAULT_SAMPLE_RATE) "\n\
  --num-rx-samples=" xstr(DEFAULT_NUM_SAMPLES) "\n\
  --tx-freq=" xstr(DEFAULT_TX_FREQUENCY) "\n\
  --attenuation=" xstr(DEFAULT_TX_ATTEN) "\n\
  --num-loops=" xstr(DEFAULT_LOOPS) "\n\
  --rf-port-config=" xstr(DEFAULT_RF_PORT_CONFIG) "\n\
  --block-size=" xstr(DEFAULT_BLOCK_SIZE) "\n\
";

// parameters read from command line
static uint8_t card=UINT8_MAX;
static char* p_serial=NULL;
static char* output_filepath=NULL;
static uint64_t rx_lo_freq=DEFAULT_RX_FREQUENCY;
static uint32_t sample_rate=DEFAULT_SAMPLE_RATE;
static uint32_t num_samples_to_rx=DEFAULT_NUM_SAMPLES;
static char* input_filepath=NULL;
static uint64_t tx_lo_freq=DEFAULT_TX_FREQUENCY;
static uint16_t tx_atten=DEFAULT_TX_ATTEN;
static uint32_t num_loops=DEFAULT_LOOPS;
static char* rf_port_config=DEFAULT_RF_PORT_CONFIG;
static skiq_tx_block_t **p_tx_blocks = NULL; /* reference to an array of transmit block references */
static uint32_t num_blocks = 0;
static uint32_t block_size_in_words = DEFAULT_BLOCK_SIZE;
skiq_tx_flow_mode_t tx_mode = skiq_tx_with_timestamps_data_flow_mode;
static uint32_t rx_gain = UINT32_MAX;
static bool rx_gain_is_present = false;
skiq_rx_gain_t gain_mode = skiq_rx_gain_auto;

skiq_rx_hdl_t rx_hdl = skiq_rx_hdl_A1;
skiq_tx_hdl_t tx_hdl = skiq_tx_hdl_A1;
static bool card_present=false;

static FILE* input_fp = NULL;
static FILE* output_fp = NULL;

static bool running=true;

static skiq_rf_port_config_t rf_port=skiq_rf_port_config_fixed;

uint32_t num_complete_rx_blocks = 0;  // # full blocks to receive
uint32_t last_block_num_bytes = 0;    // extra # bytes to receive on last block

uint32_t* p_rx_iq = NULL;  // pointer to the Rx data
skiq_tx_block_t* p_tx_block = NULL;  // pointer to the transmit block

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
    APP_ARG_OPT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_REQ("rx-output",
                0,
                "Absolute path to RX samples output file",
                NULL,
                &output_filepath,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rx-freq",
                0,
                "RX LO Frequency in Hertz",
                "Hz",
                &rx_lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("num-rx-samples",
                0,
                "Number of RX samples to receive",
                NULL,
                &num_samples_to_rx,
                UINT32_VAR_TYPE),
    APP_ARG_REQ("tx-input",
                0,
                "Absolute path to TX samples input file",
                NULL,
                &input_filepath,
                STRING_VAR_TYPE),
    APP_ARG_OPT("tx-freq",
                0,
                "TX LO Frequency in Hertz",
                "Hz",
                &tx_lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("attenuation",
                'a',
                "Output attenuation in quarter dB steps",
                "dB",
                &tx_atten,
                UINT16_VAR_TYPE),
    APP_ARG_OPT("num-loops",
                0,
                "Number of RX->TX iterations",
                NULL,
                &num_loops,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("rf-port-config",
                0,
                "RF port configuration for either \"fixed\", \"trx\".  " \
                "\n\t\t\tWhen using TRX on capable radio, both receive and transmit occurs on same RF connector.",
                NULL,
                &rf_port_config,
                STRING_VAR_TYPE),
    APP_ARG_OPT("block-size",
                0,
                "Number of samples to transmit per block",
                "N",
                &block_size_in_words,
                UINT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("gain",
                'g',
                "Manually configure the gain by index rather than using automatic",
                "index",
                &rx_gain,
                UINT32_VAR_TYPE,
                &rx_gain_is_present),
    APP_ARG_TERMINATOR
};

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
/** Utility functions for converting skiq rx/tx handles to a string for
    printing */
const char *
_rx_hdl_cstr( skiq_rx_hdl_t hdl )
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

const char *
_tx_hdl_cstr( skiq_tx_hdl_t hdl )
{
    const char* p_hdl_str =
        (hdl == skiq_tx_hdl_A1) ? "A1" :
        (hdl == skiq_tx_hdl_A2) ? "A2" :
        (hdl == skiq_tx_hdl_B1) ? "B1" :
        (hdl == skiq_tx_hdl_B2) ? "B2" :
        "unknown";

    return p_hdl_str;
}

/*****************************************************************************/
/** This is the a function that blocks until a specific RF timestamp is reached.

    @param card_: Sidekiq card of interest
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
                status = usleep(1);
                if (0 != status)
                {
                    fprintf(stderr, "Warning: failed to sleep (result = %" PRIi32 "); attempting"
                        " to continue...\n", status);
                }
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
/** This is the main function for executing the tdd_rx_tx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    uint32_t i=0;
    int32_t status = 0;
    bool b_fixed_avail = false;
    bool b_tdd_avail = false;
    bool skiq_initialized = false;
    pid_t owner = 0;

    app_name = argv[0];

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize everything based on the arguments provided */
    status = process_cmd_line_args( argc, argv );
    if(status != 0)
    {
        goto finished;
    }

    // initialize the transmit buffer
    status = init_tx_buffer();
    if (0 != status)
    {
        goto finished;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize libsidekiq for the card specified */
    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, &card, 1);
    if(status != 0)
    {
        if((EBUSY == status) &&
           (0 != skiq_is_card_avail(card, &owner)))
        {
            fprintf(stderr, "Error: card %" PRIu8 " is already in use (by process ID"
                    " %u); cannot initialize card.\n", card,
                    (unsigned int) owner);
        }
        else if(-EINVAL == status)
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq; was a valid card"
                    " specified? (result code %" PRIi32 ")\n", status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with status %" PRIi32
                    "\n", status);
        }
        goto finished;
    }
    skiq_initialized = true;

    /* get the available RF port configs and see if it matches the desired config */
    status = skiq_read_rf_port_config_avail(card, &b_fixed_avail, &b_tdd_avail);
    if(status == 0)
    {
        if((rf_port == skiq_rf_port_config_fixed) && (b_fixed_avail==false))
        {
            fprintf(stderr, "Error: Fixed RF port requested but not available\n");
            status = -EAGAIN;
            goto finished;
        }
        if((rf_port == skiq_rf_port_config_trx) && (b_tdd_avail==false))
        {
            fprintf(stderr, "Error: TDD RF port requested but not available\n");
            status = -EAGAIN;
            goto finished;
        }
    }
    else
    {
        fprintf(stderr, "Error: unable to read available RF port configuration\n");
        goto finished;
    }

    status = skiq_write_rf_port_config(card, rf_port);
    if(status != 0)
    {
        fprintf(stderr, "Error: unable to write RF port config with status %d\n", status);
        goto finished;
    }

    /* set the gain_mode based on rx_gain from the arguments */
    if (rx_gain_is_present)
    {
        gain_mode = skiq_rx_gain_manual;
    }
    else
    {
        gain_mode = skiq_rx_gain_auto;
    }
    /* prepare the interfaces */
    status = configure_sample_rate();
    if(status != 0)
    {
        goto finished;
    }
    status = prepare_rx();
    if(status != 0)
    {
        goto finished;
    }
    status = prepare_tx();
    if(status != 0)
    {
        goto finished;
    }

    status = skiq_reset_timestamps(card);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to reset timestamps. (result code %"
                PRIi32 ")\n", status);
        goto finished;
    }

    status = skiq_start_rx_streaming(card, rx_hdl);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to start_rx_streaming (result code %"
                PRIi32 ")\n", status);
        goto finished;
    }
    status = skiq_start_tx_streaming(card, tx_hdl);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to start_tx_streaming (result code %"
                PRIi32 ")\n", status);

        goto finished;
    }

    /* receive / send data for specified # times */
    for( i=0; (i<num_loops) && (running==true); i++ )
    {
        switch_to_rx();
        flush_receive(); // flush any samples that came already
        recv_samples();
        switch_to_tx();
        send_samples();
    }

    if (running == true)
    {
        printf("Info: Success\n");
    }

    /* close files and cleanup */

finished:

    if (skiq_initialized)
    {
        // stop streaming
        (void)skiq_stop_rx_streaming(card, rx_hdl);
        (void)skiq_stop_tx_streaming(card, tx_hdl);
        skiq_exit();
        skiq_initialized = false;
    }

    if (NULL != output_fp)
    {
        fclose(output_fp);
        output_fp = NULL;
    }

    if (NULL != input_fp)
    {
        fclose(input_fp);
        input_fp = NULL;
    }

    if (NULL != p_rx_iq)
    {
        free(p_rx_iq);
        p_rx_iq = NULL;
    }

    if (NULL != p_tx_block)
    {
        free(p_tx_block);
        p_tx_block = NULL;
    }

    return (int) status;
}

/*****************************************************************************/
/** The configure_sample_rate function sets the requested sample
 *  rate for both Rx/Tx interfaces.

    @param void
    @return int32_t  status where 0=success, anything else is an error
*/
static int32_t configure_sample_rate(void)
{
    int32_t status = 0;

    if( (status=skiq_write_rx_sample_rate_and_bandwidth(card,
                                                        rx_hdl,
                                                        sample_rate,
                                                        sample_rate)) != 0 )
    {
        printf("Error: unable to configure Rx sample rate and bandwidth\n");
        return status;
    }

    if( (status=skiq_write_tx_sample_rate_and_bandwidth(card,
                                                        tx_hdl,
                                                        sample_rate,
                                                        sample_rate)) != 0 )
    {
        printf("Error: unable to configure Tx sample rate and bandwidth\n");
        return status;
    }

    return status;

}

/*****************************************************************************/
/** The prepare_rx function does all configuration of the Rx interface,
    without actually starting the interface.

    @param void
    @return int32_t  status where 0=success, anything else is an error
*/
static int32_t prepare_rx(void)
{
    uint32_t num_bytes_to_alloc = 0;
    int32_t status=0;

    if( (status=skiq_write_rx_LO_freq(card, rx_hdl, rx_lo_freq)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Rx LO frequency for card %u \
                hdl %s (result code %" PRIi32 ")\n", card, _rx_hdl_cstr(rx_hdl), status);
        return (status);
    }
    if( (status=skiq_write_rx_gain_mode(card, rx_hdl, gain_mode)) != 0 )
    {
        fprintf(stderr, "Warning: unable to configure Rx gain mode for card %u \
                hdl %s (result code %"PRIi32 ")\n", card, _rx_hdl_cstr(rx_hdl),status);
    }
    if (gain_mode == skiq_rx_gain_manual)
    {
        if( (status=skiq_write_rx_gain(card, rx_hdl, rx_gain)) != 0 )
        {
            fprintf(stderr, "Warning: unable to configure Rx gain for card %u \
                    hdl %s (result code %"PRIi32 ")\n", card, _rx_hdl_cstr(rx_hdl), status);
        }
    }

    num_complete_rx_blocks = num_samples_to_rx / NUM_RX_PAYLOAD_WORDS_IN_BLOCK;
    last_block_num_bytes = ((num_samples_to_rx % NUM_RX_PAYLOAD_WORDS_IN_BLOCK)*4);

    num_bytes_to_alloc =
        (num_complete_rx_blocks*NUM_RX_PAYLOAD_WORDS_IN_BLOCK*sizeof(uint32_t)) + last_block_num_bytes;
    p_rx_iq = malloc( num_bytes_to_alloc );
    if( p_rx_iq == NULL )
    {
        fprintf(stderr, "Error: failed to allocate %" PRIu32 " bytes for Rx"
                " IQ buffer for card %u hdl %s\n", num_bytes_to_alloc, card, _rx_hdl_cstr(rx_hdl));
        status = -ENOMEM;
    }
    return (status);
}

/*****************************************************************************/
/** The prepare_tx function does all configuration of the Tx interface,
    without actually starting the interface.

    @param void
    @return int32_t  status where 0=success, anything else is an error
*/
static int32_t prepare_tx(void)
{
    int32_t status=0;

    if( (status=skiq_write_tx_LO_freq(card, tx_hdl, tx_lo_freq)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx LO frequency for card %u \
                hdl %s (result code %"PRIi32 ")\n", card, _tx_hdl_cstr(tx_hdl), status);
        return (status);
    }
    if( (status=skiq_write_tx_attenuation(card, tx_hdl, tx_atten)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx attenuation for card %u \
                hdl %s (result code %"PRIi32 ")\n", card, _tx_hdl_cstr(tx_hdl), status);
        return (status);
    }
    if( (status=skiq_write_tx_data_flow_mode(card,
                                             tx_hdl,
                                             tx_mode)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx data flow mode for card %u \
                hdl %s (result code %"PRIi32 ")\n", card, _tx_hdl_cstr(tx_hdl), status);
        return (status);
    }
    if( (status=skiq_write_tx_block_size(card,
                                         tx_hdl,
                                         block_size_in_words)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx block size of %u for card %u \
                hdl %s  (result code %"PRIi32 ")",block_size_in_words, card,
                _tx_hdl_cstr(tx_hdl), status);
        return (status);
    }

    /* allocate a transmit block by number of words */
    p_tx_block = skiq_tx_block_allocate( block_size_in_words );
    if( p_tx_block == NULL )
    {
        fprintf(stderr, "Error: unable to allocate memory for transmit block\n");
        status=-ENOMEM;
    }

    return (status);
}

/*****************************************************************************/
/** The recv_samples function is responsible for receiving the requested
    # of I/Q samples and storing them in a file.

    @param none
    @return void
*/
void recv_samples(void)
{
    skiq_rx_hdl_t hdl;
    skiq_rx_block_t *p_rx_block;
    uint32_t len;
    uint32_t tot_blocks_acquired=0;
    uint64_t curr_timestamp=0;
    skiq_rx_status_t status;
    bool done=false;
    bool first_timestamp = true;
    uint32_t* p_next_rx_write;

    uint64_t next_timestamp = 0;

    p_next_rx_write = p_rx_iq;

    printf("Info: receiving samples\n");

    while( (done==false) && (running==true) )
    {
        status = skiq_receive(card, &hdl, &p_rx_block, &len);
        if( skiq_rx_status_success == status )
        {
            if( hdl != rx_hdl )
            {
                fprintf(stderr, "Error: received unexpected data from hdl %u %s\n", hdl, _rx_hdl_cstr(hdl));
                running = false;
                continue;
            }
            curr_timestamp = p_rx_block->rf_timestamp; // peek at timestamp
            if( first_timestamp == true )
            {
                first_timestamp = false;
                next_timestamp = curr_timestamp;
            }
            else
            {
                if( curr_timestamp != next_timestamp )
                {
                    fprintf(stderr, "Error: timestamp error...expected 0x%016" PRIx64 " but got 0x%016" PRIx64 "\n",
                           next_timestamp, curr_timestamp);
                }
            }
            // copy either a complete block of data or a partial block at the end
            if( tot_blocks_acquired < num_complete_rx_blocks )
            {
                memcpy( p_next_rx_write,
                        (void *)p_rx_block->data,
                        NUM_RX_PAYLOAD_WORDS_IN_BLOCK*4 );
                p_next_rx_write += NUM_RX_PAYLOAD_WORDS_IN_BLOCK;
                tot_blocks_acquired++;
            }
            else
            {
                // we're at the end, just copy a partial block
                memcpy( p_next_rx_write,
                        (void *)p_rx_block->data,
                        last_block_num_bytes );
                done = true;
            }
            next_timestamp += (len - SKIQ_RX_HEADER_SIZE_IN_BYTES)/4;
        }
        else if (status !=  skiq_rx_status_no_data)
        {
            printf("Error: skiq_receive returned: %d\n", status);
        }
    }

    // write the file
    printf("Info: saving samples to the file\n");
    p_next_rx_write = p_rx_iq;
    for( len=0; (len<num_complete_rx_blocks) && (running==true); len++ )
    {
        fwrite(p_next_rx_write, NUM_RX_PAYLOAD_WORDS_IN_BLOCK*4, 1, output_fp);
        p_next_rx_write += NUM_RX_PAYLOAD_WORDS_IN_BLOCK;
    }
    if( last_block_num_bytes > 0 )
    {
        fwrite(p_next_rx_write, last_block_num_bytes, 1, output_fp);
    }
}

/*****************************************************************************/
/** The send_samples function is responsible for sending the samples from
    the user specified input file.

    @param none
    @return: void
*/
static void send_samples(void)
{
    int32_t status = 0;
    uint32_t errors = 0;
    uint32_t curr_block=0;
    uint64_t next_tx_timestamp = 0;
    uint32_t timestamp_increment = block_size_in_words;

    printf("Info: sending samples: (num_blocks=%" PRIu32 ")\n", num_blocks);
    status = skiq_read_curr_tx_timestamp(card, tx_hdl, &next_tx_timestamp);
    if (status != 0)
    {
        fprintf(stderr, "Error: failed to read tx timestamp for card %u \
                hdl %s(result code %" PRIi32 ")\n", card, _tx_hdl_cstr(tx_hdl),status);
    }

    // transmit a block at a time
    while( (curr_block < num_blocks) && (running==true) )
    {
        next_tx_timestamp += timestamp_increment;

        skiq_tx_set_block_timestamp( p_tx_blocks[curr_block], next_tx_timestamp);

        // transmit the data
        if (running)
        {
            status = skiq_transmit(card, tx_hdl, p_tx_blocks[curr_block], NULL );
            if (0 != status)
            {
                fprintf(stderr, "Error: failed to transmit data (result code %"
                        PRIi32 ")\n", status);
                running = false;
            }

            curr_block++;
        }
    }

    if (status == 0)
    {
        status = wait_until_after_rf_ts( card, tx_hdl, next_tx_timestamp );

        /* The scheduled transmissions should be completed, check to ensure
           they were transmitted on the desired timestamp */

        if(status == 0)
        {
            status = skiq_read_tx_num_late_timestamps(card, tx_hdl, &errors);
            if (status != 0)
            {
                fprintf(stderr, "Error: failed to read the number of Tx late timestamps (result code %"
                        PRIi32 ")\n", status);
                running = false;
            }
            printf("Info: total number of Tx late timestamps detected is %" PRIu32 "\n",
                        errors);
        }
    }
}

/*****************************************************************************/
/** The flush_receive function just receives and discards samples up until
    past the current timestamp.

    @param none
    @return void
*/
void flush_receive(void)
{
    bool done=false;
    uint32_t flush_count=0;
    uint64_t current_ts=0;
    uint64_t rx_ts=0;
    int32_t status = 0;
    skiq_rx_hdl_t hdl;
    skiq_rx_block_t *p_rx_block;
    uint32_t len;

    // read the current timestamp to determine how much we need to receive until flush is done
    status = skiq_read_curr_rx_timestamp(card,
                                rx_hdl,
                                &current_ts);
    if (status != 0)
    {
        fprintf(stderr, "Error: unable to read Rx timestamp for card %u \
                hdl %s (result code %d)\n", card, _rx_hdl_cstr(rx_hdl), status);
    }

    while( (done==false) && (status == 0) )
    {
        status = skiq_receive(card, &hdl, &p_rx_block, &len);
        if( skiq_rx_status_success == status )
        {
            flush_count++;
            if( hdl != rx_hdl )
            {
                fprintf(stderr, "Error: received unexpected data from hdl %u\n", hdl);
                continue;
            }
            rx_ts = p_rx_block->rf_timestamp; // peek at timestamp
            // if the received timestamp is past the saved timestamp, we're done flushing
            if( rx_ts > current_ts )
            {
                done = true;
            }
        }
        else if (status !=  skiq_rx_status_no_data)
        {
            fprintf(stderr, "Error: skiq_receive returned: %d\n", status);
        }
    }
    printf("flush complete, # packets flushed %u\n", flush_count);
}

/*****************************************************************************/
/** The switch_to_rx function switches to receive

    @param void
    @return void
*/
void switch_to_rx(void)
{
    // if TDD, switch the RF port operation to Rx
    if( rf_port == skiq_rf_port_config_trx )
    {
        if( skiq_write_rf_port_operation(card, false /* transmit */) != 0 )
        {
            fprintf(stderr, "Error: Unable to switch to Rx!\n");
        }
    }
}

/*****************************************************************************/
/** The switch_to_tx function switches to transmit

    @param void
    @return void
*/
void switch_to_tx(void)
{
    // if TDD, switch the RF port operation to Tx
    if( rf_port == skiq_rf_port_config_trx )
    {
        if( skiq_write_rf_port_operation(card, true /* transmit */) != 0 )
        {
            fprintf(stderr, "Error: Unable to switch to Tx!\n");
        }
    }
}

/*****************************************************************************/
/** This function extracts all cmd line args

    @param argc: the # of args to be processed
    @param argv: a pointer to the vector of arg strings
    @return: void
*/
static int32_t process_cmd_line_args(int argc, char* argv[])
{
    int32_t status = 0;

    /* extract all args */

    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if(status != 0)
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return status;
    }

    if((card_present) && (NULL != p_serial))
    {
        fprintf(stderr, "Error: must specify EITHER card ID or serial number, not"
                " both\n");
        return -EPERM;
    }
    if(!(card_present))
    {
        card = DEFAULT_CARD_NUMBER;
    }

    /* If specified, attempt to find the card with a matching serial number. */
    if(NULL != p_serial)
    {
        status = skiq_get_card_from_serial_string(p_serial, &card);
        if(0 != status)
        {
            fprintf(stderr, "Error: cannot find card with serial number %s (result"
                    " code %" PRIi32 ")\n", p_serial, status);
            status = -ENODEV;
            goto finished;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        status = -ERANGE;
        goto finished;
    }

    if((strcasecmp("fixed", rf_port_config) == 0) ||
       (strcasecmp("'fixed'", rf_port_config) == 0))
    {
        rf_port = skiq_rf_port_config_fixed;
        printf("Info: requested fixed RF port configuration\n");
    }
    else if((strcasecmp("tdd", rf_port_config) == 0) ||
            (strcasecmp("'tdd'", rf_port_config) == 0) ||
            (strcasecmp("trx", rf_port_config) == 0) ||
            (strcasecmp("'trx'", rf_port_config) == 0) )
    {
        rf_port = skiq_rf_port_config_trx;
        printf("Info: requested TRX (TDD) RF port configuration\n");
    }
    else
    {
        fprintf(stderr, "Error: invalid RF port configuration option, choose either 'fixed' or 'tdd'\n");
        status = -EINVAL;
        goto finished;
    }

    /* ----------------first rx args---------------- */
    output_fp=fopen(output_filepath,"wb");
    if (output_fp == NULL)
    {
        fprintf(stderr, "Error: unable to open output file %s\n", output_filepath);
        status = -errno;
        goto finished;
    }
    printf("Info: opened file %s to hold the received IQ data \n", output_filepath);
    printf("Info: Requested Rx LO freq will be %" PRIu64 " Hz\n", rx_lo_freq);
    printf("Info: Requested sample rate is %" PRIu32 "\n", sample_rate);
    printf("Info: Requested # of I/Q sample pairs to acquire is %" PRIu32 "\n", num_samples_to_rx);

    /* ----------------and now for tx args---------------- */
    input_fp=fopen(input_filepath, "rb");
    if (input_fp == NULL)
    {
        fprintf(stderr, "Error: unable to open input file %s\n", input_filepath);
        status = -errno;
        goto finished;
    }
    printf("Info: opened file %s for reading transmit IQ data\n", input_filepath);
    printf("Info: Requested Tx LO freq will be %" PRIu64 " Hz\n", tx_lo_freq);
    printf("Info: Requested # of loop iterations to be %" PRIu32 "\n", num_loops);
    printf("Info: requested %s RF port configuration\n",
            rf_port==skiq_rf_port_config_fixed ? "fixed" : "TDD");

finished:
    if (0 != status)
    {
        if (NULL != input_fp)
        {
            fclose(input_fp);
            input_fp = NULL;
        }
        if (NULL != output_fp)
        {
            fclose(output_fp);
            output_fp = NULL;
        }
    }

    return status;
}



static int32_t init_tx_buffer(void)
{
    int32_t status = 0;
    uint32_t num_bytes_in_file=0;
    int32_t bytes_read=0;
    uint32_t i = 0;
    uint32_t block_size_in_bytes = block_size_in_words*4;
    int result = 0;

    // determine how large the file is and how many blocks we'll need to send
    errno = 0;
    status = fseek(input_fp, 0, SEEK_END);
    if (status != 0)
    {
        fprintf(stderr, "Error: unable to seek to beginning of input file '%s' \
                (%d: '%s')\n", input_filepath, errno, strerror(errno));

        goto finished;
    }
    num_bytes_in_file = ftell(input_fp);
    errno = 0;
    rewind(input_fp);
    if (errno != 0)
    {
        fprintf(stderr, "Error: unable to rewind file (%d: '%s')\n",
                errno, strerror(errno));
        goto finished;
    }
    num_blocks = (num_bytes_in_file / block_size_in_bytes);

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
        status = -ENOMEM;
        goto finished;
    }

    // read in the contents of the file, one block at a time
    for (i = 0; i < num_blocks; i++)
    {
        if ( !feof(input_fp)  )
        {
            /* allocate a transmit block by number of words */
            p_tx_blocks[i] = skiq_tx_block_allocate( block_size_in_words );

            if ( p_tx_blocks[i] == NULL )
            {
                fprintf(stderr, "Error: unable to allocate for Tx block number %" PRIu32 "\n", i);
                status = -ENOMEM;
                goto finished;
            }

            // read a block of samples
            bytes_read += fread( p_tx_blocks[i]->data, 1,
                                   block_size_in_bytes, input_fp );
            result = ferror(input_fp);
            if ( 0 != result )
            {
                fprintf(stderr,
                    "Error: unable to read from file (result code %d)\n",
                    result);
                status = -EIO;
                goto finished;
            }
        }
    }
    //check to ensure we've read the expected number of bytes.
    if (bytes_read != num_bytes_in_file)
    {
        fprintf(stderr, "Error: failed to read in the entire TX data file '%s' \
                (expected %" PRIi32 " bytes, only read %" PRIu32 " bytes)\n",
                input_filepath, bytes_read, num_bytes_in_file);
        goto finished;
    }

finished:
    if (0 != status)
    {
        if (NULL != p_tx_blocks)
        {   /* i contains the number of blocks successfully allocated */
            uint32_t j = 0;
            for (j = 0; j < i; j++)
            {
                skiq_tx_block_free(p_tx_blocks[j]);
            }

            free(p_tx_blocks);
            p_tx_blocks = NULL;
        }

    }

    if (NULL != input_fp)
    {
        fclose(input_fp);
        input_fp = NULL;
    }

    return status;
}
