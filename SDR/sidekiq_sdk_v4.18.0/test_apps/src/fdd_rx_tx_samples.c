/*! \file fdd_rx_tx_samples.c
 * \brief This file contains a basic application for configuring
 * a Sidekiq to do the following:
 *   -Configure the Rx interface
 *   -Configure the Tx interface
 *   -Start the Rx interface in the main thread, storing samples to a file
 *   -Start a separate Tx thread and loop:
 *       -sleep for a short duration (~1 second)
 *       -transmit samples from a file
 *
 * Many of the RF configuration parameters are defaulted to sane
 * values for this application to minimize the command line args
 * required to run both rx and tx interfaces.  These can always be
 * tweaked if needed.
 *
 * <pre>
 * Copyright 2014-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

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
#   define DEFAULT_TX_FREQUENCY 950000000
#endif

#ifndef DEFAULT_TX_LOOPS
#   define DEFAULT_TX_LOOPS  5
#endif

#ifndef DEFAULT_INIT_TIMESTAMP
#   define DEFAULT_INIT_TIMESTAMP  100
#endif

#ifndef DEFAULT_BLOCK_SIZE
#   define DEFAULT_BLOCK_SIZE 16380
#endif

#ifndef DEFAULT_RX_HDL
#   define DEFAULT_RX_HDL "A1"
#endif
#ifndef DEFAULT_TX_HDL
#   define DEFAULT_TX_HDL "A1"
#endif

#define NUM_RX_PAYLOAD_WORDS_IN_BLOCK (SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS-SKIQ_RX_HEADER_SIZE_IN_WORDS)

/* local functions */
static int32_t process_cmd_line_args(int argc, char *argv[]);
static int32_t configure_sample_rate(void);
static int32_t prepare_rx(void);
static int32_t prepare_tx(void);
static void recv_samples(void);
static void* send_samples(void*);
static int32_t init_tx_buffer(void);

static char* app_name;

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- transmit and recieve IQ data simultaneously";
static const char* p_help_long = "\
Tune the RF receiver to the specified Rx LO frequency, set the specified Rx sample rate,\n\
tune the RF transmitter to the specificed Tx LO frequency, and begin transmitting I/Q\n\
samples from the Tx file while simultaneously receiving I/Q samples and storing them\n\
to the output file.  The transmitter loops through a sequence of sleeping for a short\n\
duration, followed by transmitting the I/Q data from the file.  The # of loops completed\n\
is specified by the # of transmit iterations requested on the cmd line.\n\n\
Additional Tx/Rx parameters (gain settings, filter bandwidths, etc)\n\
can all be set within the application itself...they default to sane values for now.\n\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --rx-freq=" xstr(DEFAULT_RX_FREQUENCY) "\n\
  --rate=" xstr(DEFAULT_SAMPLE_RATE) "\n\
  --num-rx-samples=" xstr(DEFAULT_NUM_SAMPLES) "\n\
  --tx-freq=" xstr(DEFAULT_TX_FREQUENCY) "\n\
  --num-tx-loops=" xstr(DEFAULT_TX_LOOPS) "\n\
  --block-size=" xstr(DEFAULT_BLOCK_SIZE) "\n\
  --rx-hdl=" DEFAULT_RX_HDL "\n\
  --tx-hdl=" DEFAULT_TX_HDL "\n\
";

