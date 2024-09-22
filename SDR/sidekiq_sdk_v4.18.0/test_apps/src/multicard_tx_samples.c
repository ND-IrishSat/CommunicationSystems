/*! \file multicard_tx_samples.c
 * \brief This file contains a basic application for transmitting
 * a file I/Q sample pairs for multiple Sidekiq cards simultaneously
 *
 * <pre>
 * Copyright 2014 - 2018 Epiq Solutions, All Rights Reserved
 * </pre>
 */
#ifdef __MINGW32__
/* we use MinGW's stdio since the msvcrt.dll version of sscanf won't support "%hhu" (aka SCNu8) */
#undef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif /* __MINGW32__ */


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
#include <sidekiq_api.h>
#include <pthread.h>

FILE *input_fp = NULL;
static char *app_name;
static uint64_t lo_freq;
static uint64_t freq_offset;
static uint16_t attenuation;
static uint32_t sample_rate;
static uint32_t bandwidth;
static uint32_t block_size_in_words;
static uint64_t timestamp=0;
static uint32_t repeat=0;
static uint8_t tx_mode=0;

static bool running=true;

static skiq_tx_block_t **p_tx_blocks = NULL; /* reference to an array of transmit block references */

uint32_t num_blocks=0;

pthread_t card_thread[SKIQ_MAX_NUM_CARDS];
int32_t thread_status[SKIQ_MAX_NUM_CARDS];

/* local functions */
static void print_usage(void);
static int32_t process_cmd_line_args(int argc, char* argv[]);
static int32_t init_tx_buffer(void);

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    printf("Info: received signal %d, cleaning up transmit threads\n", signum);
    running = false;
}

