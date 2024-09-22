/*! \file xcv_benchmark.c
 * \brief This file contains a basic application for benchmarking
 * the sending/receiving of packets to/from the DMA driver.
 *
 * <pre>
 * Copyright 2014 - 2018 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <inttypes.h>
#include <string.h>

#include <sidekiq_api.h>
#include <arg_parser.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- get Rx and Tx metrics";
static const char* p_help_long =
"\
Collects benchmark metrics of running Rx and Tx simultaneously. Note that\n\
transmit will default to synchronous mode unless threads is specified to be\n\
greater than one.\n\
\n\
Defaults:\n\
  --block-size=1020\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --rate=1000000\n\
  --threads=1";

/* command line argument variables */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static uint32_t sample_rate = 1000000;
static uint8_t num_threads = 0;
static uint16_t pkt_size_in_words = 1020;
static bool blocking_rx = false;
static char* p_temp_log_name = NULL;
static bool temp_log_is_set = false;

/* pthread related variables */
static pthread_t monitor_thread;
static pthread_t tx_thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* global variables shared amongst threads */
static bool running = true;
static uint32_t rx_throughput = 0; // current MB/s
static uint32_t tx_throughput = 0; // current MB/s
static uint32_t target = 0; // desired MB/s
static uint32_t run_time = 0;
static uint64_t num_rx_bytes=0;
static uint64_t num_tx_bytes=0;
static uint64_t ts_gaps=0; // # timestamp gaps
static uint32_t underruns=0;
static uint32_t threshold=0; // max number of errors/underruns before failure
static uint64_t num_bytes_in_tx_pkt=0;
static skiq_tx_transfer_mode_t transfer_mode = skiq_tx_transfer_mode_sync;

static uint64_t num_tx_pkts_complete = 0;
static uint64_t num_tx_pkts = 0;

static skiq_tx_block_t *p_tx_block = NULL;

