/*! \file multicard_rx_samples.c
 * \brief This file contains a basic application for acquiring
 * a contiguous block of I/Q sample pairs for multiple Sidekiq
 * cards simultaneously
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

static const char* p_file_suffix[skiq_rx_hdl_end] = { "a1", "a2", "b1"};


static char* app_name;   /* name of application being ran */
/* filename of where to store the sample data, the actual file name will
   be appended with RF IC chip and card number */
static char* filename;

/* storage for all cmd line args */
static uint32_t num_payload_words_to_acquire;
static uint8_t data_src;
static skiq_rx_hdl_t hdl[skiq_rx_hdl_end];
static uint64_t lo_freq;
static uint32_t sample_rate;
static uint32_t bandwidth;
static uint16_t warp_voltage;
static uint32_t rx_gain;
static uint8_t include_meta;
static skiq_chan_mode_t chan_mode=skiq_chan_mode_single;
/* flag indicating whether the sample data should be written to the
   file immediately upon receiving it or if all of the data should be
   buffered first and then written to the file after all of the data
   requested has been received.  Depending on the platform and the
   location of the destination file, performance can be greatly
   impacted based on this selection */
static uint8_t write_file_immediate;

/* storage for parameters related to how many bytes we'll be receiving */
static uint32_t num_bytes_per_pkt;  // number of bytes for each packet received
static uint32_t num_complete_blocks = 0;  // # of complete blocks to receive
static uint32_t last_block_num_bytes = 0; // # of bytes needed from the last block

static bool running=true;

pthread_t card_thread[SKIQ_MAX_NUM_CARDS];
int32_t thread_status[SKIQ_MAX_NUM_CARDS];

