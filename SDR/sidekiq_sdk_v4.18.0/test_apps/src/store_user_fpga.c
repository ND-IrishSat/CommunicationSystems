/*! \file store_user_fpga.c
 * \brief This file copies the provided bitstream into flash.
 *
 * <pre>
 * Copyright 2013 - 2020 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#if (!defined _GNU_SOURCE)
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <signal.h>
#include <libgen.h>             /* for basename() */

#include "sidekiq_api.h"
#include "arg_parser.h"

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s


#ifndef DEFAULT_CONFIG_SLOT
#   define DEFAULT_CONFIG_SLOT  0
#endif

#ifndef DEFAULT_CONFIG_SLOT_METADATA
#   define DEFAULT_CONFIG_SLOT_METADATA  0xFFFFFFFFFFFFFFFF
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- store user FPGA bitstream into flash";
static const char* p_help_long =
"\
Uses either PCIE or USB interface to store a user supplied bitstream into flash\n\
memory at the user bitstream slot. The FPGA can then optionally be reloaded from\n\
the flash interface.\n\
\n\
Defaults:\n\
    --config-slot=" xstr(DEFAULT_CONFIG_SLOT) "\n\
    --metadata=" xstr(DEFAULT_CONFIG_SLOT_METADATA) "\n\
";

/* Command Line Argument Variables */
/* reload the FPGA from flash after programming */
bool do_fpga_reload = false;
/* verify flash contents after writing */
bool do_verify = false;
/* file path to FPGA bitstream */
static char* p_file_path = NULL;
/* Sidekiq card index */
static uint8_t card = UINT8_MAX;
/* Sidekiq serial number */
static char* p_serial = NULL;
/* Allow libsidekiq log output */
static bool verbose = true;

/* flash config slot index */
static uint8_t config_slot = DEFAULT_CONFIG_SLOT;

/* metadata to associate with a specified flash config slot */
static uint64_t metadata = DEFAULT_CONFIG_SLOT_METADATA;