/*****************************************************************************/
/** This is the main function for processing a specific card.  This includes
    configuring the Tx interface and transmitting the data for the card.

    @param data pointer to a uint8_t that contains the card number to process
    @return void*-indicating status
*/
void* process_card( void *data )
{
    uint32_t curr_block=0;
    uint32_t errors=0;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint32_t num_repeat=repeat;

    int32_t ret_status=0;  // overall status of the thread
    uint8_t card = *((uint8_t*)(data)); // sidekiq card this thread is using

    // configure our Tx parameters
    if( (ret_status=skiq_write_tx_sample_rate_and_bandwidth(card, skiq_tx_hdl_A1, sample_rate, bandwidth)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx sample rate\n");
        goto thread_exit;
    }
    if( (ret_status=skiq_read_tx_sample_rate_and_bandwidth(card,
                                                           skiq_tx_hdl_A1,
                                                           &read_sample_rate,
                                                           &actual_sample_rate,
                                                           &read_bandwidth,
                                                           &actual_bandwidth)) == 0 )
    {
        printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
               actual_sample_rate, actual_bandwidth);
    }
    printf("Info: configuring Tx LO frequency for card %u to %" PRIu64 "\n",
           card, lo_freq+(card*freq_offset));
    if( (ret_status=skiq_write_tx_LO_freq(card, skiq_tx_hdl_A1, lo_freq+(card*freq_offset))) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx LO frequency\n");
        goto thread_exit;
    }
    if( (ret_status=skiq_write_tx_attenuation(card, skiq_tx_hdl_A1, attenuation)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx attenuation\n");
        goto thread_exit;
    }

    if( (ret_status=skiq_write_tx_data_flow_mode(card, skiq_tx_hdl_A1,
                                                 (skiq_tx_flow_mode_t)(tx_mode))) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx data flow mode\n");
        goto thread_exit;
    }

    if( (ret_status=skiq_write_tx_block_size(card, skiq_tx_hdl_A1,
                                             block_size_in_words)) != 0 )
    {
        fprintf(stderr, "Error: unable to configure Tx block size\n");
        goto thread_exit;
    }


    // reset the timestamp
    skiq_reset_timestamps(card);
    // enable the Tx streaming
    if( (ret_status=skiq_start_tx_streaming(card, skiq_tx_hdl_A1)) != 0 )
    {
        fprintf(stderr, "Error: unable to start streaming\n");
        goto thread_exit;
    }

    // replay the file the # times specified
    while( (num_repeat > 0) && (running==true) )
    {
        printf("Info: transmitting the file %u more times for card %u\n",
                num_repeat, card);

        // transmit a block at a time
        while( (curr_block < num_blocks) && (running==true) )
        {
            // transmit the data
            skiq_transmit(card, skiq_tx_hdl_A1, p_tx_blocks[curr_block], NULL );
            curr_block++;
        }
        num_repeat--;
        curr_block = 0;
    }

    // see how many underruns we had if we were running in immediate mode
    if( tx_mode == skiq_tx_immediate_data_flow_mode )
    {
        skiq_read_tx_num_underruns(card, skiq_tx_hdl_A1, &errors);
        printf("Info: number of tx underruns is %u for card %u\n",
                errors, card);
    }
    else
    {
        skiq_read_tx_num_late_timestamps(card, skiq_tx_hdl_A1, &errors);
        printf("Info: number of tx late detected is %u for card %u\n",
                errors, card);
    }

    // disable streaming, cleanup and shutdown
    skiq_stop_tx_streaming(card, skiq_tx_hdl_A1);

thread_exit:
    thread_status[card] = ret_status;
    return (void*)((&thread_status[card]));
}

/******************************************************************************/
/** This is the main function for executing the multicard_tx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    int32_t status = 0;
    uint8_t cards[SKIQ_MAX_NUM_CARDS]; /* all available Sidekiq card #s */
    uint8_t num_cards=0;
    uint8_t i=0;
    uint32_t j = 0;
    int32_t* card_status[SKIQ_MAX_NUM_CARDS];
    bool skiq_initialized = false;

    signal(SIGINT, app_cleanup);
    app_name = argv[0];

    skiq_get_cards( skiq_xport_type_pcie, &num_cards, cards );

    status = process_cmd_line_args(argc, argv);
    if (0 != status)
    {
        goto finished;
    }

    // initialize the transmit buffer
    init_tx_buffer();

    printf("Info: initializing %" PRIu8 " cards...\n", num_cards);

    /* bring up the PCIe interface for all the cards in the system */
    status = skiq_init(skiq_xport_type_pcie, skiq_xport_init_level_full,
                  cards, num_cards);
    if ( status != 0 )
    {
        if ( EBUSY == status )
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq; one or"
                    " more cards seem to be in use (result code %" PRIi32 ")\n",
                    status);
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
        goto finished;
    }
    skiq_initialized = true;

    /* start a new thread for each card */
    for( i=0; i<num_cards; i++ )
    {
        pthread_create( &(card_thread[i]), NULL, process_card,
                        (void*)(&(cards[i])) );
    }

    /* wait for the threads to complete */
    for( i=0; i<num_cards; i++ )
    {
        pthread_join( card_thread[i], (void**)(&(card_status[i])) );
        if( *(card_status[i]) == 0 )
        {
            printf("Info: completed processing receive for card %u"
                    " successfully!\n", i);
        }
        else
        {
            fprintf(stderr, "Error: an error (%d) occurred processing card"
                    " %u\n", *(card_status[i]), i);
        }
    }

finished:
    if (skiq_initialized)
    {
        skiq_exit();
        skiq_initialized = false;
    }

    if (NULL != input_fp)
    {
        fclose(input_fp);
        input_fp = NULL;
    }

    if (NULL != p_tx_blocks)
    {
        for (j = 0; j < num_blocks; j++)
        {
            skiq_tx_block_free(p_tx_blocks[j]);
        }

        free(p_tx_blocks);
        p_tx_blocks = NULL;
    }

    return ((int) status);
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
    int32_t num_bytes_read=1;
    uint32_t i = 0;
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

    // allocate the buffer
    p_tx_blocks = calloc( num_blocks + 1, sizeof( skiq_tx_block_t* ) );
    if( p_tx_blocks == NULL )
    {
        fprintf(stderr, "Error: unable to allocate %u bytes to hold transmit block descriptors\n",
                (uint32_t)((num_blocks + 1) * sizeof( skiq_tx_block_t* )));
        status = -1;
        goto finished;
    }

    // read in the contents of the file, one block at a time
    for (i = 0; i < num_blocks; i++)
    {
        if ( !feof(input_fp) && (num_bytes_read>0) )
        {
            /* allocate a transmit block by number of samples */
            p_tx_blocks[i] = skiq_tx_block_allocate( block_size_in_words );
            if ( p_tx_blocks[i] == NULL )
            {
                fprintf(stderr, "Error: unable to allocate a transmit block\n");
                status = -1;
                goto finished;
            }

            // copy in the timestamp
            skiq_tx_set_block_timestamp( p_tx_blocks[i], timestamp );

            // read another block of samples
            num_bytes_read = fread( p_tx_blocks[i]->data, sizeof(int32_t), block_size_in_words, input_fp );

            // update the timestamp
            timestamp += block_size_in_words;
        }
    }