/* local functions */
static void print_usage(void);
static void process_cmd_line_args(int argc, char* argv[]);
static void verify_data( FILE *pFile );

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
    /* variables that need to be tracked per rx handle */
    uint32_t total_blocks_acquired[skiq_rx_hdl_end]; // total # blocks acquired
    uint64_t curr_ts[skiq_rx_hdl_end];  // current timestamp
    uint64_t next_ts[skiq_rx_hdl_end];  // next timestamp
    uint8_t *p_start[skiq_rx_hdl_end];  // pointer to the start of the memory allocated
    uint8_t *rcv_buf[skiq_rx_hdl_end];  // buffer to copy the received data into
    FILE* output_fp[skiq_rx_hdl_end];   // pointer to file to save the samples
    bool first_block[skiq_rx_hdl_end];  // indicates if the first block has been received
    bool last_block[skiq_rx_hdl_end];   // indicates if the last block has been received

    /* keep track of the specified handles to start streaming */
    skiq_rx_hdl_t handles[skiq_rx_hdl_end];
    uint8_t nr_handles = 0;

    /* variables used to manage the currently received buffer*/
    uint32_t num_payload_words_read[skiq_rx_hdl_end]; // # payload words received
    skiq_rx_block_t* p_rx_block;        // reference to a receive block
    uint32_t len;                    // length (in bytes) of received data

    skiq_rx_hdl_t curr_rx_hdl;       // current handle
    uint8_t num_hdl_rcv = 0;         // # of handles currently receiving data

    /* values read back after configuring */
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;

    uint8_t card = *((uint8_t*)(data)); // sidekiq card this thread is using

    int32_t status=0;      // status of various libsidekiq calls
    int32_t ret_status=0;  // overall status of receive thread
    skiq_rx_status_t rx_status;
    char p_filename[100];

    /* open a file to write to for each handle enabled */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
        {
            snprintf(p_filename, 100, "%s.%s.%u",
                     filename, p_file_suffix[curr_rx_hdl], card);
            /* truncate the file if it's too long */
            p_filename[99] = '\0';
            output_fp[curr_rx_hdl]=fopen(p_filename,"wrb");
            if (output_fp[curr_rx_hdl] == NULL)
            {
                printf("Error: unable to open output file %s\n",p_filename);
                ret_status = -1;
                goto thread_exit;
            }
            printf("Info: opened file %s for output\n",p_filename);
        }
    }

    /* allocate the appropriate buffer size based on whether the whole
       amount of data needs to be buffered or if the data is going to be
       written immediately to file */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
        {
            if( (write_file_immediate == 1) )
            {
                rcv_buf[curr_rx_hdl] = malloc(SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES);
            }
            else
            {
                rcv_buf[curr_rx_hdl] = malloc((SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES*num_complete_blocks)+last_block_num_bytes);
            }

            if( rcv_buf[curr_rx_hdl] == NULL )
            {
                printf("Error: unable to allocate memory for card %u, handle %u!\n",
                       card, curr_rx_hdl);
                ret_status = -2;
                goto thread_close;
            }
            p_start[curr_rx_hdl] = rcv_buf[curr_rx_hdl];
        }
    }

    printf("Processing card %u at sample rate %u\n", card, sample_rate);

    /* initialize the data related to the handle */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
        {
            first_block[curr_rx_hdl] = true;
            last_block[curr_rx_hdl] = false;
            num_hdl_rcv++;
        }
    }

    /* initialize the warp voltage here to allow time for it to settle */
    if( (status=skiq_write_tcvcxo_warp_voltage(card,warp_voltage)) != 0 )
    {
        printf("Error: unable to set the warp voltage, using previous value\n");
    }

    /**********************configure the Rx ************************/

    /* initialize the receive parameters for each handle */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        /* only initialize if handle was requested */
        if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
        {
            /* write the data source (default is IQ) but we'll write it anyways just
               to be sure */
            skiq_write_rx_data_src(card,hdl[curr_rx_hdl],data_src);

            /* now that the Rx freq is set, set the gain mode and gain */
            status=skiq_write_rx_gain_mode(card,hdl[curr_rx_hdl],skiq_rx_gain_manual);
            if (status < 0)
            {
                printf("Error: failed to set Rx gain mode to manual\n");
            }
            status=skiq_write_rx_gain(card,hdl[curr_rx_hdl],rx_gain);
            if (status < 0)
            {
                printf("Error: failed to set Rx gain to %d dB\n",rx_gain);
            }

            /* set the sample rate and bandwidth */
            status = skiq_write_rx_sample_rate_and_bandwidth(card,
                        hdl[curr_rx_hdl],sample_rate,bandwidth);
            if (status < 0)
            {
                printf("Error: failed to set Rx sample rate or bandwidth(using"
                        " default from last config file)...status is %d\n",
                       status);
            }
            /*
                read back the sample rate and bandwidth to determine the
                achieved bandwidth
            */
            status = skiq_read_rx_sample_rate_and_bandwidth(card,
                        hdl[curr_rx_hdl], &read_sample_rate,
                        &actual_sample_rate, &read_bandwidth,
                        &actual_bandwidth);
            if ( status == 0 )
            {
                printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
                       actual_sample_rate, actual_bandwidth);
            }
            else
            {
                printf("Error: failed to read sample rate from card %u (status"
                        " code %" PRIi32 ")\n", card, status);
            }

            /* tune the Rx chain to the requested freq */
            status=skiq_write_rx_LO_freq(card,hdl[curr_rx_hdl],lo_freq);
            if (status < 0)
            {
                printf("Error: failed to set LO freq (using previous LO freq)...status is %d\n",
                       status);
            }

            /* add the handle to the list of configured handles */
            handles[nr_handles++] = curr_rx_hdl;

            /* initialize next_ts and total_blocks_acquired */
            next_ts[curr_rx_hdl] = 0llu;
            total_blocks_acquired[curr_rx_hdl] = 0;
        }
    }

    /************************** start Rx data flowing ***************************/

    printf("Info: starting %u Rx interface(s) on card %u\n", nr_handles, card);
    status = skiq_start_rx_streaming_multi_immediate(card, handles, nr_handles);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: starting %u Rx interface(s) on card %u failed with status %d\n", nr_handles, card, status);
        goto thread_exit;
    }

    /************************** receive data ***************************/

    /* loop through until there are no more receive handles needing data */
    while ( (num_hdl_rcv > 0) && (running==true) )
    {
        /* try to grab a packet of data */
        rx_status = skiq_receive(card, &curr_rx_hdl, &p_rx_block, &len);
        if ( skiq_rx_status_success == rx_status )
        {
            /* make sure the packet is from a handle that was enabled */
            if( hdl[curr_rx_hdl] == skiq_rx_hdl_end )
            {
                printf("Error: received unexpected data from hdl %u\n", curr_rx_hdl);
                ret_status = -3;
                goto thread_free;
            }
            /* determine the number of words read based on the number of bytes
               returned from the receive call less the header */
            num_payload_words_read[curr_rx_hdl] = (len/4) - SKIQ_RX_HEADER_SIZE_IN_WORDS;

#if CHECK_TIMESTAMPS
            curr_ts[curr_rx_hdl] = p_rx_block->rf_timestamp; /* peek at the timestamp */
            /* if this is the first block received then the next timestamp to expect
               needs to be initialized */
            if (first_block[curr_rx_hdl] == true )
            {
                first_block[curr_rx_hdl] = false;
                /* will be incremented properly below for next time through */
                next_ts[curr_rx_hdl] = curr_ts[curr_rx_hdl];
            }
            /* validate the timestamp */
            else if (curr_ts[curr_rx_hdl] != next_ts[curr_rx_hdl])
            {
                printf("Error: timestamp error in block %d for %u/%u...expected 0x%016" PRIx64 " but got 0x%016" PRIx64 "\n", 
                       total_blocks_acquired[curr_rx_hdl], card, curr_rx_hdl,
                       next_ts[curr_rx_hdl], curr_ts[curr_rx_hdl]);
                /* update the next timestamp expected based on the current timestamp
                   since there was just a gap in the data */
                next_ts[curr_rx_hdl] = curr_ts[curr_rx_hdl];
            }
#endif
            /* copy over the data if we haven't reached the end yet */
            if( total_blocks_acquired[curr_rx_hdl] < num_complete_blocks )
            {
                if ( include_meta )
                {
                    memcpy(rcv_buf[curr_rx_hdl], (uint8_t*)p_rx_block, num_bytes_per_pkt);
                }
                else
                {
                    memcpy(rcv_buf[curr_rx_hdl], (void *)p_rx_block->data, num_bytes_per_pkt);
                }

                /* increment our block count */
                total_blocks_acquired[curr_rx_hdl]++;

                /* either write to the file immediately or increment the buffer
                   where all the sample data is stored */
                if( (write_file_immediate == 1) )
                {
                    fwrite( rcv_buf[curr_rx_hdl], num_bytes_per_pkt, 1, output_fp[curr_rx_hdl] );
                }
                else
                {
                    rcv_buf[curr_rx_hdl] += num_bytes_per_pkt;
                }
            }
            else
            {
                /* if this is the first time we've reached the end for this handle
                   we need to decrement the number of handles we're trying to receive from */
                if( last_block[curr_rx_hdl] == false )
                {
                    num_hdl_rcv--;
                    last_block[curr_rx_hdl] = true;
                    /* we might need to write out a partial block to complete the processing */
                    if ( include_meta )
                    {
                        memcpy(rcv_buf[curr_rx_hdl], (uint8_t*)p_rx_block, last_block_num_bytes);
                    }
                    else
                    {
                        memcpy(rcv_buf[curr_rx_hdl], (void *)p_rx_block->data, last_block_num_bytes);
                    }

                    if( (write_file_immediate == 1) )
                    {
                        fwrite( rcv_buf[curr_rx_hdl], last_block_num_bytes, 1, output_fp[curr_rx_hdl] );
                    }
                }
            }

            /* update the next expected timestamp based on the number of words received */
            next_ts[curr_rx_hdl] += num_payload_words_read[curr_rx_hdl];
        }
    }

