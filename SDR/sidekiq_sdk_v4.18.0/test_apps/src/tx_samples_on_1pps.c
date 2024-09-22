/*! \file tx_samples_on_1pps.c
 * \brief This file contains a basic application for transmitting
 * data, starting/stopping on a 1PPS edge.
 *
 * <pre>
 * Copyright 2014 - 2018 Epiq Solutions, All Rights Reserved
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
#include <fcntl.h>
#include <sidekiq_api.h>
#include <pthread.h>
#include <inttypes.h>

#include "arg_parser.h"

pthread_t ctrl_thread[SKIQ_MAX_NUM_CARDS];     // thread responsible for starting/stopping on 1PPS
pthread_t transmit_thread[SKIQ_MAX_NUM_CARDS]; // thread responsible for transmitting data

pthread_cond_t stop_streaming = PTHREAD_COND_INITIALIZER; // condition variable to signal stop of streaming
pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;   // mutex used with stop_streaming cond var

// condition variable to signal tx_enabled
pthread_cond_t tx_enabled[SKIQ_MAX_NUM_CARDS] =
{
    [ 0 ... (SKIQ_MAX_NUM_CARDS-1) ] = PTHREAD_COND_INITIALIZER,
};
// mutex used with tx_enabled cond var
pthread_mutex_t tx_enabled_mutex[SKIQ_MAX_NUM_CARDS] =
{
    [ 0 ... (SKIQ_MAX_NUM_CARDS-1) ] = PTHREAD_MUTEX_INITIALIZER,
};

bool stream_started[SKIQ_MAX_NUM_CARDS];  // flag indicating that the stream has started
bool stream_complete[SKIQ_MAX_NUM_CARDS]; // flag indicating that the stream has stopped

// various cmd line parameters
static FILE *input_fp = NULL;
static char* p_file_path = NULL;
static uint64_t lo_freq = 850000000;
static uint64_t freq_offset = 10000000;
static uint16_t attenuation = 100;
static uint32_t sample_rate = 1000000;
static uint32_t bandwidth = 0;
static uint32_t block_size_in_words = 1020;
static uint32_t duration = 5;
static bool packed = false;
static char* p_pps_source = NULL;
static skiq_1pps_source_t pps_source = skiq_1pps_source_unavailable;
static uint8_t num_cards=0;
static bool running=true;

static skiq_tx_block_t **p_tx_blocks = NULL; /* reference to array of transmit block references */

uint32_t num_blocks=0;    // # blocks contained in the file


/* local functions */
static int32_t process_cmd_line_args(int argc, char* argv[]);
static void app_cleanup(int signum);
static void tx_enabled_callback(uint8_t card, int32_t status);

static int32_t init_tx_buffer(void);

static void* transmit_card( void *data );
void* ctrl_card( void *data );

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- transmit samples from multiple cards and/or handles starting and ending on a 1PPS pulse";
static const char* p_help_long = 
"\
   Tune to the user-specifed Tx freq and transmit the sample file\n\
   for the duration specified on all Sidekiq cards. \n\
   Each card starts transmitting on the next 1PPS edge.\n\
\n\
   The data is stored in the file as 16-bit I/Q pairs with 'I' samples\n\
   stored in the the lower 16-bits of each word, and 'Q' samples stored\n\
   in the upper 16-bits of each word, resulting in the following format:\n\
           -31-------------------------------------------------------0-\n\
           |         12-bit I0           |       12-bit Q0            |\n\
    word 0 | (sign extended to 16 bits   | (sign extended to 16 bits) |\n\
           ------------------------------------------------------------\n\
           |         12-bit I1           |       12-bit Q1            |\n\
    word 1 | (sign extended to 16 bits   | (sign extended to 16 bits) |\n\
           ------------------------------------------------------------\n\
           |         12-bit I2           |       12-bit Q2            |\n\
    word 2 |  (sign extended to 16 bits  | (sign extended to 16 bits) |\n\
           ------------------------------------------------------------\n\
           |           ...               |          ...               |\n\
           ------------------------------------------------------------\n\n\
   Each I/Q sample is little-endian, twos-complement, signed, and sign-extended\n\
   from 12-bits to 16-bits.\n\n\
\n\
Defaults:\n\
  --attenuation=100\n\
  --block-size=1020\n\
  --frequency=850000000\n\
  --rate=1000000\n\
  --time=5";

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
                UINT32_VAR_TYPE),
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to transmit samples at in Hertz",
                "Hz",
                &lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("frequency-offset",
                0,
                "Frequency offset to transmit samples of additional cards in Hertz",
                "Hz",
                &freq_offset,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_REQ("source",
                's',
                "Input file to source for I/Q data",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("packed",
                0,
                "Transmit packed mode data",
                NULL,
                &packed,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("pps-source",
                0,
                "Source of 1PPS signal (external or host)",
                NULL,
                &p_pps_source,
                STRING_VAR_TYPE),
    APP_ARG_OPT("time",
                't',
                "Number of seconds to transmit",
                "SECONDS",
                &duration,
                UINT32_VAR_TYPE),    
    APP_ARG_TERMINATOR,
};


/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    uint8_t i=0;

    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);

    running = false;

    for( i=0; i<num_cards; i++ )
    {
        pthread_cancel( transmit_thread[i] );
        pthread_cancel( ctrl_thread[i] );
    }
}

