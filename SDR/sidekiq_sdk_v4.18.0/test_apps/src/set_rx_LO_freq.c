/*! \file set_rx_LO_freq.c
 * \brief This file contains a basic application for tuning the
 * Rx interface to the requested LO freq.  It accepts a start,
 * stop, and step frequency, and then performs the requested
 * tuning operation.  The duration of the tuning operation,
 * on a per-tuning basis, is then printed to standard out for
 * each tuned channel
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
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sidekiq_api.h>
#include <sys/time.h>

static char* app_name;

static void print_usage(void);
static int32_t run_timed_LO_tuning_loop(uint8_t card_id,
        uint64_t start_freq,
        uint64_t stop_freq,
        uint64_t step);

/*****************************************************************************/
/** This is the main function for executing set_rx_LO_freq commands.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    int32_t status;
    int result=0;
    uint64_t start_lo_freq,stop_lo_freq,step_size;
    uint8_t card=0;
    pid_t owner = 0;

    bool skiq_initialized = false;

    app_name = argv[0];

    if (argc != 5)
    {
        printf("Error: incorrect # of args\n");
        print_usage();
    }
    else
    {
        card = (uint8_t)(strtoul(argv[4],NULL,10));

        if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
        {
            printf("Error: card ID %" PRIu8 " exceeds the maximum card ID"
                    " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
            return (-1);
        }

        printf("Info: initializing card %" PRIu8 "...\n", card);

        status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full,
                    &card, 1);
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
                printf("Error: unable to initialize libsidekiq; was a valid"
                        " card specified? (result code %" PRIi32 ")\n", status);
            }
            else
            {
                printf("Error: unable to initialize libsidekiq with status %"
                        PRIi32 "\n", status);
            }
            return (-1);
        }
        skiq_initialized = true;

        start_lo_freq = (uint64_t)strtoul(argv[1],NULL,10);
        stop_lo_freq = (uint64_t)strtoul(argv[2],NULL,10);
        step_size = (uint64_t)strtoul(argv[3],NULL,10);

        printf("Info: starting LO freq is %" PRIu64 ", stopping LO freq is %"
                PRIu64 ", step size is %" PRIu64 " Hz\n",
               start_lo_freq, stop_lo_freq, step_size);

        status = run_timed_LO_tuning_loop(card, start_lo_freq, stop_lo_freq,
                    step_size);
        if (status < 0)
        {
            printf("Error: failed to run LO tuning loop...status is %" PRIi32
                    "\n", status);
            result = -1;
        }
    }

    if (skiq_initialized)
    {
        skiq_exit();
        skiq_initialized = false;
    }

    return(result);
}

/*****************************************************************************/
/** This function performs the actual LO tuning loop, logging the time at
    each tuning iteration.

    @param id the card id being tuned
    @param start_freq the starting LO freq
    @param stop_freq the ending LO freq
    @param step the frequency increment to step through
    @return void
*/
static int32_t run_timed_LO_tuning_loop(uint8_t card_id,
        uint64_t start_freq,
        uint64_t stop_freq,
        uint64_t step)
{
    int32_t status = 0;
    uint32_t num_steps = 0;
    struct timeval before_tune_time;
    struct timeval after_tune_time;
    struct timeval elapsed_time;
    uint64_t curr_freq;
    uint32_t hdl;

    hdl = skiq_tx_hdl_A1;
    curr_freq = start_freq;
    while (curr_freq < stop_freq)
    {
        /* grab the current time before and after the tuning operation */
        gettimeofday(&before_tune_time,NULL);
        status=skiq_write_rx_LO_freq(card_id,hdl,curr_freq);
        gettimeofday(&after_tune_time,NULL);

        if (0 != status)
        {
            printf("Error: failed to set RX LO frequency to %" PRIu64 " Hz"
                    " (result code %" PRIi32 ")\n", curr_freq, status);
        }

        /* calculate the elapsed time and print it out */
        elapsed_time.tv_sec = (after_tune_time.tv_sec - before_tune_time.tv_sec);
        if (after_tune_time.tv_usec >= before_tune_time.tv_usec)
        {
            elapsed_time.tv_usec = \
                after_tune_time.tv_usec - before_tune_time.tv_usec;
        }
        else
        {
            elapsed_time.tv_sec--;
            elapsed_time.tv_usec = \
                (after_tune_time.tv_usec + 1000000UL) - before_tune_time.tv_usec;
        }
        printf("Iteration %d: tuning to freq %" PRIu64 " Hz took %lu.%06lu"
                " seconds\n", num_steps, curr_freq, elapsed_time.tv_sec,
                elapsed_time.tv_usec);
        num_steps++;

        /* increment frequency to next step */
        curr_freq += step;
    }

    return(status);
}

/*****************************************************************************/
/** This function prints the main usage of the function.

    @param void
    @return void
*/
static void print_usage(void)
{
    printf("Usage: %s <start LO freq in Hz> <stop LO freq in Hz> <step size in Hz> <card>\n",app_name);
    printf("   Sweep the Rx LO frequency starting at the start LO freq, adding in step size in Hz\n");
    printf("   until the stop LO frequency is reached.  The tuning time for each tuning step is\n");
    printf("   reported along the way.\n");
}