thread_free:
    /* all done, so stop streaming */
    printf("Info: stopping %u Rx interface(s) on card %u\n", nr_handles, card);
    skiq_stop_rx_streaming_multi_immediate(card, handles, nr_handles);

    /* actually save the file now if this wasn't been done while receiving */
    if( (write_file_immediate == 0) && (running==true) )
    {
        for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
        {
            if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
            {
                printf("Info: writing file for card %u, handle %u\n", card, curr_rx_hdl);
                /* reset the receive buffer to the beginning to process the data */
                rcv_buf[curr_rx_hdl] = p_start[curr_rx_hdl];
                for( len=0; len<num_complete_blocks; len++ )
                {
                    fwrite( rcv_buf[curr_rx_hdl], num_bytes_per_pkt, 1, output_fp[curr_rx_hdl] );
                    rcv_buf[curr_rx_hdl] += num_bytes_per_pkt;
                }
                /* write out the last partial packet if necessary */
                if( last_block_num_bytes > 0 )
                {
                    fwrite( rcv_buf[curr_rx_hdl], last_block_num_bytes, 1, output_fp[curr_rx_hdl] );
                }
            }
        }
    }

    /* make sure to free the receive buffers */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
        {
            free(p_start[curr_rx_hdl]);
        }
    }

thread_close:
    /* close all of the files */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
        {
            fclose( output_fp[curr_rx_hdl] );
        }
    }

    /* verify the data if a counter was used */
    if( (data_src == skiq_data_src_counter) && (running==true) )
    {
        for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
        {
            if( hdl[curr_rx_hdl] != skiq_rx_hdl_end )
            {
                output_fp[curr_rx_hdl]=fopen(p_filename,"rb");
                if (output_fp[curr_rx_hdl] == NULL)
                {
                    printf("Error: unable to open output file %s\n",p_filename);
                    ret_status = -1;
                }
                printf("Info: opened file %s for verification\n",p_filename);
                verify_data( output_fp[curr_rx_hdl] );
            }
        }
    }