// parameters read from command line
static bool running=true;
static uint8_t card=UINT8_MAX;
static char* p_serial=NULL;
static char* output_filepath=NULL;
static uint64_t rx_lo_freq=DEFAULT_RX_FREQUENCY;
static uint32_t sample_rate=DEFAULT_SAMPLE_RATE;
static uint32_t num_samples_to_rx=DEFAULT_NUM_SAMPLES;
static char* input_filepath=NULL;
static uint64_t tx_lo_freq=DEFAULT_TX_FREQUENCY;
static uint32_t num_tx_loops=DEFAULT_TX_LOOPS;
static uint8_t rx_gain = UINT8_MAX;
static bool rx_gain_is_present = false;
static bool timestamp_is_present = false;
static skiq_tx_block_t **p_tx_blocks = NULL; /* reference to an array of transmit block references */
static uint32_t num_blocks = 0;
static uint32_t block_size_in_words = DEFAULT_BLOCK_SIZE;
static uint64_t timestamp = DEFAULT_INIT_TIMESTAMP;
static char* p_rx_hdl = DEFAULT_RX_HDL;
static char* p_tx_hdl = DEFAULT_TX_HDL;
static skiq_rx_hdl_t rx_hdl =  skiq_rx_hdl_end;
static skiq_tx_hdl_t tx_hdl =  skiq_tx_hdl_end;

static FILE* input_fp=NULL;
static FILE* output_fp=NULL;

uint32_t num_complete_rx_blocks;  // # full blocks to receive
uint32_t last_block_num_bytes;    // extra # bytes to receive on last block

uint32_t* p_rx_iq;  // pointer to the Rx data