/* command line arguments available for use with this application */
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
    APP_ARG_REQ("source",
                's',
                "Bitstream file to source for writing to flash",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("reload",
                0,
                "Reload the FPGA with the updated contents in flash",
                NULL,
                &do_fpga_reload,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("verbose",
                'v',
                "Enable logging from libsidekiq to stdout",
                NULL,
                &verbose,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("verify",
                0,
                "Verify the flash memory after it is written",
                NULL,
                &do_verify,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("config-slot",
                0,
                "Store bitstream file in flash memory config slot N (defaults to " xstr(DEFAULT_CONFIG_SLOT) ")",
                "N",
                &config_slot,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("metadata",
                'm',
                "Associate metadata with flash memory config slot N (defaults to " xstr(DEFAULT_CONFIG_SLOT_METADATA) ")",
                "META",
                &metadata,
                UINT64_VAR_TYPE),
    APP_ARG_TERMINATOR,
};


#if (defined __MINGW32__)
/* Unfortunately Mingw does not support sigset_t, so define a fake type definition and stub out the
   related functions. */
typedef int sigset_t;
static inline int mask_signals(sigset_t *old_mask) { return 0; }
static inline int unmask_signals(sigset_t *old_mask) { return 0; }
static inline bool shutdown_signal_pending(void) { return false; }
#else
/**
    @brief  Mask (block) the SIGINT and SIGTERM signals during critical
            operations

    @param[out]  old_mask   The original signal mask, needed by
                            ::unmask_signals().

    @note   Pending signals will be raised when ::unmask_signals() is called.

    @return 0 on success, else an errno.
*/
static int
mask_signals(sigset_t *old_mask)
{
    int result = 0;
    sigset_t new_mask;

    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGINT);
    sigaddset(&new_mask, SIGTERM);

    errno = 0;
    result = pthread_sigmask(SIG_BLOCK, &new_mask, old_mask);

    return (0 == result) ? result : errno;
}

/**
    @brief  Unmask (unblock) the signals blocked by ::mask_signals().

    @param[in]  old_mask    The original signal mask, as generated by
                            ::mask_signals().

    @note   Pending signals will be processed after calling this function.

    @return 0 on success, else an errno.
*/
static int
unmask_signals(sigset_t *old_mask)
{
    int result = 0;

    errno = 0;
    result = pthread_sigmask(SIG_SETMASK, old_mask, NULL);

    return (0 == result) ? result : errno;
}

/**
    @brief  Check if a shutdown signal (SIGINT or SIGTERM) is pending (while
            masked).

    @return true if a signal is pending, else false.
*/
static bool
shutdown_signal_pending(void)
{
    int result = 0;
    sigset_t pending_mask;
    bool pending = false;

    errno = 0;
    result = sigpending(&pending_mask);
    if (0 == result)
    {
        pending = ((1 == sigismember(&pending_mask, SIGINT)) ||
                   (1 == sigismember(&pending_mask, SIGTERM)));
    }
    else
    {
        printf("Debug: sigpending() failed (%d: '%s')\n",
                errno, strerror(errno));
    }

    return pending;
}
#endif	/* __MINGW32__ */

/*****************************************************************************/
/** This is the main function for executing the store_default_fpga app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    skiq_xport_type_t type = skiq_xport_type_auto;
    skiq_xport_init_level_t level = skiq_xport_init_level_basic;
    int32_t status = 0;
    FILE *pFile = NULL;
    pid_t owner = 0;

    int result = 0;
    bool masked = false;
    sigset_t old_mask;

    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -1;
        goto finished;
    }

    if( !verbose )
    {
        /* disable messages */
        skiq_register_logging(NULL);
    }

    if ( ( UINT8_MAX == card ) && ( NULL == p_serial ) )
    {
        fprintf(stderr, "Error: one of --card or --serial MUST be specified\n");
        status = -1;
        goto finished;
    }
    else if ( ( UINT8_MAX != card ) && ( NULL != p_serial ) )
    {
        fprintf(stderr, "Error: either --card OR --serial must be specified,"
                " not both\n");
        status = -1;
        goto finished;
    }

    /* check for serial number */
    if ( NULL != p_serial )
    {
        status = skiq_get_card_from_serial_string( p_serial, &card );
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to find Sidekiq with serial number"
                    " %s\n", p_serial);
            status = -1;
            goto finished;
        }
    }

    if ((SKIQ_MAX_NUM_CARDS - 1) < card)
    {
        fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card"
                " ID (%" PRIu8 ").\n", card, (SKIQ_MAX_NUM_CARDS - 1));
        status = -1;
        goto finished;
    }

    /* open the bitstream */
    pFile = fopen( p_file_path, "rb" );
    if( pFile == NULL )
    {
        fprintf(stderr, "Error: unable to open file %s to read from (error code"
                " %d: '%s')\n", p_file_path, errno, strerror(errno));
        status = -1;
        goto finished;
    }

    /*
        Later in this application, the shutdown signals SIGINT and SIGTERM will
        be set to be ignored so that the FPGA can't be only partially
        programmed. Blocking of these signals must be done here as skiq_init()
        can spin up threads, any of which can receive & handle these signals;
        as threads inherit the signal mask of the parent on creation, blocking
        signals now prevents any created threads from receiving the shutdown
        signals.
    */
    result = mask_signals(&old_mask);
    if (0 != result)
    {
        printf("Warning: failed to block signals before storing image"
                " (%d: '%s')\n", result, strerror(result));
    }
    else
    {
        masked = true;
    }

    printf("Info: initializing card %" PRIu8 "...\n", card);

    /* Initialize libsidekiq using the specified card. */
    status = skiq_init(type, level, &card, 1);
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
        goto finished;
    }

    /*
        If the signals were successfully masked, check if one of them was
        received and, if so, shut down the program. This check allows the
        user to interrupt the program before doing work that shouldn't be
        interrupted.
    */
    if( ( masked ) && ( shutdown_signal_pending() ) )
    {
        printf("Info: got shutdown signal\n");
        goto finished;
    }

    printf("Info: card %" PRIu8 " initialized; programming...\n", card);
    printf("Info: Storing contents of '%s' to card %" PRIu8 " at config-slot %" PRIu8 " with "
           "metadata 0x%016" PRIX64 "\n", basename(p_file_path), card, config_slot, metadata);

    /* Store the contents of the file on the card's on-board flash memory. */
    status = skiq_save_fpga_config_to_flash_slot(card, config_slot, pFile, metadata);
    if ( 0 == status )
    {
        printf("Info: skiq_save_fpga_config_to_flash_slot() returned %" PRIi32 "\n", status);
    }

    if ( 0 != status )
    {
        fprintf(stderr, "Error: unable to save FPGA image to flash status %" PRIi32 "\n", status);
        status = -1;
        goto finished;
    }

    /*
        If the signals were successfully masked, unmask them so that the
        shutdown signals operate normally; this is done here to allow the user
        to interrupt the verify operation(s) below.
    */
    if( masked )
    {
        result = unmask_signals(&old_mask);
        if (0 == result)
        {
            masked = false;
        }
    }

    if( true == do_verify )
    {
        printf("Info: performing flash data verification\n");
        status = skiq_verify_fpga_config_in_flash_slot(card, config_slot, pFile, metadata);

        if ( 0 == status )
        {
            printf("Info: flash verification succeeded\n");
        }
        else
        {
            fprintf(stderr, "Error: flash verification failed\n");
            status = -1;
            goto finished;
        }
    }

    if( true == do_fpga_reload )
    {
        printf("Info: reloading FPGA from flash\n");
        status = skiq_prog_fpga_from_flash_slot(card, config_slot);

        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to program FPGA from flash on card %" PRIu8
                    " (status = %" PRIi32 ")\n", card, status);
            status = -1;
            goto finished;
        }
    }

finished:
    /*
        If the shutdown signals are still masked, restore them; this is done
        as clean up.
    */
    if (masked)
    {
        (void) unmask_signals(&old_mask);
        masked = false;
    }

    /* cleanup */
    skiq_exit();

    if (NULL != pFile)
    {
        fclose(pFile);
        pFile = NULL;
    }

    return (int) status;
}

