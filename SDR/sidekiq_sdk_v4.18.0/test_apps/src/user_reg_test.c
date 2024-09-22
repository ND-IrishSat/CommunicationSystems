/*! \file user_reg_test.c
 * \brief This file contains a basic application that reads and writes
 * the user-definable registers in the FPGA using libsidekiq.
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
#include <sidekiq_api.h>
#include <arg_parser.h>
#include <errno.h>
#include <inttypes.h>

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- access user registers in FPGA";
static const char* p_help_long =
"\
Read or write a user-defined register in the FPGA for the specified Sidekiq.\n\
Note, user accessible registers start at address " \
STR(SKIQ_START_USER_FPGA_REG_ADDR) " and end at " \
STR(SKIQ_END_USER_FPGA_REG_ADDR) "\n\
Defaults:\n\
  --card=" STR(DEFAULT_CARD_NUMBER) "\n";

/* Command Line Argument Variables */
/* sidekiq card number */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
/* user register address */
static uint32_t addr = 0;
/* value to write (UINT64_MAX for no write requested) */
static uint64_t write = UINT64_MAX;
/* boolean set to true to perform read */
static bool do_read = false;

/* the command line arguments available with this application */
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
    APP_ARG_REQ("address",
                'a',
                "User register address",
                "VALUE",
                &addr,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("write",
                'w',
                "Value to write to 32 bit user register",
                "VALUE",
                &write,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("read",
                'r',
                "Read the user register, after write if applicable",
                "VALUE",
                &do_read,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

/*****************************************************************************/
/** This is the main function for executing reg_test commands.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    uint32_t data = 0;
    int32_t status = 0;
    bool do_write = false;
    pid_t owner = 0;

    if( 0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    if( (SKIQ_START_USER_FPGA_REG_ADDR > addr) ||
        (SKIQ_END_USER_FPGA_REG_ADDR < addr) )
    {
        printf("Error: user register address requested is out of bounds\n");
    }

    if( (UINT64_MAX != write) && (UINT32_MAX < write) )
    {
        printf("Error: write value must be a 32 bit value\n");
        return -1;
    }
    else if( UINT64_MAX != write )
    {
        data = (uint32_t) write;
        do_write = true;
    }

    if( !do_read && !do_write )
    {
        printf("Error: no action specified\n");
        return -1;
    }

    if( (UINT8_MAX != card) && (NULL != p_serial) )
    {
        printf("Error: must specify EITHER card ID or serial number, not"
                " both\n");
        return -1;
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
            return -1;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    if ( (SKIQ_MAX_NUM_CARDS - 1) < card )
    {
        printf("Error : card ID %" PRIu8 " exceeds the maximum card ID"
                " (%" PRIu8 ")\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        return -1;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* if the RFIC is not initialized and an init level of 1 is used, accessing
     * sample_clk synchronized FPGA registers will fail. */
    status = skiq_init(skiq_xport_type_auto,
                       skiq_xport_init_level_basic,
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
        return (-1);
    }

    /* should be a write, but confirm */
    if( do_write )
    {
        if ((status = skiq_write_user_fpga_reg(card,addr,data)) != 0)
        {
            printf("Error: failed to read FPGA address 0x%08x, status is %d\n",
                   addr,
                   status);
        }
        else
        {
        printf("Info: wrote card=%u addr=0x%08x, data=0x%08x\n",
               card,
               addr,
               data);
        }
    }

    if( (0 == status) && (do_read) )
    {
        if ((status = skiq_read_user_fpga_reg(card,addr,&data)) != 0)
        {
            printf("Error: failed to read FPGA address 0x%08x, status is %d\n",
                   addr,
                   status);
        }
        else
        {
            printf("Info: read card=%u addr=0x%08x, data=0x%08x\n",
                   card,
                   addr,
                   data);
        }
    }

    /* cleanup */
    skiq_exit();

    return(status);
}