pthread_t tx_thread; // transmit thread

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
    APP_ARG_OPT("num-tx-loops",
                0,
                "Number of TX iterations",
                NULL,
                &num_tx_loops,
                UINT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("gain",
                        'g',
                        "Manually configure the gain by index rather than using automatic",
                        "index",
                        &rx_gain,
                        UINT8_VAR_TYPE,
                        &rx_gain_is_present), 
    APP_ARG_OPT("rx-hdl",
                0,
                "Rx handle to use, either A1, A2, B1, B2, C1, D1",
                NULL,
                &p_rx_hdl,
                STRING_VAR_TYPE), 
    APP_ARG_OPT("tx-hdl",
                0,
                "Tx handle to use, either A1, A2, B1",
                NULL,
                &p_tx_hdl,
                STRING_VAR_TYPE), 
    APP_ARG_OPT("block-size",
                0,
                "Number of samples to transmit per block (in words)",
                "N",
                &block_size_in_words,
                UINT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("timestamp",
                't',
                "Initial timestamp value",
                "N",
                &timestamp,
                UINT64_VAR_TYPE,
                &timestamp_is_present),
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
    // set the flag for everything to cleanup
    running=false;
}

/*****************************************************************************/
/** This is the main function for executing the fdd_rx_tx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    int32_t status=0;
    bool skiq_initialized = false;
    pid_t owner = 0;

    app_name = argv[0];

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize everything based on the arguments provided */
    status = process_cmd_line_args( argc, argv );
    if (0 != status)
    {
        goto finished;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize libsidekiq for the card specified */
    status = skiq_init(skiq_xport_type_pcie, skiq_xport_init_level_full,
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
        goto finished;
    }
    skiq_initialized = true;

    /* prepare the interfaces */
    if( (status=configure_sample_rate()) != 0 )
    {
        fprintf(stderr, "Error: unable to configure sample rate.\n");
        goto finished;
    }
    if( (status=prepare_rx()) != 0 )
    {
        fprintf(stderr, "Error: unable to initialize Rx parameters\n");
        goto finished;
    }
    if( (status=prepare_tx()) != 0 )
    {
        fprintf(stderr, "Error: unable to initialize Tx parameters\n");
        goto finished;
    }

    // reset the timestamp
    status = skiq_reset_timestamps(card);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to reset timestamps (result code %"
                PRIi32 " card=%u )\n", status, card);
        goto finished;
    }

    /* fire off a thread to handle transmit tasks */
    pthread_create(&tx_thread, NULL, send_samples, NULL);

    printf("Info: start Recv samples\n");
    recv_samples();
    printf("Info: done receiving samples\n");

    if (pthread_join(tx_thread,NULL))
    {
        fprintf(stderr, "Error: failed to join Tx thread\n");
        status = -1;
        goto finished;
    }
    else
    {
        status = 0;
    }

    printf("Info: Success\n");

finished:
    /* close files and cleanup */
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

    if (skiq_initialized)
    {
        if (NULL != p_tx_blocks)
        {
            uint32_t curr_block = 0;
            for( curr_block=0; curr_block<num_blocks; curr_block++ )
            {
                if (p_tx_blocks[curr_block] != NULL) 
                {
                    skiq_tx_block_free( p_tx_blocks[curr_block] );
                    p_tx_blocks[curr_block] = NULL;
                }
            }
            free( p_tx_blocks );
            p_tx_blocks = NULL;
        }

        skiq_exit();
        skiq_initialized = false;
    }

    return status;
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
        fprintf(stderr, "Error: unable to configure Rx sample rate and bandwidth\n");
        return status;
    }
    if( (status=skiq_write_tx_sample_rate_and_bandwidth(card,
                                                        tx_hdl,
                                                        sample_rate,
                                                        sample_rate)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx sample rate and bandwidth\n");
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
    uint32_t num_bytes_to_alloc=0;
    int32_t status=0;

    // configure the Rx parameters

    /* set the gain_mode based on rx_gain from the arguments */
    if( (status=skiq_write_rx_gain_mode(card, rx_hdl, (rx_gain_is_present ? skiq_rx_gain_manual : skiq_rx_gain_auto))) != 0 )
    {
        fprintf(stderr, "Error: unable to set Rx gain mode\n");
        return (status);
    }
    if (rx_gain_is_present)
    {
        if( (status=skiq_write_rx_gain(card, rx_hdl, rx_gain)) != 0 )
        {
            fprintf(stderr, "Error: unable to set Rx gain\n");
            return (status);
        }
        printf("Info: set gain index to %" PRIu8 "\n", rx_gain);
    }
    else
    {
        printf("Info: set rx_gain mode to skiq_rx_gain_auto\n");
    }


    if( (status=skiq_write_rx_LO_freq(card, rx_hdl, rx_lo_freq)) != 0 )
    {
        fprintf(stderr, "Error: unable to set Rx LO frequency\n");
        return (status);
    }

    // determine number of complete blocks to receive and # bytes for last partial block
    num_complete_rx_blocks = num_samples_to_rx / NUM_RX_PAYLOAD_WORDS_IN_BLOCK;
    last_block_num_bytes = ((num_samples_to_rx % NUM_RX_PAYLOAD_WORDS_IN_BLOCK)*4);

    // allocate memory to save all the receive data
    num_bytes_to_alloc =
        (num_complete_rx_blocks*NUM_RX_PAYLOAD_WORDS_IN_BLOCK*sizeof(uint32_t)) + last_block_num_bytes;
    p_rx_iq = malloc( num_bytes_to_alloc );
    if( p_rx_iq == NULL )
    {
        fprintf(stderr, "Error: didn't successfully allocate %d bytes to hold unpacked iq\n",
               num_bytes_to_alloc);
        status = -1;
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
    uint16_t tx_atten=50;
    int32_t status=0;

    // initialize the transmit buffer
    status = init_tx_buffer();
    if (0 != status)
    {
        fprintf(stderr, "Error: unable to initialize tx buffer\n");
        return (status);
    }

    if( (status=skiq_write_tx_LO_freq(card, tx_hdl, tx_lo_freq)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx LO frequency\n");
        return (status);
    }
    if( (status=skiq_write_tx_attenuation(card, tx_hdl, tx_atten)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx attenuation\n");
        return (status);
    }
    if( (status=skiq_write_tx_data_flow_mode(card,
                                             tx_hdl,
                                             skiq_tx_with_timestamps_data_flow_mode)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx data flow mode\n");
        return (status);
    }
    if( (status=skiq_write_tx_block_size(card,
                                         tx_hdl,
                                         block_size_in_words)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx block size\n");
        return (status);
    }


    return (status);
}

/*****************************************************************************/
/** The recv_samples function is responsible for receiving the requested
    # of I/Q samples and storing them in a file.

    @param none
    @return void
*/
static void recv_samples(void)
{
    skiq_rx_hdl_t hdl;
    skiq_rx_block_t *p_rx_block;
    uint32_t len;
    uint32_t tot_blocks_acquired=0;
    uint64_t curr_timestamp=0;
    skiq_rx_status_t status=0;
    bool done=false;
    bool first_timestamp = true;
    uint32_t* p_next_rx_write;
    uint64_t next_timestamp = 0;

    p_next_rx_write = p_rx_iq;

    printf("Info: receiving samples\n");
    skiq_start_rx_streaming(card, rx_hdl);

    // call receive until all of the bytes are received
    while( (done==false) && (running==true) )
    {
        status = skiq_receive(card, &hdl, &p_rx_block, &len);
        if( skiq_rx_status_success == status )
        {
            if( hdl != rx_hdl )
            {
                fprintf(stderr, "Error: received unexpected data from hdl %u\n", hdl);
            }
            // check the timestamp if it isn't the first block
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
    }

    skiq_stop_rx_streaming(card, rx_hdl);

    // write the file
    if( running==true )
    {
        printf("Info: saving samples to the file\n");
        p_next_rx_write = p_rx_iq;
        for( len=0; len<num_complete_rx_blocks; len++ )
        {
            fwrite(p_next_rx_write, NUM_RX_PAYLOAD_WORDS_IN_BLOCK*4, 1, output_fp);
            p_next_rx_write += NUM_RX_PAYLOAD_WORDS_IN_BLOCK;
        }
        if( last_block_num_bytes > 0 )
        {
            fwrite(p_next_rx_write, last_block_num_bytes, 1, output_fp);
        }
    }
}

/*****************************************************************************/
/** The send_samples function is responsible for sending the samples from
    the user specified input file.

    @param data     A pointer to the data to send.
    @return Always NULL.
*/
static void* send_samples(void* data)
{
    int32_t status = 0;
    uint32_t curr_block=0;
    uint32_t timestamp_increment = block_size_in_words;
    uint32_t i,num_lates = 0;

    if( skiq_start_tx_streaming(card, tx_hdl) != 0 )
    {
        fprintf(stderr, "Error: unable to start Tx streaming\n");
        return NULL;
    }

    for( i=0; (i<num_tx_loops) && (running==true); i++ )
    {
        printf("Info: sending samples: (num_blocks=%d)\n", num_blocks);

        // transmit a block at a time
        while( (curr_block < num_blocks) && (running==true) )
        {
            skiq_tx_set_block_timestamp( p_tx_blocks[curr_block], timestamp);

            // transmit the data
            if (running)
            {
                status = skiq_transmit(card, tx_hdl, p_tx_blocks[curr_block], NULL );
                if (0 != status)
                {
                    fprintf(stderr, "Error: failed to transmit data on block %d (result code %"
                            PRIi32 ")\n", curr_block, status);
                    running = false;
                }
                curr_block++;
                // update the timestamp
                timestamp += timestamp_increment;
            }
        }
        curr_block=0;

        status = skiq_read_tx_num_late_timestamps(card, tx_hdl, &num_lates);
        if (status != 0)
        {
            fprintf(stderr, "Error: failed to read num late timestamps (result code %"
                    PRIi32 ")\n", status);
            running = false;
        }
        else if (num_lates > 0)
        {
            printf("Number of late timestamps: %d !\n", num_lates );
        }

        
        
    }

    status = skiq_stop_tx_streaming(card, tx_hdl);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: failed to stop TX streaming (result code %" PRIi32
                ")\n", status);
    }

    return NULL;
}

/*****************************************************************************/
/** This function extracts all cmd line args

    @param argc: the # of args to be processed
    @param argv: a pointer to the vector of arg strings
    @return: int32_t-indicating status
*/
static int32_t process_cmd_line_args(int argc, char* argv[])
{
    int32_t status = 0;

    /* extract all args */

    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return status;
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

    /* map argument values to sidekiq specific variable values */
    if( 0 == strcasecmp(p_rx_hdl, "A1") )
    {
        rx_hdl  = skiq_rx_hdl_A1;
        printf("Info: using Rx handle A1\n");
    }
    else if( 0 == strcasecmp(p_rx_hdl, "A2") )
    {
        rx_hdl  = skiq_rx_hdl_A2;
        printf("Info: using Rx handle A2\n");
    }
    else if( 0 == strcasecmp(p_rx_hdl, "B1") )
    {
        rx_hdl  = skiq_rx_hdl_B1;
        printf("Info: using Rx handle B1\n");
    }
    else if( 0 == strcasecmp(p_rx_hdl, "B2") )
    {
        rx_hdl  = skiq_rx_hdl_B2;
        printf("Info: using Rx handle B2\n");
    }
    else if( 0 == strcasecmp(p_rx_hdl, "C1") )
    {
        rx_hdl  = skiq_rx_hdl_C1;
        printf("Info: using Rx handle C1\n");
    }
    else if( 0 == strcasecmp(p_rx_hdl, "D1") )
    {
        rx_hdl  = skiq_rx_hdl_D1;
        printf("Info: using Rx handle D1\n");
    }
    else
    {
        fprintf(stderr, "Error: Invalid Rx handle %s specified!\n", p_rx_hdl);
        status = -EINVAL;
        goto finished;
    }
    
    /* map argument values to sidekiq specific variable values */
    if( 0 == strcasecmp(p_tx_hdl, "A1") )
    {
        tx_hdl  = skiq_tx_hdl_A1;
        printf("Info: using Tx handle A1\n");
    }
    else
    {
        fprintf(stderr, "Error: Invalid Tx handle %s specified for FDD operation!\n", p_tx_hdl);
        status = -EINVAL;
        goto finished;
    }
    /* ----------------first rx args---------------- */
    output_fp=fopen(output_filepath,"wb");
    if (output_fp == (FILE*)NULL)
    {
        fprintf(stderr, "Error: unable to open output file %s\n",output_filepath);
        status = -1;
        goto finished;
    }
    printf("Info: opened file %s to hold the received IQ data \n",output_filepath);
    printf("Info: Requested Rx LO freq will be %" PRIu64 " Hz\n",rx_lo_freq);
    printf("Info: Requested sample rate is %d\n", sample_rate);
    printf("Info: Requested # of I/Q sample pairs to acquire is %d\n",num_samples_to_rx);

    /* ----------------and now for tx args---------------- */
    input_fp=fopen(input_filepath,"rb");
    if (input_fp == (FILE*)NULL)
    {
        fprintf(stderr, "Error: unable to open input file %s with status %" PRIi32 "\n",input_filepath, status);
        status = -errno;
        goto finished;
    }
    printf("Info: opened file %s for reading transmit IQ data\n",input_filepath);
    printf("Info: Requested Tx LO freq will be %" PRIu64 " Hz\n",tx_lo_freq);
    printf("Info: Requested # of Tx loop iterations to be %d\n",num_tx_loops);

    /* check the initial tx timestamp value */
    if( timestamp_is_present )
    {
        if ( timestamp == 0)
        {
            fprintf(stderr, "Error: initial Tx timestamp for card %u, Tx handle %s cannot be zero!\n", card, p_tx_hdl);
            status = -EINVAL;
            goto finished;
        }

    } 
    
finished:
    if (0 != status)
    {
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
    }

    return status;
}

/*****************************************************************************/
/** This function allocates and populates a skiq_tx_block
    @return: int32_t-indicating status
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

            /* allocate a transmit block by number of words */
            p_tx_blocks[i] = skiq_tx_block_allocate( block_size_in_words );
            
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
