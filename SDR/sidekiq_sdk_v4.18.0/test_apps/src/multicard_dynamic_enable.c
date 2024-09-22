/*! \file multicard_dynamic_enable.c
 * \brief This file contains a basic application for dynamically
 * enabling / disabling cards
 *
 * <pre>
 * Copyright 2012-2018 Epiq Solutions, All Rights Reserved
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

/* flag indicating that we want to check timestamps for loss of data */
#define CHECK_TIMESTAMPS (1)

/* number of payload words in a packet (not including the header) */
#define NUM_PAYLOAD_WORDS_IN_BLOCK (SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS-SKIQ_RX_HEADER_SIZE_IN_WORDS)

static bool running=true;

static uint64_t lo_freq = 850000000;
static uint32_t sample_rate = 10000000;

pthread_t card_thread[SKIQ_MAX_NUM_CARDS];
int32_t thread_status[SKIQ_MAX_NUM_CARDS];

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    printf("Info: received signal %d, cleaning up libsidekiq...\n", signum);
    running = false;
}

/*****************************************************************************/
/** This is the main function for processing a specific card.  This includes
    configuring the Rx interface and receiving the data for the card.

    @param data pointer to a uint8_t that contains the card number to process
    @return void*-indicating status
*/
void* process_card( void *data )
{
    /* variables that need to be tracked per card */
    uint64_t curr_ts;  // current timestamp
    uint64_t next_ts;  // next timestamp
    bool first_block = true;  // indicates if the first block has been received
    uint32_t num_payload_words_read;
    /* variables used to manage the currently received buffer*/
    skiq_rx_block_t* p_rx_block;        // reference to a receive block
    uint32_t len;                    // length (in bytes) of received data

    skiq_rx_hdl_t curr_rx_hdl = skiq_rx_hdl_A1;       // current handle

    uint8_t card = *((uint8_t*)(data)); // sidekiq card this thread is using

    int32_t status=0;      // status of various libsidekiq calls
    skiq_rx_status_t rx_status;

    printf("Processing card %u at sample rate %u\n", card, sample_rate);

    /**********************configure the Rx ************************/

    /* initialize the receive parameters for each handle */

    /* write the data source to counter */
    skiq_write_rx_data_src(card,curr_rx_hdl,skiq_data_src_counter);

    /* set the sample rate and bandwidth */
    status = skiq_write_rx_sample_rate_and_bandwidth(card,
                                                     curr_rx_hdl,
                                                     sample_rate,
                                                     sample_rate);
    if (status < 0)
    {
        printf("Error: failed to set Rx sample rate or bandwidth(using"
               " default from last config file)...status is %d\n",
               status);
    }

    /* tune the Rx chain to the requested freq */
    status=skiq_write_rx_LO_freq(card,curr_rx_hdl,lo_freq);
    if (status < 0)
    {
        printf("Error: failed to set LO freq (using previous LO freq)...status is %d\n",
               status);
    }

    /* initialize next_ts and total_blocks_acquired */
    next_ts = 0llu;

    /************************** start Rx data flowing ***************************/

    printf("Info: starting Rx interface(s) on card %u\n", card);
    status = skiq_start_rx_streaming_multi_immediate(card, &curr_rx_hdl, 1);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: starting Rx interface(s) on card %u failed with status %d\n", card, status);
        goto thread_exit;
    }

    /************************** receive data ***************************/

    /* loop through until there are no more receive handles needing data */
    while ( (running==true) )
    {
        /* try to grab a packet of data */
        rx_status = skiq_receive(card, &curr_rx_hdl, &p_rx_block, &len);
        if ( skiq_rx_status_success == rx_status )
        {
            /* determine the number of words read based on the number of bytes
               returned from the receive call less the header */
            num_payload_words_read = (len/4) - SKIQ_RX_HEADER_SIZE_IN_WORDS;

#if CHECK_TIMESTAMPS
            curr_ts = p_rx_block->rf_timestamp; /* peek at the timestamp */
            /* if this is the first block received then the next timestamp to expect
               needs to be initialized */
            if (first_block == true )
            {
                first_block = false;
                /* will be incremented properly below for next time through */
                next_ts = curr_ts;
            }
            /* validate the timestamp */
            else if (curr_ts != next_ts)
            {
                printf("Error: timestamp error for %u/%u...expected 0x%016" PRIx64 " but got 0x%016" PRIx64 "\n",
                       card, curr_rx_hdl,
                       next_ts, curr_ts);
                /* update the next timestamp expected based on the current timestamp
                   since there was just a gap in the data */
                next_ts = curr_ts;
            }
#endif

            /* update the next expected timestamp based on the number of words received */
            next_ts += num_payload_words_read;

            // TODO: verify counter data
        }
    }

    curr_rx_hdl = skiq_rx_hdl_A1;
    /* all done, so stop streaming */
    printf("Info: stopping Rx interface(s) on card %u\n", card);
    status = skiq_stop_rx_streaming_multi_immediate(card, &curr_rx_hdl, 1);

thread_exit:
    thread_status[card] = status;
    return (void*)((&thread_status[card]));
}