thread_exit:
    thread_status[card] = ret_status;
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
    skiq_rx_hdl_t curr_rx_hdl;
    int32_t *card_status[SKIQ_MAX_NUM_CARDS];

    app_name = argv[0];

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize all the handles to be invalid */
    for( curr_rx_hdl=skiq_rx_hdl_A1; curr_rx_hdl<skiq_rx_hdl_end; curr_rx_hdl++ )
    {
        hdl[curr_rx_hdl] = skiq_rx_hdl_end;
    }

    skiq_get_cards( skiq_xport_type_pcie, &num_cards, cards );

    /* initialize everything based on the arguments provided */
    process_cmd_line_args(argc,argv);

    /* if we're storing the metadata then our packet size should add the number
       of bytes contained in the header */
    if ( include_meta )
    {
        num_bytes_per_pkt = SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES;
    }
    else
    {
        num_bytes_per_pkt = SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES - SKIQ_RX_HEADER_SIZE_IN_BYTES;
    }

    /* set up the # of blocks to acquire according to the cmd line args */
    num_complete_blocks = num_payload_words_to_acquire / NUM_PAYLOAD_WORDS_IN_BLOCK;
    last_block_num_bytes = ((num_payload_words_to_acquire % NUM_PAYLOAD_WORDS_IN_BLOCK)*4);
    if ( include_meta && (last_block_num_bytes != 0) )
    {
        /* add bytes for the metadata of the last block */
        last_block_num_bytes += SKIQ_RX_HEADER_SIZE_IN_BYTES;
    }
    printf("Info: num blocks to acquire is %d\n",num_complete_blocks);

    printf("Info: initializing %" PRIu8 "card(s)...\n", num_cards);

    /* bring up the PCIe interface for all the cards in the system */
    status = skiq_init(skiq_xport_type_pcie, skiq_xport_init_level_full, cards, num_cards);
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

    /* start a new thread for each card */
    for( i=0; i<num_cards; i++ )
    {
        skiq_write_chan_mode( cards[i], chan_mode );
        pthread_create( &(card_thread[i]), NULL, process_card, (void*)(&(cards[i])) );
    }

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

    skiq_exit();

    return (0);
}