/*****************************************************************************/
/** This function is called when TX is enabled and skiq_transmit() is ready to
    call.

    @param card card number of Sidekiq TX enabled
    @param status status of enabling transmit
    @return: void
*/
void tx_enabled_callback(uint8_t card, int32_t status)
{
    pthread_cond_broadcast( &tx_enabled[card] );
}

/*****************************************************************************/
/** This function reads the contents of the file into p_tx_buf

    @param none
    @return: void
*/
static int32_t init_tx_buffer(void)
{
    int32_t status = 0;
    int read_status = 0;
    uint32_t num_bytes_in_file=0;
    int32_t num_bytes_read=1;
    int32_t i = 0;
    int32_t j = 0;

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

    // read in the contents of the file
    for (i = 0; i < num_blocks; i++)
    {
        if ( !feof(input_fp) && (num_bytes_read>0) )
        {
            /* allocate a transmit block by number of words */
            p_tx_blocks[i] = skiq_tx_block_allocate( block_size_in_words );

            if ( p_tx_blocks[i] == NULL )
            {
                fprintf(stderr, "Error: unable to allocate transmit block\n");
                status = -1;
                goto finished;
            }

            // read a block of samples
            num_bytes_read = fread( p_tx_blocks[i]->data, sizeof(int32_t),
                                    block_size_in_words, input_fp );
        }
        read_status = ferror(input_fp);
        if ( 0 != read_status )
        {
            fprintf(stderr, "Error: error while reading from input file (result"
                    " code %d)\n", read_status);
            status = -1;
            goto finished;
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
/** This is the main function for transmitting data for a specific card.

    @param data pointer to a uint8_t that contains the card number to process
    @return void*-indicating status
*/
void* transmit_card( void *data )
{
    uint8_t card = *((uint8_t*)(data)); // sidekiq card this thread is using

    uint32_t curr_block=0;
    uint64_t timestamp=0;
    uint32_t timestamp_increment=0;
    bool first_time=true;

    if( packed == true )
    {
        timestamp_increment = SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK(block_size_in_words);
    }
    else
    {
        timestamp_increment = block_size_in_words;
    }
    printf("Timestamp increment is %u\n", timestamp_increment);


    // replay the file until the stream is completed
    while( (stream_complete[card] == false) && (running==true) )
    {
        printf("Info: transmitting the file for card %u\n", card);
        // transmit a block at a time
        while( (curr_block < num_blocks) && (stream_complete[card] == false) &&
               (running==true) )
        {
            skiq_tx_set_block_timestamp( p_tx_blocks[curr_block], timestamp );
            if( first_time == true )
            {
                // wait for TX to be enabled
                printf("Waiting for TX to be enabled for card %u\n", card);
                pthread_mutex_lock( &tx_enabled_mutex[card] );
                pthread_cond_wait( &tx_enabled[card], &tx_enabled_mutex[card] );
                pthread_mutex_unlock( &tx_enabled_mutex[card] );
                first_time = false;
            }
            // transmit the data
            skiq_transmit(card, skiq_tx_hdl_A1, p_tx_blocks[curr_block], NULL );
            curr_block++;
            // update the timestamp
            timestamp += timestamp_increment;
        }
        curr_block = 0;
    }

    return (NULL);
}

/*****************************************************************************/
/** This is the main funciton for the thread responsible for starting and
    stopping streaming on the 1PPS edge.

    @param data pointer to a uint8_t that contains the card number to process
    @return void*-indicating status
*/
void* ctrl_card( void *data )
{
    uint8_t card = *((uint8_t*)(data)); // sidekiq card this thread is using
    uint32_t errors=0;

    /************************** start Tx data flowing ***************************/

    /* NOTE: There is a possibility that the streams won't start on all of the
       cards at the exact same time since we're scheduling it to happen on the
       next 1PPS (specifying a timestamp of 0 to reset and start).  Ideally, the
       current system timestamp should be sampled (using
       skiq_read_curr_sys_timestamp) and the last 1PPS timestamp should be read
       (using skiq_read_last_1pps_timestamp) to determine where in time the
       current timestamp is relative to the last 1PPS. This should allow for the
       streaming to be scheduled based on this information, as well as the
       frequency of the system timestamp (as defined by SKIQ_SYS_TIMESTAMP_FREQ)
       to ensure that all of the cards start on the same 1PPS edge.
     */

    /* setup the timestamps to reset on the next PPS */
    skiq_write_timestamp_reset_on_1pps(card, 0);

    /* begin streaming on the Tx interface */
    printf("starting on pps card %u\n",card);
    /* start streaming on the next PPS */
    if( skiq_start_tx_streaming_on_1pps(card, skiq_tx_hdl_A1, 0) != 0 )
    {
        fprintf(stderr, "Error: unable to start streaming on card %u\n", card);
        skiq_exit();
        _exit(-1);
    }
    printf("stream started on card %u\n",card);
    stream_started[card] = true;

    if( running == true )
    {
        // wait to be signaled to finish
        pthread_mutex_lock(&stop_mutex);
        pthread_cond_wait(&stop_streaming, &stop_mutex);
        pthread_mutex_unlock(&stop_mutex);
    }

    /* stop streaming on the Tx interface */
    printf("Info: stopping Tx interface\n");
    skiq_read_tx_num_underruns(card, skiq_tx_hdl_A1, &errors);
    printf("Info: number of tx underruns is %u for card %u\n", errors, card);
    skiq_stop_tx_streaming_on_1pps(card, skiq_tx_hdl_A1, 0);
    printf("stream stopped on card %u\n",card);
    stream_complete[card] = true;

    return (NULL);
}

/*****************************************************************************/
/** This is the main function for executing the tx_samples_on_1pps app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    uint8_t cards[SKIQ_MAX_NUM_CARDS]; /* all available Sidekiq card #s */
    uint8_t i=0;
    uint32_t j = 0;
    uint8_t card=0;
    uint8_t started=0;
    pid_t owner = 0;

    /* values read back after configuring */
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    uint32_t read_sample_rate;
    double actual_sample_rate;

    int32_t status=0;
    bool skiq_initialized = false;

    signal(SIGINT, app_cleanup);

    status = process_cmd_line_args(argc, argv);
    if (0 != status)
    {
        status = -1;
        goto cleanup;
    }

    if( p_pps_source != NULL )
    {
        if( 0 == strcasecmp(p_pps_source, "HOST") )
        {
            pps_source = skiq_1pps_source_host;
        }
        else if( 0 == strcasecmp(p_pps_source, "EXTERNAL") )
        {
            pps_source = skiq_1pps_source_external;
        }
        else
        {
            fprintf(stderr, "Error: invalid 1PPS source %s specified", p_pps_source);
            return (-1);
        }
    }

    skiq_get_cards( skiq_xport_type_auto, &num_cards, cards );

    printf("Info: initializing %" PRIu8 " card(s)...\n", num_cards);

    /* bring up the PCIe interface for all the cards in the system */
    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, cards, num_cards);
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
        status = -1;
        goto cleanup;
    }
    skiq_initialized = true;

    /* configure the 1PPS source for each of the cards */
    if( pps_source != skiq_1pps_source_unavailable )
    {
        for( i=0; i<num_cards && status == 0; i++ )
        {
            status = skiq_write_1pps_source( i, pps_source );
            if( status != 0 )
            {
                fprintf(stderr,"Error: unable to configure PPS source to %s for card %u (status=%d)\n",
                        p_pps_source, i, status );
                goto cleanup;
            }
            else
            {
                printf("Info: configured 1PPS source to %u\n", pps_source);
            }
        }
    }


    // initialize the transmit buffer
    status = init_tx_buffer();
    if (0 != status)
    {
        status = -1;
        goto cleanup;
    }

    /* perform some initialization for all of the cards */
    for( i=0; i<num_cards; i++ )
    {
        /* initialize the stream complete flag */
        stream_started[i] = false;
        stream_complete[i] = false;

        /* grab the card that we're talking to */
        card = cards[i];

        // configure our Tx parameters
        status = skiq_write_tx_sample_rate_and_bandwidth(card, skiq_tx_hdl_A1,
                    sample_rate, bandwidth);
        if( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure Tx sample rate (return"
                    " code %" PRIi32 ")\n", status);
            goto cleanup;
        }
        status = skiq_read_tx_sample_rate_and_bandwidth(card, skiq_tx_hdl_A1,
                    &read_sample_rate, &actual_sample_rate,
                    &read_bandwidth, &actual_bandwidth);
        if ( 0 == status )
        {
            fprintf(stderr, "Info: actual sample rate is %f, actual bandwidth"
                    " is %u\n", actual_sample_rate, actual_bandwidth);
        }
        printf("Info: configuring Tx LO frequency for card %u to %" PRIu64 "\n",
                card, lo_freq+(card*freq_offset));
        status = skiq_write_tx_LO_freq(card, skiq_tx_hdl_A1,
                    lo_freq+(card*freq_offset));
        if( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure Tx LO frequency"
                    " (return code %" PRIu32 ")\n", status);
            goto cleanup;
        }
        /* set the mode (packed or unpacked) */
        status = skiq_write_iq_pack_mode(card, packed);
        if( 0 != status )
        {
            if ( status == -ENOTSUP )
            {
                fprintf(stderr, "Error: packed mode is not supported on this Sidekiq product\n");
            }
            else
            {
                fprintf(stderr, "Error: unable to set the packed mode (return code %" PRIi32 ")\n",
                        status);
            }
            goto cleanup;
        }
        status = skiq_write_tx_attenuation(card, skiq_tx_hdl_A1, attenuation);
        if( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure Tx attenuation (return"
                   "  code %" PRIi32 ")\n", status);
            goto cleanup;
        }
        status = skiq_write_tx_data_flow_mode(card, skiq_tx_hdl_A1,
                    skiq_tx_immediate_data_flow_mode);
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure Tx data flow mode"
                    " (result code %" PRIi32 ")\n", status);
            goto cleanup;
        }
        status = skiq_write_tx_block_size(card, skiq_tx_hdl_A1,
                    block_size_in_words);
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure Tx block size (result"
                    " code %" PRIi32 ")\n", status);
            goto cleanup;
        }

        // register the callback so we're notified when TX is enabled and ready
        // for packets
        status = skiq_register_tx_enabled_callback(card, &tx_enabled_callback);
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to register TX enable callback"
                    " (result code %" PRIi32 ")\n", status);
            goto cleanup;
        }
    }

    /* start the receive threads for each card */
    for( i=0; i<num_cards; i++ )
    {
        pthread_create( &(transmit_thread[i]), NULL, transmit_card,
                        (void*)(&(cards[i])) );
    }

    /* start the ctrl thread for each card */
    for( i=0; i<num_cards; i++ )
    {
        pthread_create( &(ctrl_thread[i]), NULL, ctrl_card,
                        (void*)(&(cards[i])) );
    }

    /* set timer to signal to cards to stop */
    do
    {
        started = 0;
        for( i=0; i<num_cards; i++ )
        {
            card = cards[i];
            if( stream_started[card] == true )
            {
                started++;
                printf("card %u started\n", card);
            }
        }
        usleep(100*100); // wait for a bit and check again
    } while( (started < num_cards) && (running==true) );
    printf("all cards started\n");

    if( running==true )
    {
        // transmit for # seconds specified
        sleep(duration);

        // signal the threads to stop streaming
        pthread_cond_broadcast(&stop_streaming);
    }

    /* wait for the threads to complete */
    for( i=0; (i<num_cards) && (running==true); i++ )
    {
        pthread_join( ctrl_thread[i], NULL );
        pthread_join( transmit_thread[i], NULL );
        printf("Info: completed processing transmit for card %u"
               " successfully!\n", i);
    }

    status = 0;

