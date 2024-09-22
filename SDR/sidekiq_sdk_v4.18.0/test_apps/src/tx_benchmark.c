/*! \file tx_benchmark.c
 * \brief This file contains a basic application for benchmarking
 * the sending of packets to the DMA driver.
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
#include <errno.h>
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
static const char* p_help_short = "characterize transmit";
static const char* p_help_long = "\
Transmit data using the chosen transport layer, reporting back benchmark\n\
information collected during execution. Note that transmit will default to\n\
synchronous mode unless threads is specified to be greater than one.\n\
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
static uint8_t num_threads = 1;
static int32_t priority = -1;
static uint16_t pkt_size_in_words = 1020;
static char* p_temp_log_name = NULL;
static bool temp_log_is_set = false;

/* global variables shared amongst threads */
static pthread_t monitor_thread;
static bool running = true; /* TODO: we probably should have a mutex for this */
static uint64_t num_bytes = 0; // # bytes sent
static uint32_t num_bytes_in_pkt = 0;
static uint64_t num_pkts_complete = 0;
static uint64_t num_pkts = 0;
static uint32_t throughput = 0; // current MB/s
static uint32_t target = 0; // desired MB/s
static uint32_t run_time = 0;
static uint32_t underruns=0;
static uint32_t threshold=0; // max number of errors/underruns before failure
static FILE *p_temp_log=NULL;

static skiq_tx_transfer_mode_t transfer_mode = skiq_tx_transfer_mode_sync;
static uint32_t usleep_period;
static SKIQ_TX_BLOCK_INITIALIZER_BY_WORDS(buf, SKIQ_MAX_TX_BLOCK_SIZE_IN_WORDS);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t space_avail_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t space_avail_cond = PTHREAD_COND_INITIALIZER;

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
                "Number of underrun occurrences before considering test a failure",
                "NUMBER",
                &threshold,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("time",
                't',
                "Number of seconds to run benchmark",
                "SECONDS",
                &run_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("priority",
                0,
                "Thread priority of asynchronous TX threads",
                "p",
                &priority,
                INT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("temp-log",
                        0,
                        "File name to log temperature data",
                        "PATH",
                        &p_temp_log_name,
                        STRING_VAR_TYPE,
                        &temp_log_is_set),
    APP_ARG_TERMINATOR,
};

static void app_cleanup(int signum);

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
void app_cleanup(int signum)
{
    // ignore any other attempt to cleanup until we're done
    signal(SIGINT, SIG_IGN);
    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);
    running = false; // clears the running flag so that everything exits
}

/*****************************************************************************/
/** This is the callback function that is called when running in async mode
    when a packet has completed processing.

    @param status status of the transmit packet completed
    @param p_block pointer to the transmit block that has completed processing
    @param p_user pointer to user data (ignored)
    @return void
*/
void tx_complete_callback( int32_t status, skiq_tx_block_t *p_block, void *p_user )
{
    num_pkts_complete++;

    // signal to the other thread that there may be space available now that a
    // packet send has completed
    pthread_cond_signal(&space_avail_cond);
}

/*****************************************************************************/
/** This is a separate thread that monitors the performance of the DMA engine.

    @param none
    @return none
*/
void* monitor_performance(void *ptr)
{
    uint32_t last_underruns = 0;
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

            throughput = num_bytes / 1000000;

            printf("Send throughput: %3u MB/s (# underruns total %u,"
                " delta %u)\n", throughput, underruns,
                underruns - last_underruns);

            if( run_time > 0 )
            {
                running = (0 == --run_time) ? false : running;
            }

            num_bytes = 0;
            last_underruns = underruns;
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
/** This is the main function for executing the tx_benchmark app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    uint32_t i=0;
    uint32_t curr_count=0;
    int32_t status=0;
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
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        printf("Error: card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return (-1);
    }

    num_bytes_in_pkt = (pkt_size_in_words + SKIQ_TX_HEADER_SIZE_IN_WORDS)*4;
    /* determine the tx transfer mode */
    if( num_threads <= 1 )
    {
        transfer_mode = skiq_tx_transfer_mode_sync;
    }
    else
    {
        transfer_mode = skiq_tx_transfer_mode_async;
    }

    printf("Info: number of samples is %u (%u bytes)\n",
            pkt_size_in_words, num_bytes_in_pkt);

    /* initialize the packet of data, data is an array of 16-bit values */
    for (i = 0; i < 2 * pkt_size_in_words; i++)
    {
        buf.data[i] = curr_count;
        curr_count++;
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

    printf("Setting sample rate to %u\n", sample_rate);
    skiq_write_tx_sample_rate_and_bandwidth(card, skiq_tx_hdl_A1,
                                            sample_rate, sample_rate);

    // the sleep period should be a factor of the sample rate and pkt size
    usleep_period = (1.0/((double)(sample_rate)/1000000.0))*pkt_size_in_words;

    skiq_write_tx_data_flow_mode(card, skiq_tx_hdl_A1,
            skiq_tx_immediate_data_flow_mode);
    skiq_write_tx_block_size(card, skiq_tx_hdl_A1, pkt_size_in_words);
    skiq_write_tx_transfer_mode( card, skiq_tx_hdl_A1, transfer_mode );

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
        // update priority if it's the default value
        if( priority != -1 )
        {
            printf("Info: setting priority to %d\n", priority);
            if( skiq_write_tx_thread_priority(card, priority) != 0 )
            {
                printf("Error: unable to configure TX priority\n");
                skiq_exit();
                if( p_temp_log != NULL )
                {
                    fclose( p_temp_log );
                }
                _exit(-1);
            }
        }
    }

    if( skiq_start_tx_streaming(card, skiq_tx_hdl_A1) != 0 )
    {
        printf("Error: unable to start streaming\n");
        skiq_exit();
        if( p_temp_log != NULL )
        {
            fclose( p_temp_log );
        }
        _exit(-1);
    }

    /* initialize a thread to monitor our performance */
    pthread_create( &monitor_thread, NULL, monitor_performance, NULL );

    /* run forever and ever and ever (or until Ctrl-C) */
    while( running==true )
    {
        /* send the data */
        status=skiq_transmit(card, skiq_tx_hdl_A1, (skiq_tx_block_t *)&buf,
                    NULL);
        if( status != 0 )
        {
            // if we're running async and we got a queue full indication,
            // we should wait for a bit and then try to transmit again
            if( transfer_mode == skiq_tx_transfer_mode_async &&
                status == SKIQ_TX_ASYNC_SEND_QUEUE_FULL )
            {
                pthread_mutex_lock( &space_avail_mutex );
                pthread_cond_wait( &space_avail_cond, &space_avail_mutex );
                pthread_mutex_unlock( &space_avail_mutex );
            }
            else
            {
                printf("packet %" PRIu64 " sent failed with error %d\n",
                        num_pkts, status);
            }
        }
        else
        {
            pthread_mutex_lock(&lock);
            num_bytes += num_bytes_in_pkt;
            pthread_mutex_unlock(&lock);
            num_pkts++;
        }
    }
    printf("Sending complete\n");

    /* stop streaming and clean up the memory */
    printf("Cleaning up\n");
    skiq_stop_tx_streaming(card, skiq_tx_hdl_A1);

    /* wait for the monitor thread to complete */
    pthread_join(monitor_thread,NULL);

    if( p_temp_log != NULL )
    {
        fclose( p_temp_log );
    }

    skiq_exit();

    if( (throughput < target) || (underruns >= threshold) )
    {
        return (1);
    }

    return 0;
}