/*****************************************************************************/
/** This function verifies that the received sample data is a monotonically
    increasing counter.

    @param pFile: a pointer to the file of data to verify
    @return: void
*/
static void verify_data(FILE *pFile)
{
    int16_t p_data[num_bytes_per_pkt/2];
    int16_t last_data;
    size_t samples_read;
    uint32_t offset=0;
    uint32_t header_offset=0;

    if( include_meta == 1 )
    {
        header_offset = (SKIQ_RX_HEADER_SIZE_IN_WORDS*2);
    }

    printf("Info: verifying data contents...");

    samples_read = fread(p_data, 2, num_bytes_per_pkt/2, pFile);
    last_data = p_data[header_offset]+1;
    while( samples_read > 0 )
    {
        for( offset=(header_offset+1); offset<samples_read; offset++ )
        {
            if( last_data != p_data[offset] )
            {
                printf("Error: at sample %u, expected 0x%x but got 0x%x\n",
                       offset, (uint16_t)last_data, (uint16_t)p_data[offset]);
                return;
            }
            last_data = p_data[offset]+1;
            /* we only have a 12-bit counter (sign extended), so we need to manually wrap it */
            if( last_data == 0x800 )
            {
                last_data = -2048;
            }
        }
        samples_read = fread(p_data, 2, num_bytes_per_pkt/2, pFile);
        last_data = p_data[header_offset]+1;
        /* we only have a 12-bit counter (sign extended), so we need to manually wrap it */
        if( last_data == 0x800 )
        {
            last_data = -2048;
        }
    }

    printf("done\n");
    printf("-------------------------\n");
}


/** This function extracts all cmd line args

    @param argc: the # of args to be processed
    @param argv: a pointer to the vector of arg strings
    @return: void
*/
static void process_cmd_line_args(int argc,char* argv[])
{
    if (argc != 13)
    {
        printf("Error: incorrect # of cmd line args\n");
        print_usage();
        exit(-1);
    }

    /* extract all args */
    num_payload_words_to_acquire = (uint32_t)strtoul(argv[2],NULL,10);
    printf("Info: acquiring %d words\n",num_payload_words_to_acquire);

    lo_freq = (uint64_t)strtoul(argv[3],NULL,10);
    printf("Info: requested Rx LO freq is %" PRIu64 " Hz\n",lo_freq);

    rx_gain = (uint32_t)strtoul(argv[4],NULL,10);
    printf("Info: requested Rx gain of %d db\n",rx_gain);

    sample_rate = (uint32_t)strtoul(argv[5],NULL,10);
    printf("Info: requested Rx sample rate is %d Hz\n",sample_rate);

    bandwidth = (uint32_t)strtoul(argv[6],NULL,10);
    printf("Info: requested Rx channel bandwidth is %d Hz\n",bandwidth);

    warp_voltage = (uint16_t)strtoul(argv[7],NULL,10);
    printf("Info: requested tcvcxo warp voltage %u\n",warp_voltage);

    /* determine data mode */
    if ((strcasecmp(argv[8],"iq")) == 0)
    {
        data_src = skiq_data_src_iq;
        printf("Info: using IQ data mode\n");
    }
    else if ((strcasecmp(argv[8],"counter")) == 0)
    {
        data_src = skiq_data_src_counter;
        printf("Info: using counter data mode\n");
    }
    else
    {
        printf("Error: invalid data type %s\n",argv[5]);
        print_usage();
        exit(-1);
    }

    /* check whether to include metadata in output file */
    write_file_immediate = atoi(argv[9]);
    if( (write_file_immediate) != 0 && (write_file_immediate != 1) )
    {
        printf("Error: invalid save to file while receiving option\n");
        print_usage();
        exit(-1);
    }

    /* check whether to include metadata in output file */
    include_meta = atoi(argv[10]);
    if( include_meta != 0 && include_meta != 1 )
    {
        printf("Error: invalid include metadata option\n");
        print_usage();
        exit(-1);
    }

    /* determine which handles are enabled */
    if ((strcasecmp(argv[11],"a") == 0))
    {
        if( strcasecmp(argv[12], "1") == 0 )
        {
            hdl[skiq_rx_hdl_A1] = skiq_rx_hdl_A1;
        }
        else if( strcasecmp(argv[12], "2") == 0 )
        {
            hdl[skiq_rx_hdl_A2] = skiq_rx_hdl_A2;
            // we must have dual chan mode to when receiving with A2
            chan_mode = skiq_chan_mode_dual;
        }
        else if( strcasecmp(argv[12], "all") == 0 )
        {
            hdl[skiq_rx_hdl_A1] = skiq_rx_hdl_A1;
            hdl[skiq_rx_hdl_A2] = skiq_rx_hdl_A2;
            chan_mode = skiq_chan_mode_dual;
        }
        else
        {
            printf("Error: invalid rx path (options are 1, 2 or both)\n");
            print_usage();
            exit(-1);
        }
    }
    else if ((strcasecmp(argv[11],"b") == 0))
    {
        if( strcasecmp(argv[12], "1") == 0 )
        {
            hdl[skiq_rx_hdl_B1] = skiq_rx_hdl_B1;
        }
        else
        {
            printf("Error: invalid rx path options (1 only for B)\n");
        }
    }
    else
    {
        printf("Error: invalid ad9361 chip id\n");
        print_usage();
        exit(-1);
    }
    printf("Info: Requested Catalina chip id %s\n",argv[10]);

    filename = argv[1];
}

