/*! \file version_test.c
 * \brief This file contains a basic application that
 * reads and prints the various version strings for
 * libsidekiq and the FPGA.
 *
 * <pre>
 * Copyright 2011-2021 Epiq Solutions, All Rights Reserved
 *
 * </pre>
 */

#include "sidekiq_api.h"
#include "sidekiq_params.h"
#include <stdio.h>

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <arg_parser.h>
#include <sidekiq_xport_api.h>

#ifndef STR_MAX_LENGTH
#   define  STR_MAX_LENGTH     (4096)
#endif

#define DIVIDER_STR \
    "***********************************************************\n"

/* card operations / functions for the custom transport (implementation in my_custom_transport.c) */
extern skiq_xport_card_functions_t card_ops;

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- obtain version information";
static const char* p_help_long = "\
Scan the system for Sidekiq cards, displaying version information for one\n\
or all card(s) upon detection. By default, all detected Sidekiq cards will be\n\
initialized at a basic level to display minimal information. Optionally, a\n\
single Sidekiq card can be targeted and/or a full RF initialization performed\n\
in order to obtain further information related to the RF capabilities.";

/* command line argument variables */
static uint8_t single_card = SKIQ_MAX_NUM_CARDS;
static char* p_serial = NULL;
static bool do_full_init = false;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("card",
                'c',
                "Specify Sidekiq by card index",
                "ID",
                &single_card,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_OPT("full",
                0,
                "Perform full RF initialization",
                NULL,
                &do_full_init,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

static const char *rxHandles[] =
{ "RxA1", "RxA2", "RxB1", "RxB2", "RxC1", "RxD1", "Unknown" };
static uint32_t rxHandlesLen = sizeof(rxHandles) / sizeof(rxHandles[0]);

static const char *txHandles[] =
    { "TxA1", "TxA2", "TxB1", "TxB2", "Unknown" };
static uint32_t txHandlesLen = sizeof(txHandles) / sizeof(txHandles[0]);

static const char *
hdlToString(const char **handleList, uint32_t handlesLen, uint32_t hdl)
{
    if (hdl >= handlesLen)
    {
        /**
            @note   This assumes that the last entry is an "invalid" or
                    "unknown" entry.
        */
        return handleList[handlesLen - 1];
    }
    else
    {
        return handleList[hdl];
    }
}

static const char *
rxHdlToString(skiq_rx_hdl_t hdl)
{
    return hdlToString((const char **) rxHandles, rxHandlesLen, (uint32_t) hdl);
}

static const char *
txHdlToString(skiq_tx_hdl_t hdl)
{
    return hdlToString((const char **) txHandles, txHandlesLen, (uint32_t) hdl);
}


#ifdef __MINGW32__

static bool
getNameFromPid(pid_t pid, char *name, uint32_t nameSize)
{
    return false;
}

#else

/**
    @brief  Get the name of a program from its PID (process ID).

    @param[in]  pid         The PID of the program that is using the card.
    @param[out] name        The buffer to store the program name in; this should
                            not be NULL.
    @param[in]  nameSize    The size of name in bytes.

    @return false if name is NULL or the name couldn't be read, else true.
*/
static bool
getNameFromPid(pid_t pid, char *name, uint32_t nameSize)
{
    FILE *fp = NULL;
    char pathName[STR_MAX_LENGTH];
    ssize_t numRead = 0;

    if (NULL == name)
    {
        return false;
    }

    snprintf(pathName, STR_MAX_LENGTH, "/proc/%" PRIu64 "/cmdline",
            (uint64_t) pid);

    errno = 0;
    fp = fopen(pathName, "rb");
    if (NULL == fp)
    {
        printf("ERROR : failed to open PID %" PRIu64 " command file"
                " (%s): '%s' (%d)\n", (uint64_t) pid, pathName,
                strerror(errno), errno);
        return false;
    }

    /**
        @todo   Is a read loop needed here to ensure that all bytes are
                read?
    */

    errno = 0;
    numRead = fread((void *) name, 1, (size_t) nameSize, fp);
    if (0 != ferror(fp))
    {
        printf("ERROR : failed to read from PID %" PRIu64 " command file"
                " (%s): '%s'\n", (uint64_t) pid, pathName, strerror(errno));
        fclose(fp);
        fp = NULL;
        return false;
    }

    if (numRead == 0)
    {
        name[numRead] = '\0';
    }
    else
    {
        name[numRead - 1] = '\0';
    }

    fclose(fp);
    fp = NULL;

    return true;
}

#endif /* ifdef __MINGW32__ */


/* translate from enum to string */
static const char *
fifo_cstr( skiq_fpga_tx_fifo_size_t fifo )
{
    const char* p_fifo_str =
        (fifo == skiq_fpga_tx_fifo_size_4k) ? "4k samples" :
        (fifo == skiq_fpga_tx_fifo_size_8k) ? "8k samples" :
        (fifo == skiq_fpga_tx_fifo_size_16k) ? "16k samples" :
        (fifo == skiq_fpga_tx_fifo_size_32k) ? "32k samples" :
        (fifo == skiq_fpga_tx_fifo_size_64k) ? "64k samples" :
        "unknown";

    return p_fifo_str;
}

static const char *
rx_cal_data_cstr( uint8_t card, skiq_rx_hdl_t hdl, skiq_rf_port_t port )
{
    bool has_rx_cal_data;

    if ( 0 == skiq_read_rx_cal_data_present_for_port( card, hdl, port, &has_rx_cal_data ) )
    {
        return (has_rx_cal_data) ? "present" : "default";
    }
    else
    {
        return "unknown";
    }
}


/*****************************************************************************/
/** This is the main function for executing version_test command.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
    skiq_xport_init_level_t level = skiq_xport_init_level_basic;
    skiq_param_t param;
    const char *libsidekiq_label;
    const char *ref_clock_str;
    const char *xport_str;
    char program_name[STR_MAX_LENGTH];
    uint8_t all_cards[SKIQ_MAX_NUM_CARDS];
    uint8_t locked_cards[SKIQ_MAX_NUM_CARDS];
    uint8_t unlocked_cards[SKIQ_MAX_NUM_CARDS];
    uint8_t num_all_cards = 0;
    uint8_t num_locked_cards = 0;
    uint8_t num_unlocked_cards = 0;
    pid_t owner = 0;
    uint8_t libsidekiq_maj = 0;
    uint8_t libsidekiq_min = 0;
    uint8_t libsidekiq_patch = 0;
    int32_t status = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t k = 0;
    uint16_t cal_year=0;
    uint8_t cal_week=0;
    uint8_t cal_interval=0;
    skiq_rx_hdl_t hdl_conflicts[skiq_rx_hdl_end];
    uint8_t num_hdl_conflict;

    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    /* perform full init to obtain RF parameters */
    if( do_full_init )
    {
        level = skiq_xport_init_level_full;
    }

    /* register the custom transport's card functions */
    skiq_register_custom_transport( &card_ops );

    /* determine how many Sidekiqs are there and their card IDs */
    if ( NULL != p_serial)
    {
        status = skiq_get_card_from_serial_string(p_serial, &(all_cards[0]));
        if ( 0 != status )
        {
            printf("Error: cannot find card with serial number %s (result"
                    " code %" PRIi32 ")\n", p_serial, status);
        }
        else
        {
            num_all_cards = 1;

            if ( skiq_is_card_avail( all_cards[0], &owner) != 0 )
            {
                if (getNameFromPid(owner, (char *) program_name, STR_MAX_LENGTH))
                {
                    printf("Warning: card %u is currently locked by another"
                            " process ('%s', PID=%u)\n", all_cards[i],
                            program_name, (unsigned int) owner);
                }
                else
                {
                    printf("Warning: card %u is currently locked by another process"
                            " (PID=%u)\n", all_cards[i], (unsigned int)(owner));
                }
                locked_cards[num_locked_cards] = all_cards[0];
                num_locked_cards++;
            }
            else
            {
                unlocked_cards[num_unlocked_cards] = all_cards[0];
                num_unlocked_cards++;
            }
        }
    }
    else
    {
        skiq_get_cards( skiq_xport_type_auto, &num_all_cards, all_cards );
        for( i=0; i<num_all_cards; i++ )
        {
            if( skiq_is_card_avail(all_cards[i], &owner) != 0 )
            {
                if (getNameFromPid(owner, (char *) program_name, STR_MAX_LENGTH))
                {
                    printf("Warning: card %u is currently locked by another"
                            " process ('%s', PID=%u)\n", all_cards[i],
                            program_name, (unsigned int) owner);
                }
                else
                {
                    printf("Warning: card %u is currently locked by another process"
                            " (PID=%u)\n", all_cards[i], (unsigned int)(owner));
                }

                locked_cards[num_locked_cards] = all_cards[i];
                num_locked_cards++;
            }
            else
            {
                if( single_card == SKIQ_MAX_NUM_CARDS )
                {
                    unlocked_cards[num_unlocked_cards] = all_cards[i];
                    num_unlocked_cards++;
                }
                else if( all_cards[i] == single_card )
                {
                    unlocked_cards[num_unlocked_cards] = single_card;
                    num_unlocked_cards++;
                }
            }
        }
    }
    printf( "%d card(s) found: %d in use, %d available!\n",
            num_all_cards, num_locked_cards, num_unlocked_cards );
    printf("Card IDs currently used     : ");
    if (num_locked_cards > 0)
    {
        for (i = 0; i < num_locked_cards; i++)
        {
            printf("%d ", locked_cards[i]);
        }
    }
    printf("\n");
    printf("Card IDs currently available: ");
    if (num_unlocked_cards > 0)
    {
        for (i = 0; i < num_unlocked_cards; i++)
        {
            printf("%d ", unlocked_cards[i]);
        }
    }
    printf("\n");

    if ( 0 == num_unlocked_cards )
    {
        printf("No cards available!\n");
        return (-1);
    }

    printf("Info: initializing %" PRIu8 " card(s)...\n", num_unlocked_cards);

    /* bring up the interfaces for all the cards */
    status = skiq_init(skiq_xport_type_auto, level, unlocked_cards,
                num_unlocked_cards);
    if( status != 0 )
    {
        printf("Error: unable to initialize libsidekiq, status=%d\n", status);
        return (-1);
    }

    /* read the libsidekiq version */
    skiq_read_libsidekiq_version(&libsidekiq_maj,
                                 &libsidekiq_min,
                                 &libsidekiq_patch,
                                 &libsidekiq_label);

    printf(DIVIDER_STR
           "* libsidekiq v%d.%d.%d%s\n"
           DIVIDER_STR,
           libsidekiq_maj,
           libsidekiq_min,
           libsidekiq_patch,
           libsidekiq_label);

    /* read the version info for each for each of the cards */
    for( i = 0; i < num_unlocked_cards; i++ )
    {
        if( 0 != skiq_read_parameters(unlocked_cards[i], &param) )
        {
            printf("  Failed to obtain parameters for card %d.\n",
                    unlocked_cards[i]);
        }
        else
        {
            printf(DIVIDER_STR
                   "* Sidekiq Card %u\n",
                   param.card_param.card);

            /* string representation of xport type */
            switch( param.card_param.xport )
            {
            case( skiq_xport_type_pcie ):
                xport_str = "PCIe";
                break;
            case( skiq_xport_type_usb ):
                xport_str = "USB";
                break;
            case( skiq_xport_type_custom ):
                xport_str = "custom";
                break;
            case( skiq_xport_type_max ):
            case( skiq_xport_type_unknown ):
            default:
                xport_str = "unknown";
                break;
            }

            /* string representation of reference clock configuration */
            switch( param.rf_param.ref_clock_config )
            {
            case( skiq_ref_clock_external ):
                ref_clock_str = "external";
                break;
            case( skiq_ref_clock_carrier_edge ):
                ref_clock_str = "carrier edge";
                break;
            case( skiq_ref_clock_internal ):
                ref_clock_str = "internal";
                break;
            case( skiq_ref_clock_host ):
                ref_clock_str = "host";
                break;
            default:
                ref_clock_str = "unknown";
            }

            printf("  Card\n"
                   "\taccelerometer present: %s\n"
                   "\tpart type: %s\n"
                   "\tpart info: ES%s-%s-%s\n"
                    "\tserial: %s\n"
                   "\txport: %s\n",
                   (param.card_param.is_accelerometer_present) ? "true" : "false",
                   skiq_part_string(param.card_param.part_type),
                   param.card_param.part_info.number_string,
                   param.card_param.part_info.revision_string,
                   param.card_param.part_info.variant_string,
                   param.card_param.serial_string,
                   xport_str);

            printf("  FPGA\n"
                   "\tversion: %u.%u.%u\n"
                   "\tgit hash: 0x%08x\n"
                   "\tbuild date (yymmddhh): %08x\n"
                   "\ttx fifo size: %s\n",
                   param.fpga_param.version_major,
                   param.fpga_param.version_minor,
                   param.fpga_param.version_patch,
                   param.fpga_param.git_hash,
                   param.fpga_param.build_date,
                   fifo_cstr(param.fpga_param.tx_fifo_size));
            // System timestamp may not be valid if full initialization not performed
            if( do_full_init )
            {
                printf( "\tsystem timestamp frequency: %" PRIu64 " Hz\n",
                        param.fpga_param.sys_timestamp_freq );
            }

            if( true == param.fw_param.is_present )
            {
                printf("  FW\n"
                       "\tversion: %d.%d\n",
                       param.fw_param.version_major,
                       param.fw_param.version_minor);
                if( 0 != param.fw_param.enumeration_delay_ms )
                {
                    printf("\tenumeration delay: %d ms\n",
                           param.fw_param.enumeration_delay_ms);
                }
            }

            if( !do_full_init )
            {
                printf("  RF\n"
                       "\treference clock: %s\n"
                       "\treference clock frequency: %" PRIu32 " Hz\n"
                       "\treference clock warp value range: %" PRIu16 " - %" PRIu16 "\n"
                       "\treference clock warp resolution: %0.3f ppb/value\n",
                       ref_clock_str,
                       param.rf_param.ref_clock_freq,
                       param.rf_param.warp_value_min,
                       param.rf_param.warp_value_max,
                       param.rf_param.warp_value_unit);
                if( skiq_read_calibration_date(unlocked_cards[i],
                                               &cal_year,
                                               &cal_week,
                                               &cal_interval) == 0 )
                {
                    printf("\tlast calibration year: %u\n"
                           "\tlast calibration week number: %u\n"
                           "\trecalibration interval: %u years\n",
                           cal_year, cal_week, cal_interval);
                }
            }
            else
            {
                /* display information only available when fully initialize */
                printf("  RF\n"
                       "\treference clock: %s\n"
                       "\treference clock frequency: %" PRIu32 " Hz\n"
                       "\treference clock warp value range: %" PRIu16 " - %" PRIu16 "\n"
                       "\treference clock warp resolution: %0.3f ppb/value\n"
                       "\tfixed port: %s\n"
                       "\ttdd port: %s\n"
                       "\trx channels: %d\n"
                       "\ttx channels: %d\n",
                       ref_clock_str,
                       param.rf_param.ref_clock_freq,
                       param.rf_param.warp_value_min,
                       param.rf_param.warp_value_max,
                       param.rf_param.warp_value_unit,
                       (param.rf_param.is_rf_port_fixed) ? "true" : "false",
                       (param.rf_param.is_rf_port_trx_supported) ? "true" : "false",
                       param.rf_param.num_rx_channels,
                       param.rf_param.num_tx_channels);
                if( skiq_read_calibration_date(unlocked_cards[i],
                                               &cal_year,
                                               &cal_week,
                                               &cal_interval) == 0 )
                {
                    printf("\tlast calibration year: %u\n"
                           "\tlast calibration week number: %u\n"
                           "\trecalibration interval: %u years\n",
                           cal_year, cal_week, cal_interval);
                }


                for( j = 0; j < param.rf_param.num_rx_channels; j++ )
                {
                    printf("  RX[%d]: %s\n"
                           "\tLO tuning: %" PRIu64 " Hz - %" PRIu64 " Hz\n"
                           "\tsample rate: %" PRIu32 " Hz - %" PRIu32 " Hz\n",
                           j, rxHdlToString(j),
                           param.rx_param[j].lo_freq_min,
                           param.rx_param[j].lo_freq_max,
                           param.rx_param[j].sample_rate_min,
                           param.rx_param[j].sample_rate_max);
                    printf("\tfilters: %d\n", param.rx_param[j].num_filters);
                    for( k = 0; k < param.rx_param[j].num_filters; k++ )
                    {
                        printf("\t\t- %s\n",
                               SKIQ_FILT_STRINGS[param.rx_param[j].filters[k]]);
                    }
                    if( param.rx_param[j].num_fixed_rf_ports > 0 )
                    {
                        printf("\tRX Fixed RF ports: %d\n", param.rx_param[j].num_fixed_rf_ports);
                        for( k = 0; k < param.rx_param[j].num_fixed_rf_ports; k++ )
                        {
                            printf("\t\t- %s",
                                   skiq_rf_port_string(param.rx_param[j].fixed_rf_ports[k]));
                            printf(" (cal data: %s)\n",
                                   rx_cal_data_cstr(unlocked_cards[i],
                                                    j,
                                                    param.rx_param[j].fixed_rf_ports[k]));
                        }
                    }
                    if( param.rx_param[j].num_trx_rf_ports > 0 )
                    {
                        printf("\tRX TRX ports: %d\n", param.rx_param[j].num_trx_rf_ports);
                        for( k=0; k<param.rx_param[j].num_trx_rf_ports; k++ )
                        {
                            printf("\t\t - %s",
                                   skiq_rf_port_string(param.rx_param[j].trx_rf_ports[k]));
                            printf(" (cal data: %s)\n",
                                   rx_cal_data_cstr(unlocked_cards[i],
                                                    j,
                                                    param.rx_param[j].trx_rf_ports[k]));
                        }
                    }
                    if( skiq_read_rx_stream_handle_conflict( param.card_param.card,
                                                             j,
                                                             hdl_conflicts,
                                                             &num_hdl_conflict ) == 0 )
                    {
                        if( num_hdl_conflict > 0 )
                        {
                            printf("\tConflicting handles: %" PRIu8 "\n", num_hdl_conflict);
                            for( k=0; k<num_hdl_conflict; k++ )
                            {
                                printf("\t\thandle[%d]: %s\n",
                                       hdl_conflicts[k],
                                       rxHdlToString(hdl_conflicts[k]));
                            }
                        }
                    }
                }

                for( j = 0; j < param.rf_param.num_tx_channels; j++ )
                {
                    printf("  Tx[%d]: %s\n"
                           "\tLO tuning: %" PRIu64 " Hz - %" PRIu64 " Hz\n"
                           "\tsample rate: %" PRIu32 " Hz - %" PRIu32 " Hz\n"
                           "\tfilters: %d\n",
                           j, txHdlToString(j),
                           param.tx_param[j].lo_freq_min,
                           param.tx_param[j].lo_freq_max,
                           param.tx_param[j].sample_rate_min,
                           param.tx_param[j].sample_rate_max,
                           param.tx_param[j].num_filters);
                    for( k = 0; k < param.tx_param[j].num_filters; k++ )
                    {
                        printf("\t\t- %s\n",
                               SKIQ_FILT_STRINGS[param.tx_param[j].filters[k]]);
                    }
                    if( param.tx_param[j].num_fixed_rf_ports > 0 )
                    {
                        printf("\tTX Fixed RF ports: %d\n", param.tx_param[j].num_fixed_rf_ports);
                        for( k = 0; k < param.tx_param[j].num_fixed_rf_ports; k++ )
                        {
                            printf("\t\t- %s\n",
                                   skiq_rf_port_string(param.tx_param[j].fixed_rf_ports[k]));
                        }
                    }
                    if( param.tx_param[j].num_trx_rf_ports > 0 )
                    {
                        printf("\tTX TRX RF ports: %d\n", param.tx_param[j].num_trx_rf_ports);
                        for( k = 0; k < param.tx_param[j].num_trx_rf_ports; k++ )
                        {
                            printf("\t\t- %s\n",
                                   skiq_rf_port_string(param.tx_param[j].trx_rf_ports[k]));
                        }
                    }
                }
            }
            printf("\n");
        }
    }

    skiq_exit();

    return status;

}