cleanup:
    /* finalize / cleanup */

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
/** This function extracts all cmd line args

    @param argc: the # of args to be processed
    @param argv: a pointer to the vector of arg strings
    @return: void
*/
static int32_t process_cmd_line_args(int argc, char* argv[])
{
    int32_t status = 0;

    /* initialize everything based on the arguments provided */
    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -1;
        goto finished;
    }

    input_fp = fopen(p_file_path, "rb");
    if( input_fp == NULL )
    {
        fprintf(stderr, "Error: unable to open input file %s\n", p_file_path);
        status = -1;
        goto finished;
    }

    printf("Info: Requested Tx LO freq will be %" PRIu64 " Hz\n", lo_freq);
    printf("Info: Requested Tx freq offset will be %" PRIu64 " Hz\n",
            freq_offset);
    printf("Info: Requested Tx attenuation is %" PRIu16 "\n", attenuation);
    printf("Info: Requested Tx sample rate is %" PRIu32 "\n", sample_rate);
    printf("Info: Requested Tx channel bandwidth is %" PRIu32 "\n", bandwidth);
    printf("Info: Requested block size in words is %" PRIu32 "\n",
            block_size_in_words);

finished:
    if (0 != status)
    {
        if (NULL != input_fp)
        {
            fclose(input_fp);
            input_fp = NULL;
        }
    }

    return status;
}