/*****************************************************************************/
/** This function prints the main usage of the function.

    @param void
    @return void
*/
static void print_usage(void)
{
    printf("Usage: multicard_rx_samples <absolute path to output file> <# of words to acquire> \n");
    printf("       <Rx freq in Hz> <Rx gain index> <sample rate in Hz> <channel bandwidth in Hz>\n");
    printf("       <warp voltage in raw D/A count (0-1023 corresponding to 0.75-2.25V)> <iq | counter> \n");
    printf("       <save to file while receiving, 0|1> <store metadata, 0|1>  \n");
    printf("       <RF chip id, a> <Rx path within chip id 1|2|both>\n\n");

    printf("   Tune to the user-specifed Rx freq and acquire the specified # of words \n");
    printf("   at the requested sample rate at the requested Rx gain (using manual gain control\n");
    printf("   from the requested RFIC chip ('a') on all Sidekiq cards, storing the \n");
    printf("   output to the specified output file.\n\n");

    printf("   The data is stored in the file as 16-bit I/Q pairs with 'I' samples\n");
    printf("   stored in the the upper 16-bits of each word, and 'Q' samples stored\n");
    printf("   in the lower 16-bits of each word, resulting in the following format:\n");
    printf("           -31-------------------------------------------------------0-\n");
    printf("           |         12-bit I0           |       12-bit Q0            |\n");
    printf("    word 0 | (sign extended to 16 bits   | (sign extended to 16 bits) |\n");
    printf("           ------------------------------------------------------------\n");
    printf("           |         12-bit I1           |       12-bit Q1            |\n");
    printf("    word 1 | (sign extended to 16 bits   | (sign extended to 16 bits) |\n");
    printf("           ------------------------------------------------------------\n");
    printf("           |         12-bit I2           |       12-bit Q2            |\n");
    printf("    word 2 |  (sign extended to 16 bits  | (sign extended to 16 bits) |\n");
    printf("           ------------------------------------------------------------\n");
    printf("           |           ...               |          ...               |\n");
    printf("           ------------------------------------------------------------\n\n");

    printf("   Each I/Q sample is little-endian, twos-complement, signed, and sign-extended\n");
    printf("   from 12-bits to 16-bits.  Metadata is optionally removed from the sample data stored.\n");
    printf("   When metadata is included, it is located at the beginning of every 1018 IQ samples.\n");
    printf("   The metadata consists of 3 64-bit little endian values.  The first 64-bit value is \n");
    printf("   a timestamp that increments relative to the sample rate.  This timestamp is synchronized\n");
    printf("   within a chip.  The second 64-bits is a timestamp is a value that increments independent\n");
    printf("   of the sample rate and is consistent across the system.  The third 64-bits of metadata\n");
    printf("   represents the source of the samples, which is interpreted as follows: 0=RxA1, 1=RxA2\n");
    printf("   No additional meta-data is interleaved with the I/Q samples.\n\n");

    printf("Example: ./multicard_rx_samples /tmp/out 100000 850000000 50 10000000 10000000 512 iq 0 0 a 1\n");
}