/*****************************************************************************/
/** This is the main function for executing the multicard_rx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    uint8_t cards[SKIQ_MAX_NUM_CARDS]; /* all available Sidekiq card #s */
    uint8_t num_cards=0;
    uint8_t i=0;
    int32_t status = 0;
    int32_t *card_status[SKIQ_MAX_NUM_CARDS];
    uint8_t curr_ena_cards[SKIQ_MAX_NUM_CARDS] = { [0 ... SKIQ_MAX_NUM_CARDS-1] = 0 };
    uint8_t curr_num_ena_cards = 0;
    uint8_t curr_dis_cards[SKIQ_MAX_NUM_CARDS] = { [0 ... SKIQ_MAX_NUM_CARDS-1] = 0 };
    uint8_t curr_num_dis_cards = 0;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    skiq_get_cards( skiq_xport_type_pcie, &num_cards, cards );

    printf("Info: initializing %" PRIu8 "card(s)...\n", num_cards);

    /* bring up all the cards in the system */
    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, cards, num_cards);
    if( status != 0 )
    {
        if ( EBUSY == status )
        {
            printf("Error: unable to initialize libsidekiq; one or more cards"
                    " seem to be in use (result code %" PRIi32 ")\n", status);
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

    //////////////////////
    // run all cards
    /* start a new thread for each card */
    running = true;
    for( i=0; i<num_cards; i++ )
    {
        pthread_create( &(card_thread[i]), NULL, process_card, (void*)(&(cards[i])) );
    }

    printf("Allowing cards to run for a moment\n");
    sleep(10);
    printf("Stopping cards\n");
    running = false;

    /* wait for the threads to complete */
    for( i=0; i<num_cards; i++ )
    {
        pthread_join( card_thread[i], (void**)(&(card_status[i])) );
        if( *(card_status[i]) == 0 )
        {
            printf("Info: completed processing receive for card %u successfully!\n", i);
        }
        else
        {
            printf("Error: an error (%d) occurred processing card %u\n", *(card_status[i]), i);
        }
    }
    //////////////////////

    //////////////////////
    // disable odd numbered cards, enable even
    curr_num_ena_cards = 0;
    curr_num_dis_cards = 0;
    for( i=0; i<num_cards; i++ )
    {
        if( (cards[i]%2) != 0 )
        {
            curr_dis_cards[curr_num_dis_cards] = cards[i];
            curr_num_dis_cards++;
            printf("Adding card %u to disable list\n", cards[i]);
        }
        else
        {
            curr_ena_cards[curr_num_ena_cards] = cards[i];
            curr_num_ena_cards++;
            printf("Adding card %u to enable list\n", cards[i]);
        }
    }
    if( (status=skiq_disable_cards( curr_dis_cards, curr_num_dis_cards ))== 0 )
    {
        printf("Successfully disabled cards\n");
    }
    else
    {
        printf("Error: disabling cards failed with status %d\n", status);
    }

    /* start a new thread for each enabled card */
    running = true;
    for( i=0; i<curr_num_ena_cards; i++ )
    {
        pthread_create( &(card_thread[i]), NULL, process_card, (void*)(&(curr_ena_cards[i])) );
    }

    printf("Allowing cards to run for a moment\n");
    sleep(10);
    printf("Stopping cards\n");
    running = false;

    /* wait for the threads to complete */
    for( i=0; i<curr_num_ena_cards; i++ )
    {
        pthread_join( card_thread[i], (void**)(&(card_status[i])) );
        if( *(card_status[i]) == 0 )
        {
            printf("Info: completed processing receive for card %u successfully!\n", i);
        }
        else
        {
            printf("Error: an error (%d) occurred processing card %u\n", *(card_status[i]), i);
        }
    }
    //////////////////////

    //////////////////////
    // disable even numbered cards, enable odd
    curr_num_ena_cards = 0;
    curr_num_dis_cards = 0;
    for( i=0; i<num_cards; i++ )
    {
        if( (cards[i]%2) == 0 )
        {
            curr_dis_cards[curr_num_dis_cards] = cards[i];
            curr_num_dis_cards++;
            printf("Adding card %u to disable list\n", cards[i]);
        }
        else
        {
            curr_ena_cards[curr_num_ena_cards] = cards[i];
            curr_num_ena_cards++;
            printf("Adding card %u to enable list\n", cards[i]);
        }
    }
    if( (status=skiq_disable_cards( curr_dis_cards, curr_num_dis_cards ))== 0 )
    {
        printf("Successfully disabled cards\n");
    }
    else
    {
        printf("Error: disabling cards failed with status %d\n", status);
    }

    if( (status=skiq_enable_cards( curr_ena_cards, curr_num_ena_cards, skiq_xport_init_level_full ))== 0 )
    {
        printf("Successfully enabled cards\n");
    }
    else
    {
        printf("Error: enabling cards failed with status %d\n", status);
    }

    /* start a new thread for each enabled card */
    running = true;
    for( i=0; i<curr_num_ena_cards; i++ )
    {
        pthread_create( &(card_thread[i]), NULL, process_card, (void*)(&(curr_ena_cards[i])) );
    }

    printf("Allowing cards to run for a moment\n");
    sleep(10);
    printf("Stopping cards\n");
    running = false;

    /* wait for the threads to complete */
    for( i=0; i<curr_num_ena_cards; i++ )
    {
        pthread_join( card_thread[i], (void**)(&(card_status[i])) );
        if( *(card_status[i]) == 0 )
        {
            printf("Info: completed processing receive for card %u successfully!\n", i);
        }
        else
        {
            printf("Error: an error (%d) occurred processing card %u\n", *(card_status[i]), i);
        }
    }
    //////////////////////

    skiq_exit();

    return (0);
}