static FILE *p_temp_log=NULL;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("block-size",
                0,
                "Number of samples to transmit per block",
                "N",
                &pkt_size_in_words,
                UINT16_VAR_TYPE),
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
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("target",
                0,
                "Desired data throughput in megabytes per second",
                "MBPS",
                &target,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("threads",
                0,
                "Transmit asynchronously using 'N' threads",
                "N",
                &num_threads,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("threshold",
                0,
                "Number of timestamp gaps or underrun occurrences before considering test a failure",
                "NUMBER",
                &threshold,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("time",
                't',
                "Number of seconds to run benchmark",
                "SECONDS",
                &run_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("blocking",
                0,
                "Perform blocking during skiq_receive call",
                NULL,
                &blocking_rx,
                BOOL_VAR_TYPE),
    APP_ARG_OPT_PRESENT("temp-log",
                        0,
                        "File name to log temperature data",
                        "PATH",
                        &p_temp_log_name,
                        STRING_VAR_TYPE,
                        &temp_log_is_set),    

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
    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);
    running = false; // clears the running flag so that everything exits
}

/*****************************************************************************/
/** This is the callback function that is called when running in async mode
    when a packet has completed processing.  There is no guarantee that the
    complete callback will be in the order that the data was sent, this
    function just increments the completion count and signals the main thread
    that there is space available to send more packets.

    @param status status of the transmit packet completed
    @param p_block reference to the completed transmit block
    @param p_user reference to the user data
    @return void
*/
void tx_complete_callback( int32_t status, skiq_tx_block_t *p_block, void *p_user )
{
    if( status != 0 )
    {
        printf("Error: packet %" PRIu64 " failed with status %d\n", num_tx_pkts_complete, status);
    }

    num_tx_pkts_complete++;
}

/*****************************************************************************/
/** This is a separate thread that monitors the performance of the DMA engine.

    @param none
    @return none
*/
void* monitor_performance(void *ptr)
{
    uint32_t last_underruns = 0;
    uint64_t last_ts_gaps = 0;
    uint64_t monitor_time=0;

    /* run until we get canceled */
    while( running )
    {
        sleep(1);

        /* if still running after sleeping, report statistics */
        if ( running )
        {
            skiq_read_tx_num_underruns(card, skiq_tx_hdl_A1, &underruns);

            pthread_mutex_lock(&lock);

            tx_throughput = num_tx_bytes / 1000000;
            rx_throughput = num_rx_bytes / 1000000;

            printf("   Send throughput: %3u MB/s (# underruns total %u, delta %u)\n",
                   tx_throughput, underruns, underruns - last_underruns);

            printf("Receive throughput: %3u MB/s (# timestamp gaps total %" PRIu64 ", delta %" PRIu64 ")\n",
                   rx_throughput, ts_gaps, ts_gaps - last_ts_gaps);

            num_rx_bytes=0;
            num_tx_bytes=0;
            last_underruns = underruns;
            last_ts_gaps = ts_gaps;

            if( run_time > 0 )
            {
                running = (0 == --run_time) ? false : running;
            }

            pthread_mutex_unlock(&lock);

            if( p_temp_log != NULL )
            {
                int32_t temp_status=0;
                int8_t temp=0;

                // append the header if the beginning of the file
                if( monitor_time == 0 )
                {
                    fprintf(p_temp_log, "Time(s),Temperature(C)\n");
                }
                monitor_time++;

                if( (temp_status=skiq_read_temp(card, &temp)) == 0 )
                {
                    printf("Current temperature: %d C\n", temp);
                    // write the temperature to the file
                    fprintf(p_temp_log, "%" PRIu64 ",%d\n", monitor_time, temp);
                }
                else
                {
                    printf("Unable to obtain temperature (status=%d)\n",
                           temp_status);
                }
            }
        }
    }

    return NULL;
}

/*****************************************************************************/
/** This is a separate thread that sends packets to the DMA engine.

    @param none
    @return none
*/
void* send_pkts(void *ptr)
{
    uint32_t i=0;
    uint32_t curr_count=0;
    int32_t status=0;
    uint64_t num_pkts=0;
    uint32_t usleep_period;

    /* allocate the memory for the transmit block by number of bytes */
    p_tx_block = skiq_tx_block_allocate_by_bytes( num_bytes_in_tx_pkt );
    if( p_tx_block == NULL )
    {
        printf("Error: unable to allocate a transmit block\n");
        return (NULL);
    }
    /* initialize the packet of data */
    for( i=0; i<pkt_size_in_words; i++ )
    {
        p_tx_block->data[i] = curr_count;
        curr_count++;
    }

    // the sleep period should be a factor of the sample rate and pkt size
    usleep_period = (1.0/((float)(sample_rate)/1000000.0))*num_bytes_in_tx_pkt;

    // initialize the interface
    skiq_write_tx_data_flow_mode(card, skiq_tx_hdl_A1, skiq_tx_immediate_data_flow_mode);
    skiq_write_tx_block_size(card, skiq_tx_hdl_A1, (num_bytes_in_tx_pkt-SKIQ_TX_HEADER_SIZE_IN_BYTES)/4);
    skiq_write_tx_transfer_mode( card, skiq_tx_hdl_A1, transfer_mode );
    if( skiq_start_tx_streaming(card, skiq_tx_hdl_A1) != 0 )
    {
        printf("Error: unable to start Tx streaming\n");
        skiq_exit();
        _exit(-1);
    }

    /* run forever and ever and ever (or until Ctrl-C) */
    while( running )
    {
        /* send the data */
        status=skiq_transmit(card, skiq_tx_hdl_A1, p_tx_block, NULL);
        if( status != 0 )
        {
            // if we're running async and we got a queue full indication,
            // we should wait for a bit and then try to transmit again
            if( (transfer_mode == skiq_tx_transfer_mode_async) &&
                (status == SKIQ_TX_ASYNC_SEND_QUEUE_FULL) )
            {
                usleep(usleep_period);
            }
            else
            {
                printf("packet %" PRIu64 " sent failed with error %d\n", num_pkts, status);
            }
        }
        else
        {
            pthread_mutex_lock(&lock);
            num_tx_bytes += num_bytes_in_tx_pkt;
            pthread_mutex_unlock(&lock);
            num_tx_pkts++;
        }
    }

    if( transfer_mode == skiq_tx_transfer_mode_async )
    {
        printf("Waiting for packets to complete transfer, num_pkts %" PRIu64 ", num_complete %" PRIu64 "\n",
               num_tx_pkts, num_tx_pkts_complete);
        while( num_tx_pkts_complete < num_tx_pkts )
        {
            usleep(usleep_period);
        }
    }
    printf("Packet send completed!\n");

    skiq_stop_tx_streaming(card, skiq_tx_hdl_A1);
    skiq_tx_block_free(p_tx_block);

    return (NULL);
}

int main( int argc, char *argv[] )
{
    skiq_rx_block_t* p_rx_block;
    uint32_t data_len;
    uint64_t curr_ts = 0;
    uint64_t next_ts = 0;
    bool first_block = true;
    skiq_rx_hdl_t rx_hdl=skiq_rx_hdl_A1;
    uint32_t usleep_period;
    skiq_rx_status_t rx_result;
    int32_t status = 0;
    pid_t owner = 0;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
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

    num_bytes_in_tx_pkt = (pkt_size_in_words+SKIQ_TX_HEADER_SIZE_IN_WORDS)*4;

    /* determine the tx transfer mode */
    if( num_threads <= 1 )
    {
        transfer_mode = skiq_tx_transfer_mode_sync;
    }
    else
    {
        transfer_mode = skiq_tx_transfer_mode_async;
    }

    /* open the temperature log if it's set */
    if( temp_log_is_set == true )
    {
        p_temp_log = fopen( p_temp_log_name, "w+" );
        if( p_temp_log == NULL )
        {
            fprintf(stderr, "Error: unable to open temperature log %s (errno=%d)\n", p_temp_log_name, errno);
            return (-1);
        }
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize the interface */
    status = skiq_init(skiq_xport_type_auto,
                       skiq_xport_init_level_full,
                       &card,
                       1);
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

        if( p_temp_log != NULL )
        {
            fclose( p_temp_log );
        }
        return (-1);
    }

    if ( blocking_rx )
    {
        /* set a modest rx timeout */
        status = skiq_set_rx_transfer_timeout( card, 10000 );
        if( status != 0 )
        {
            printf("Error: unable to set RX transfer timeout with status %d\n", status);
            skiq_exit();

            if( p_temp_log != NULL )
            {
                fclose( p_temp_log );
            }
            return (-1);
        }
    }

    // config the tx sample rate
    skiq_write_rx_sample_rate_and_bandwidth(card, skiq_rx_hdl_A1, sample_rate, sample_rate);
    /* specify the # of threads only if running in async mode */
    if( transfer_mode == skiq_tx_transfer_mode_async )
    {
        if( skiq_write_num_tx_threads(card, num_threads) != 0 )
        {
            printf("Error: unable to set # of tx threads\n");
            skiq_exit();
            if( p_temp_log != NULL )
            {
                fclose( p_temp_log );
            }
            _exit(-1);
        }
        if( skiq_register_tx_complete_callback(card, &tx_complete_callback) != 0 )
        {
            printf("Error: unable to register callback complete\n");
            skiq_exit();
            if( p_temp_log != NULL )
            {
                fclose( p_temp_log );
            }
            _exit(-1);
        }
    }

    // the sleep period should be a factor of the sample rate and pkt size
    usleep_period = (1.0/((double)(sample_rate)/1000000.0))*1024*100;

    /* initialize a thread to monitor our performance */
    pthread_create( &monitor_thread, NULL, monitor_performance, NULL );

    /* intialize a thread to continuously transmit */
    pthread_create( &tx_thread, NULL, send_pkts, NULL );

    //  start streaming
    skiq_start_rx_streaming(card, skiq_rx_hdl_A1);

    /* run forever and ever and ever (or until Ctrl-C) */
    while( running )
    {
        rx_result = skiq_receive(card, &rx_hdl, &p_rx_block, &data_len);
        if( rx_result == skiq_rx_status_success )
        {
            curr_ts = p_rx_block->rf_timestamp;
            if( first_block == false )
            {
                if( curr_ts != next_ts )
                {
                    pthread_mutex_lock(&lock);
                    ts_gaps++;
                    pthread_mutex_unlock(&lock);
                }
            }
            else
            {
                first_block = false;
            }
            pthread_mutex_lock(&lock);
            num_rx_bytes += data_len;
            pthread_mutex_unlock(&lock);
            next_ts = curr_ts + (data_len/4) - SKIQ_RX_HEADER_SIZE_IN_WORDS;
        }
        // no data, sleep for a bit
        else if( rx_result == skiq_rx_status_no_data )
        {
            usleep( usleep_period );
        }
    }

    /* stop receive streaming */
    skiq_stop_rx_streaming(card, skiq_rx_hdl_A1);

    /* wait for the tx and monitor threads to be complete */
    pthread_join(tx_thread, NULL);
    pthread_join(monitor_thread, NULL);

    skiq_exit();

    if( p_temp_log != NULL )
    {
        fclose( p_temp_log );
    }

    if( (rx_throughput < target) ||
        (tx_throughput < target) ||
        (ts_gaps >= threshold) ||
        (underruns >= threshold) )
    {
        return (1);
    }

    return (0);
}