finished:
    if (0 != status)
    {
        for (j = 0; j < i; j++)
        {
            skiq_tx_block_free(p_tx_blocks[j]);
        }

        if (NULL != p_tx_blocks)
        {
            free(p_tx_blocks);
            p_tx_blocks = NULL;
        }
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

    if( argc != 11 )
    {
        fprintf(stderr, "Error: invalid # arguments\n");
        print_usage();
        status = -1;
        goto finished;
    }
    input_fp=fopen(argv[1],"rb");
    if( input_fp == NULL )
    {
        fprintf(stderr, "Error: unable to open input file %s\n", argv[1]);
        status = -2;
        goto finished;
    }

    sscanf(argv[2], "%" PRIu64 "", &lo_freq);
    printf("Info: Requested Tx LO freq will be %" PRIu64 " Hz\n", lo_freq);
    sscanf(argv[3], "%" PRIu64 "", &freq_offset);
    printf("Info: Requested Tx freq offset will be %" PRIu64 " Hz\n", freq_offset);
    sscanf(argv[4], "%hu", &attenuation);
    printf("Info: Requested Tx attenuation is %u\n", attenuation);
    sscanf(argv[5], "%u", &sample_rate);
    printf("Info: Requested Tx sample rate is %u\n", sample_rate);
    sscanf(argv[6], "%u", &bandwidth);
    printf("Info: Requested Tx channel bandwidth is %u\n", bandwidth);

    sscanf(argv[7], "%u", &block_size_in_words);
    printf("Info: Requested block size in words is %d\n", block_size_in_words);
#ifdef __MINGW32__
    sscanf(argv[8], "%2" SCNu8, &tx_mode);
#else
    sscanf(argv[8], "%hhu", &tx_mode);
#endif /* __MINGW32__ */
    if( tx_mode == 0 )
    {
        printf("Info: Requested immediate tx data flow mode\n");
    }
    else
    {
        printf("Info: Requested timestamp tx data flow mode\n");
    }
    sscanf(argv[9], "%" PRIu64 "", &timestamp);
    printf("Info: Requested starting timestamp %" PRIu64 "\n", timestamp);
    sscanf(argv[10], "%u", &repeat);

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

/*****************************************************************************/
/** This function prints the main usage of the function.

    @param void
    @return void
*/
static void print_usage(void)
{
    printf("Usage: multicard_tx_samples <path to file with I/Q data to transmit>\n");
    printf("       <LO freq in Hz> <freq offset in Hz> <attenuation, 0-359> <sample rate in Hz>\n");
    printf("       <channel bandwidth in Hz> <block size> <mode (0: immediate, 1:timestamp)>\n");
    printf("       <starting timestamp> <# times to transmit file>\n");
    printf("   Configure the Tx lineup according to the specified parameters,\n"); 
    printf("   and open the specified file containing I/Q samples formatted as follows:\n");
    printf("   <16-bit Q0><16-bit I0><16-bit Q1><16-bit I1>...etc\n");
    printf("   (where each 16-bit value is a signed twos-complement little-endian value).\n");
    printf("   Note: in timestamp mode, the appropriate timestamps are automatically added\n");
    printf("   to the I/Q data as it is being sent out, without any gaps in the data.\n");
    printf("   I/Q data won't start transmitting out until the <initial timestamp>\n");
    printf("   has been reached.  In practice, a reasonable value for this\n");
    printf("   is on the order of 100000.  The same file is transmitted by each card detected,\n");
    printf("   with the LO frequency of each card varying by freq offset.  So, Sidekiq card 0 will transmit\n");
    printf("   at a frequency of LO freq specified, Sidekiq card 1 will transmit at LO freq + offset, \n");
    printf("   and Sidekiq card 2 will transmit at LO freq + offset*2.\n\n");
    printf("Example: ./multicard_tx_samples samples_file 850000000 5000000 50 1000000 1000000 1020 0 100000 5\n");
}
