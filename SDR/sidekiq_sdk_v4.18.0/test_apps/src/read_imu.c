/**
 * @file   read_imu.c
 * @author <info@epiq-solutions.com>
 * @date   Mon Jan 28 12:30:19 2019
 * 
 * @brief This file contains a basic application that reads the accelerometer & gyroscope of the
 * specified Sidekiq.  Only supports ICM-20602 register set.  This is meant to be an example
 * applications, no claims are made for the accuracy of the ICM-20602's configuration.
 * 
 * <pre>
 * Copyright 2018 - 2019 Epiq Solutions, All Rights Reserved
 * </pre>
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "sidekiq_api.h"
#include "arg_parser.h"

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- obtain accelerometer and gyroscope measurements";
static const char* p_help_long =
"\
Reads the accelerometer and gyroscope for a specified Sidekiq.\
\n\
Defaults:\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n";

/* command line argument variables */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static int32_t repeat = 0;
static bool running = true;

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
    APP_ARG_OPT("repeat",
                0,
                "Read the sensors N additional times (-1 indicates forever, until interrupted)",
                "N",
                &repeat,
                INT32_VAR_TYPE),
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
    running = false;
}


/**************************************************************************************************/
/* This functions reads two 8-bit registers and returns the 16-bit two's complement result

   @param card_idx  which sidekiq card to read, already initialized
   @param reg   base register address (reads reg, reg+1)
   @param int *val    returns result
   @return int - indicated status of read

*/
int read_accel_reg_word( uint8_t card_idx, uint8_t reg, int16_t *val)
{
    int32_t status = 0;
    uint8_t low_byte = 0, high_byte = 0;
    int16_t result = 0;

    status = skiq_read_accel_reg( card_idx, reg, &high_byte, 1 );
    if ( status == 0 )
    {
        status = skiq_read_accel_reg( card_idx, reg+1, &low_byte, 1);
    }

    if ( status == 0 )
    {
        result = (((int16_t)high_byte)<<8) | low_byte;
        if (result >= 0x8000)
        {
            result = result - 0x10000;
        }
        *val = result;
    }
    else
    {
        fprintf(stderr, "Error: unable to read register 0x%02x (%" PRIi32 ")\n",
                reg, status);
    }

    return status;
}


/*************************************************************************************************/
/** This is the main function for executing read_imu application

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    int32_t status = 0;
    int32_t i = 0;
    pid_t owner = 0;
    bool skiq_initialized = false;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -1;
        goto finished;
    }

    if( (UINT8_MAX != card) && (NULL != p_serial) )
    {
        printf("Error: must specify EITHER card ID or serial number, not both\n");
        status = -1;
        goto finished;
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
            fprintf(stderr, "Error: cannot find card with serial number %s (result code %" PRIi32
                    ")\n", p_serial, status);
            status = -1;
            goto finished;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n", p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID (%" PRIu8 ")\n",
                card, (SKIQ_MAX_NUM_CARDS - 1));
        status = -1;
        goto finished;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* initialize Sidekiq */
    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_basic, &card, 1);
    if( status != 0 )
    {
        if ( ( EBUSY == status ) &&
             ( 0 != skiq_is_card_avail( card, &owner ) ) )
        {
            fprintf(stderr, "Error: card %" PRIu8 " is already in use (by process ID %u); cannot "
                    "initialize card.\n", card, (unsigned int) owner);
        }
        else if ( -EINVAL == status )
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq; was a valid card specified? "
                    "(result code %" PRIi32 ")\n", status);
        }
        else
        {
            fprintf(stderr, "Error: unable to initialize libsidekiq with status %d\n", status);
        }
        status = -1;
        goto finished;
    }
    skiq_initialized = true;

    for (i = 0; ((i <= repeat) || repeat == -1) && running; i++)
    {
        int16_t acc_x = 0, acc_y = 0, acc_z = 0, gyro_x = 0, gyro_y = 0, gyro_z = 0, temp = 0;
        uint8_t val = 0;

        /* set reg 0x6b to 0x01 to take card out of sleep/standby */
        val = 0x01;
        status = skiq_write_accel_reg( card, 0x6b, &val, 1);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to take IMU out of sleep / standby (result code %"
                    PRIi32 ")\n", status);
            goto finished;
        }

        if ( status == 0 ) status = read_accel_reg_word( card, 0x3b, &acc_x);
        if ( status == 0 ) status = read_accel_reg_word( card, 0x3d, &acc_y);
        if ( status == 0 ) status = read_accel_reg_word( card, 0x3f, &acc_z);
        if ( status == 0 ) status = read_accel_reg_word( card, 0x41, &temp);
        if ( status == 0 ) status = read_accel_reg_word( card, 0x43, &gyro_x);
        if ( status == 0 ) status = read_accel_reg_word( card, 0x45, &gyro_y);
        if ( status == 0 ) status = read_accel_reg_word( card, 0x47, &gyro_z);

        if ( status == 0)
        {
            temp = (int)((temp/327.8)+25.5);                
            printf("AX: %6d  AY: %6d  AZ: %6d  GX: %6d  GY: %6d  GZ:  %6d  Temp=%2d\n",
                   acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, temp);
        }
        else
        {
            fprintf(stderr, "Error: unable to read IMU measurement (result code %"
                    PRIi32 ")\n", status);
            goto finished;
        }

        usleep(100000);
    }

    status = 0;

finished:
    if (skiq_initialized)
    {
        skiq_exit();
        skiq_initialized = false;
    }

    return ((int) status);
}
