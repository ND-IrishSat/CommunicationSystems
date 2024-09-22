/**
 * @file   tx_samples_from_FPGA_RAM.c
 * @date   Mon Jun 24 15:45:25 2019
 *
 * @brief This file contains a basic application for transmitting sample data using the Block RAM in
 * the FPGA that's available in the "user app" on certain Sidekiq products
 *
 * <pre>
 * Copyright 2019 Epiq Solutions, All Rights Reserved
 * </pre>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>

#if (defined __MINGW32__)
#  if (defined __MINGW64__)
#    define PRI_SIZET PRIu64
#  else
#    define PRI_SIZET PRIu32
#  endif
#else
#  define PRI_SIZET "zu"
#endif

#if (defined __MINGW32__)
#define INPUT_PATH_MAX                  260
#else
#include <linux/limits.h>
#define INPUT_PATH_MAX                  PATH_MAX
#endif

#include "sidekiq_api.h"
#include "arg_parser.h"

/* The user register addresses in the FPGA design for the TX RAM block and its size.  It is not
 * always available depending on whether or not the user built the design with the feature
 * enabled. */
#define MAX_TX_RAM_NUM_SAMPLES                    1024
#define MIN_TX_RAM_NUM_SAMPLES                    2
#define FPGA_USER_REG_TX_MEM_DATA                 0x8700
#define FPGA_USER_REG_TX_MEM_LOOP_SIZE            0x8704 /* bits 31:28 designate the TX memory FIFO
                                                          * (as defined in the FPGA reference
                                                          * design) */
#define FPGA_USER_REG_TX_MEM_LOOP_SIZE_FIFO_(_fifo) (((_fifo) & 0xf) << 28)
#define FPGA_USER_REG_TX_MEM_CTRL                 0x870C /* bit 0 designates the 'enable' bit */
#define FPGA_USER_REG_TX_MEM_CTRL_ENABLE          (1 << 0)

#define FPGA_DUAL_JESD_LANE_SAMPLE_RATE_THRESH    ((uint32_t)250e6)

#define TX_BLOCK_SIZE_SINGLE_CHAN                 (SKIQ_TX_PACKET_SIZE_INCREMENT_IN_WORDS - \
                                                   SKIQ_TX_HEADER_SIZE_IN_WORDS)
#define TX_BLOCK_SIZE_MULTI_CHAN                  ((2 * SKIQ_TX_PACKET_SIZE_INCREMENT_IN_WORDS - \
                                                    SKIQ_TX_HEADER_SIZE_IN_WORDS) / 2)


/** @brief A simple macro for comparing semantic FPGA versions (major, minor, patch) */
#define FPGA_VERSION(a,b,c)   (((a) << 16) + ((b) << 8) + (c))


/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER          0
#endif

#ifndef DEFAULT_LO_FREQ
#   define DEFAULT_LO_FREQ              850000000
#endif

#ifndef DEFAULT_SAMPLE_RATE
#   define DEFAULT_SAMPLE_RATE          122880000
#endif

#ifndef DEFAULT_ATTENUATION
#   define DEFAULT_ATTENUATION          100
#endif

#ifndef DEFAULT_DURATION
#   define DEFAULT_DURATION             5
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- transmit I/Q data";
static const char* p_help_long =
"\
Configure the Tx lineup according to the specified parameters and transmit\n\
the entire contents of a provided file. The file should contain 16-bit\n\
signed twos-complement little-endian I/Q samples formatted as follows:\n\
\n\
    <16-bit Q0> <16-bit I0> <16-bit Q1> <16-bit I1> ... etc\n\
\n\
Defaults:\n\
  --attenuation=" xstr(DEFAULT_ATTENUATION) "\n\
  --card=" xstr(DEFAULT_CARD_NUMBER) "\n\
  --frequency=" xstr(DEFAULT_LO_FREQ) "\n\
  --handle=A1\n\
  --rate=" xstr(DEFAULT_SAMPLE_RATE) "\n\
  --time=" xstr(DEFAULT_DURATION) "\n\
  --cal-mode=auto\n\
  --force-cal=false";

/* variables used for all command line arguments */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static uint16_t attenuation = DEFAULT_ATTENUATION;
static uint64_t lo_freq = DEFAULT_LO_FREQ;
static uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
static uint32_t bandwidth = 0;
static int32_t duration = DEFAULT_DURATION;
static char* p_file_path = NULL;
static char* p_file_prefix = NULL;
static char* p_hdl = "A1";

/* application variables  */
static FILE *input_fp[skiq_tx_hdl_end] = { [0 ... (skiq_tx_hdl_end-1)] = NULL };
static bool running = true;
static uint32_t sample_buffer[skiq_tx_hdl_end][MAX_TX_RAM_NUM_SAMPLES];
static char *p_cal_mode = "auto";
static skiq_tx_quadcal_mode_t cal_mode = skiq_tx_quadcal_mode_auto;
static bool force_cal = false;
static char* p_rfic_file_path = NULL;

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("attenuation",
                'a',
                "Output attenuation in quarter dB steps",
                "dB",
                &attenuation,
                UINT16_VAR_TYPE),
    APP_ARG_OPT("bandwidth",
                'b',
                "Bandwidth in Hertz",
                "Hz",
                &bandwidth,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("card",
                'c',
                "Specify Sidekiq by card index",
                "ID",
                &card,
                UINT8_VAR_TYPE),
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to transmit samples at in Hertz",
                "Hz",
                &lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("handle",
                0,
                "Tx handle to use, either A1 or A2",
                "Tx",
                &p_hdl,
                STRING_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("time",
                0,
                "Transmit the RAM contents for N seconds (or -1 to TX until interrupted)",
                "N",
                &duration,
                INT32_VAR_TYPE),
    APP_ARG_OPT("source",
                's',
                "Input file to source for I/Q data for ALL specified handles",
                "PATH",
                &p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("prefix",
                0,
                "Input file prefix to source for I/Q data for EACH specified handle",
                "PREFIX",
                &p_file_prefix,
                STRING_VAR_TYPE),
    APP_ARG_OPT("serial",
                'S',
                "Specify Sidekiq by serial number",
                "SERNUM",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_OPT("cal-mode",
                0,
                "Calibration mode, either auto or manual",
                NULL,
                &p_cal_mode,
                STRING_VAR_TYPE),
    APP_ARG_OPT("force-cal",
                0,
                "Force calibration to run",
                NULL,
                &force_cal,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("rfic-config",
                0,
                "Input filename of RFIC configuration",
                NULL,
                &p_rfic_file_path,
                STRING_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

static const char* p_file_suffix[skiq_tx_hdl_end] = {
    [skiq_tx_hdl_A1] = ".a1",
    [skiq_tx_hdl_A2] = ".a2",
    [skiq_tx_hdl_B1] = ".b1",
    [skiq_tx_hdl_B2] = ".b2",
};

/**************************************************************************************************/
/* local functions */
/**************************************************************************************************/


static int32_t
enable_stream_from_tx_ram_block( void )
{
    int32_t status = 0;
    uint32_t value;

    status = skiq_read_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_CTRL, &value );
    if ( status == 0 )
    {
        value |= FPGA_USER_REG_TX_MEM_CTRL_ENABLE;
        status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_CTRL, value );
    }

    return status;
}


static int32_t
disable_stream_from_tx_ram_block( void )
{
    int32_t status = 0;
    uint32_t value;

    status = skiq_read_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_CTRL, &value );
    if ( status == 0 )
    {
        value &= ~FPGA_USER_REG_TX_MEM_CTRL_ENABLE;
        status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_CTRL, value );
    }

    return status;
}


/* NOTE: The user_app in the FPGA implements the Transmit Memory differently depending on whether
 * one or two JESD lanes are being used.  This function handles the case where the sample rate is
 * less than or equal to 250Msps (single JESD lane) */
static int32_t
write_tx_ram_block( skiq_tx_hdl_t tx_hdl,
                    uint16_t nr_samples,
                    uint32_t samples[] )
{
    int32_t status = 0;
    uint8_t fifo_index = 0;

    /* Map the transmit handle to the TX memory index, this is highly dependent on the user_app
     * implementation */
    switch (tx_hdl)
    {
        case skiq_tx_hdl_A1:
            fifo_index = 0;
            break;

        case skiq_tx_hdl_A2:
            fifo_index = 1;
            break;

        case skiq_tx_hdl_B1:
            fifo_index = 2;
            break;

        case skiq_tx_hdl_B2:
            fifo_index = 3;
            break;

        default:
            status = -EINVAL;
            break;
    }

    if ( status == 0 )
    {
        uint32_t i, loop_size = (uint32_t)nr_samples - 1;

        /* specify with MACRO which TX FIFO to populate */
        loop_size |= FPGA_USER_REG_TX_MEM_LOOP_SIZE_FIFO_(fifo_index);

        status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_LOOP_SIZE, loop_size );
        for ( i = 0; ( i < nr_samples ) && ( status == 0 ); i++ )
        {
            status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_DATA, samples[i] );
        }
    }

    return status;
}


/* NOTE: The user_app in the FPGA implements the Transmit Memory differently depending on whether
 * one or two JESD lanes are being used.  This function handles the case where the sample rate is
 * strictly greater than 250Msps (dual JESD lanes) */
static int32_t
write_tx_ram_block_dual_lane( skiq_tx_hdl_t tx_hdl,
                              uint16_t nr_samples,
                              uint32_t samples[] )
{
    int32_t status = 0;
    uint8_t i_fifo_index = 0, q_fifo_index = 0;

    switch (tx_hdl)
    {
        case skiq_tx_hdl_A1:
            i_fifo_index = 0;
            q_fifo_index = 1;
            break;

        case skiq_tx_hdl_A2:
            i_fifo_index = 4;
            q_fifo_index = 5;
            break;

        case skiq_tx_hdl_B1:
            i_fifo_index = 2;
            q_fifo_index = 3;
            break;

        case skiq_tx_hdl_B2:
            i_fifo_index = 6;
            q_fifo_index = 7;
            break;

        default:
            status = -EINVAL;
            break;
    }

    if ( status == 0 )
    {
        uint32_t k, loop_size;

        /* first the I part of the sample */
        loop_size = ((uint32_t)nr_samples) / 2 - 1;
        loop_size |= FPGA_USER_REG_TX_MEM_LOOP_SIZE_FIFO_(i_fifo_index);

        status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_LOOP_SIZE, loop_size );
        for ( k = 0; ( k < nr_samples ) && ( status == 0 ); k += 2 )
        {
            uint32_t i_sample = 0;
            uint16_t i_0, i_1;

            /* take two 16-bit I parts and write them to the memory */
            i_0 = ((samples[k]   >> 16) & 0xFFFF);
            i_1 = ((samples[k+1] >> 16) & 0xFFFF);

            i_sample = (i_0 << 16) | i_1;
            status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_DATA, i_sample );
        }
    }

    if ( status == 0 )
    {
        uint32_t k, loop_size;

        /* then the Q part of the sample */
        loop_size = ((uint32_t)nr_samples) / 2 - 1;
        loop_size |= FPGA_USER_REG_TX_MEM_LOOP_SIZE_FIFO_(q_fifo_index);

        status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_LOOP_SIZE, loop_size );
        for ( k = 0; ( k < nr_samples ) && ( status == 0 ); k += 2 )
        {
            uint32_t q_sample = 0;
            uint16_t q_0, q_1;

            /* take two 16-bit Q parts and write them to the memory */
            q_0 = ((samples[k]   >> 0) & 0xFFFF);
            q_1 = ((samples[k+1] >> 0) & 0xFFFF);

            q_sample = (q_0 << 16) | q_1;
            status = skiq_write_user_fpga_reg( card, FPGA_USER_REG_TX_MEM_DATA, q_sample );
        }
    }

    return status;
}


/**************************************************************************************************/
/** This function reads the contents of the file into Sidekiq's TX RAM blocks in the FPGA user app
    for the specified transmit handle
 */
static int32_t
init_tx_buffer_by_hdl( skiq_tx_hdl_t handle,
                       FILE *tx_data_fp )
{
    int32_t status = 0;
    uint32_t num_bytes_in_file = 0, nr_samples;
    size_t nr_samples_read;

    /* determine how large the file is and how many samples are included */
    fseek(tx_data_fp, 0, SEEK_END);
    num_bytes_in_file = ftell(tx_data_fp);
    rewind(tx_data_fp);
    nr_samples = (num_bytes_in_file / sizeof(int32_t));

    /* if there isn't a multiple of full sample(s) (32 bits = 16-bit I, 16-bit Q), then complain and
     * set the status */
    if ( ( num_bytes_in_file % sizeof(int32_t) ) > 0 )
    {
        fprintf(stderr, "Error: number of bytes (%u) must be a multiple of 4 (a full I/Q sample)\n", num_bytes_in_file);
        status = -ERANGE;
    }

    if ( nr_samples > MAX_TX_RAM_NUM_SAMPLES )
    {
        fprintf(stderr, "Warning: number of samples (%u) requested exceeds maximum, capping at %u\n", nr_samples, MAX_TX_RAM_NUM_SAMPLES);
        nr_samples = MAX_TX_RAM_NUM_SAMPLES;
    }
    else if ( nr_samples < MIN_TX_RAM_NUM_SAMPLES )
    {
        fprintf(stderr, "Warning: number of samples (%u) requested does not meet minimum of %u\n", nr_samples, MIN_TX_RAM_NUM_SAMPLES);
        status = -EINVAL;
    }

    nr_samples_read = fread( sample_buffer[handle], sizeof(int32_t), nr_samples, tx_data_fp );
    if ( nr_samples_read == nr_samples )
    {
        /* write the samples to the transmit handle's RAM block based on the desired sample rate
         * (which determines single lane vs dual lane) */
        if ( sample_rate > FPGA_DUAL_JESD_LANE_SAMPLE_RATE_THRESH )
        {
            uint8_t major = 0, minor = 0, patch = 0;

            status = skiq_read_fpga_semantic_version( card, &major, &minor, &patch );
            if ( status == 0 )
            {
                if ( FPGA_VERSION(major, minor, patch) < FPGA_VERSION(3,12,1) )
                {
                    /* ability to transmit on dual JESD lanes was introduced in FPGA designs v3.12.1
                     * and later */
                    status = -ENOTSUP;
                }
            }

            if ( status == 0 )
            {
                status = write_tx_ram_block_dual_lane( handle, nr_samples_read, sample_buffer[handle] );
            }
        }
        else
        {
            status = write_tx_ram_block( handle, nr_samples_read, sample_buffer[handle] );
        }
    }
    else
    {
        fprintf(stderr, "Error: read fewer samples (%" PRI_SIZET ") from file than expected (%u)\n", nr_samples_read, nr_samples);
        status = -EIO;
    }

    return status;
}


/**************************************************************************************************/
static int32_t
init_tx_buffers( skiq_tx_hdl_t handles[],
                 uint8_t nr_handles,
                 FILE *fp[skiq_tx_hdl_end] )
{
    int32_t status = 0;
    skiq_tx_hdl_t hdl;
    uint8_t i;

    for ( i = 0; ( i < nr_handles ) && ( status == 0 ); i++ )
    {
        hdl = handles[i];
        status = init_tx_buffer_by_hdl( hdl, fp[hdl] );
    }

    return status;
}


static skiq_tx_hdl_t
str2hdl( const char *str )
{
    return ( 0 == strcasecmp( str, "A1" ) ) ? skiq_tx_hdl_A1 :
        ( 0 == strcasecmp( str, "A2" ) ) ? skiq_tx_hdl_A2 :
        ( 0 == strcasecmp( str, "B1" ) ) ? skiq_tx_hdl_B1 :
        ( 0 == strcasecmp( str, "B2" ) ) ? skiq_tx_hdl_B2 :
        skiq_tx_hdl_end;
}


static const char *
tx_hdl_cstr( skiq_tx_hdl_t hdl )
{
    return ( skiq_tx_hdl_A1 == hdl ) ? "A1" :
        ( skiq_tx_hdl_A2 == hdl ) ? "A2" :
        ( skiq_tx_hdl_B1 == hdl ) ? "B1" :
        ( skiq_tx_hdl_B2 == hdl ) ? "B2" :
        "unknown";
}


static int32_t
parse_hdl_list( char *handle_str,
                skiq_tx_hdl_t handles[],
                uint8_t *p_nr_handles,
                skiq_chan_mode_t *p_chan_mode )
{
#define TOKEN_LIST ",;:"
    bool handle_requested[skiq_tx_hdl_end];
    skiq_tx_hdl_t tx_hdl;
    char *token, *handle_str_dup;

    for ( tx_hdl = skiq_tx_hdl_A1; tx_hdl < skiq_tx_hdl_end; tx_hdl++ )
    {
        handle_requested[tx_hdl] = false;
    }

    handle_str_dup = strdup( handle_str );
    if ( handle_str_dup == NULL )
    {
        return -ENOMEM;
    }

    token = strtok( handle_str_dup, TOKEN_LIST );
    while ( token != NULL )
    {
        tx_hdl = str2hdl( token );
        if ( tx_hdl == skiq_tx_hdl_end )
        {
            free( handle_str_dup );
            return -EPROTO;
        }
        else
        {
            handle_requested[tx_hdl] = true;
        }

        token = strtok( NULL, TOKEN_LIST );
    }

    *p_nr_handles = 0;
    for ( tx_hdl = skiq_tx_hdl_A1; tx_hdl < skiq_tx_hdl_end; tx_hdl++ )
    {
        if ( handle_requested[tx_hdl] )
        {
            handles[(*p_nr_handles)++] = tx_hdl;
        }
    }

    /* set chan_mode based on whether one of the second handles in each pair is requested */
    if ( handle_requested[skiq_tx_hdl_A2] || handle_requested[skiq_tx_hdl_B2] )
    {
        *p_chan_mode = skiq_chan_mode_dual;
    }
    else
    {
        *p_chan_mode = skiq_chan_mode_single;
    }

    free( handle_str_dup );

    return 0;
#undef TOKEN_LIST
}


static int32_t
tx_streaming( skiq_tx_hdl_t handles[],
              uint8_t nr_handles,
              int32_t (*stream_function)( uint8_t card, skiq_tx_hdl_t hdl) )
{
    int32_t status = 0;
    bool handle_requested[skiq_tx_hdl_end];
    skiq_tx_hdl_t tx_hdl;
    uint8_t i;

    for ( tx_hdl = skiq_tx_hdl_A1; tx_hdl < skiq_tx_hdl_end; tx_hdl++ )
    {
        handle_requested[tx_hdl] = false;
    }

    for ( i = 0; i < nr_handles; i++ )
    {
        handle_requested[handles[i]] = true;
    }

    /* For each transmit handle pair A1/A2 and B1/B2, consider which ones are requested.  If the
     * second one is requested (A2 or B2), then just start/stop streaming on that handle (libsidekiq
     * start streaming on the other one automatically), otherwise just start/stop the first one (A1
     * or B1) if requested
     */

    if ( handle_requested[skiq_tx_hdl_A2] )
    {
        status = stream_function( card, skiq_tx_hdl_A2 );
    }
    else if ( handle_requested[skiq_tx_hdl_A1] )
    {
        status = stream_function( card, skiq_tx_hdl_A1 );
    }

    if ( status == 0 )
    {
        if ( handle_requested[skiq_tx_hdl_B2] )
        {
            status = stream_function( card, skiq_tx_hdl_B2 );
        }
        else if ( handle_requested[skiq_tx_hdl_B1] )
        {
            status = stream_function( card, skiq_tx_hdl_B1 );
        }
    }

    return status;
}


static int32_t
start_tx_streaming( skiq_tx_hdl_t handles[],
                    uint8_t nr_handles )
{
    return tx_streaming( handles, nr_handles, skiq_start_tx_streaming );
}


static int32_t
stop_tx_streaming( skiq_tx_hdl_t handles[],
                    uint8_t nr_handles )
{
    return tx_streaming( handles, nr_handles, skiq_stop_tx_streaming );
}


/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param signum: the signal number that occurred
    @return void
*/
static void
app_cleanup(int signum)
{
    printf("Info: received signal %d, cleaning up libsidekiq\n", signum);
    running=false;
}


/*****************************************************************************/
/** This is the main function for executing the tx_samples app.

    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ASCII string arguments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[])
{
    skiq_tx_hdl_t hdl, handles[skiq_tx_hdl_end];
    skiq_chan_mode_t chan_mode = skiq_chan_mode_single;
    uint32_t block_size_in_words;
    uint8_t i, nr_handles;
    int32_t status=0;
    uint32_t read_bandwidth;
    uint32_t actual_bandwidth;
    uint32_t read_sample_rate;
    double actual_sample_rate;
    uint64_t min_lo_freq, max_lo_freq;
    pid_t owner = 0;
    bool skiq_initialized = false;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /* initialize everything based on the arguments provided */
    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = -1;
        goto cleanup;
    }

    if ( ( NULL != p_file_path ) && ( NULL != p_file_prefix ) )
    {
        fprintf(stderr, "Error: must specify EITHER --source or --prefix, not both\n");
        status = -1;
        goto cleanup;
    }
    else if ( ( NULL == p_file_path ) && ( NULL == p_file_prefix ) )
    {
        fprintf(stderr, "Error: must specify ONE of --source or --prefix\n");
        status = -1;
        goto cleanup;
    }

    if( (UINT8_MAX != card) && (NULL != p_serial) )
    {
        fprintf(stderr, "Error: must specify EITHER card ID or serial number, not both\n");
        status = -1;
        goto cleanup;
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
            fprintf(stderr, "Error: cannot find card with serial number %s"
                    " (result code %" PRIi32 ")\n", p_serial, status);
            status = -1;
            goto cleanup;
        }

        printf("Info: found serial number %s as card ID %" PRIu8 "\n",
                p_serial, card);
    }

    /* map argument values to Sidekiq specific variable values */
    status = parse_hdl_list( p_hdl, handles, &nr_handles, &chan_mode );
    if ( status == 0 )
    {
        if ( nr_handles == 0 )
        {
            fprintf(stderr, "Error: invalid number of handles specified (must be greater than zero)\n");
            status = -1;
            goto cleanup;
        }
    }
    else
    {
        fprintf(stderr, "Error: invalid handle(s) specified\n");
        status = -1;
        goto cleanup;
    }

    if( 0 == strcasecmp(p_cal_mode, "auto") )
    {
        cal_mode = skiq_tx_quadcal_mode_auto;
    }
    else if( 0 == strcasecmp(p_cal_mode, "manual") )
    {
        cal_mode = skiq_tx_quadcal_mode_manual;
    }
    else
    {
        fprintf(stderr, "Error: invalid calibration mode\n");
        status = -1;
        goto cleanup;
    }

    if ( bandwidth == 0 )
    {
        bandwidth = (uint32_t)(sample_rate * 0.80);
    }

    if ( p_file_path != NULL )
    {
        FILE *common_fp;

        common_fp = fopen(p_file_path, "rb");
        if ( common_fp == NULL )
        {
            fprintf(stderr, "Error: unable to open input file %s\n", p_file_path);
            status = -1;
            goto cleanup;
        }
        else
        {
            /* init_tx_buffer_by_hdl() rewinds the FILE pointer, so there's no harm in replicating
             * it across the input_fp[] array */
            for ( i = 0; i < nr_handles; i++ )
            {
                hdl = handles[i];
                input_fp[hdl] = common_fp;
            }
        }
    }
    else if ( p_file_prefix != NULL )
    {
        for ( i = 0; i < nr_handles; i++ )
        {
            char filename[INPUT_PATH_MAX];

            hdl = handles[i];
            snprintf(filename, INPUT_PATH_MAX, "%s%s", p_file_prefix, p_file_suffix[hdl]);
            input_fp[hdl] = fopen(filename, "rb");
            if ( input_fp[hdl] == NULL )
            {
                fprintf(stderr, "Error: unable to open input file %s\n", filename);
                status = -1;
                goto cleanup;
            }
        }
    }
    else
    {
        fprintf(stderr, "Error: must specify at least --source or --prefix\n");
        status = -1;
        goto cleanup;
    }


    printf("Info: initializing card %" PRIu8 "...\n", card);

    status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, &card, 1);
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
        goto cleanup;
    }
    skiq_initialized = true;

    if ( p_rfic_file_path != NULL )
    {
        FILE *p_rfic_file = fopen(p_rfic_file_path, "r");
        if( p_rfic_file == NULL )
        {
            fprintf(stderr, "Error: unable to open specified RFIC configuration file %s (errno %d)\n",
                    p_rfic_file_path, errno);
            goto cleanup;
        }
        else
        {
            printf("Info: configuring RFIC with configuration from %s\n", p_rfic_file_path);
            status = skiq_prog_rfic_from_file(p_rfic_file, card);
        }
        if( status != 0 )
        {
            printf("Error: unable to program RFIC from file with error %d\n", status);
            fclose( p_rfic_file );
            goto cleanup;
        }
    }

    /* configure the calibration mode across the requested handles */
    for ( i = 0; i < nr_handles; i++ )
    {
        hdl = handles[i];
        status = skiq_write_tx_quadcal_mode( card, hdl, cal_mode );
        if ( 0 != status )
        {
            fprintf(stderr, "Error: unable to configure quadcal mode on handle %s with %" PRIi32 "\n",
                    tx_hdl_cstr( hdl ), status);
            goto cleanup;
        }
    }

    status = skiq_read_tx_LO_freq_range( card, &max_lo_freq, &min_lo_freq );
    if ( 0 == status )
    {
        printf("Info: tunable TX LO frequency range = %" PRIu64
                "Hz to %" PRIu64 "Hz\n", min_lo_freq, max_lo_freq);
    }
    else
    {
        fprintf(stderr, "Warning: failed to read TX LO frequency range (result code %" PRIi32 ")\n", status);
    }

    /* set the channel mode */
    status = skiq_write_chan_mode(card, chan_mode);
    if( status != 0 )
    {
        fprintf(stderr, "Error: unable to set channel mode (result code %" PRIi32 ")\n", status);
        goto cleanup;
    }

    // configure our Tx parameters
    if ( p_rfic_file_path == NULL )
    {
        /* set the transmit sample rate and bandwidth across the requested handles */
        for ( i = 0; i < nr_handles; i++ )
        {
            hdl = handles[i];
            status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl, sample_rate,
                                                             bandwidth);
            if ( status != 0 )
            {
                fprintf(stderr, "Warning: unable to configure Tx sample rate on handle %s (result code %" PRIi32 ")\n",
                        tx_hdl_cstr( hdl ), status);
                goto cleanup;
            }
        }
    }
    else
    {
        printf("Info: RFIC configuration provided, skipping sample rate / bandwidth configuration\n");
    }

    status = skiq_read_tx_sample_rate_and_bandwidth(card, handles[0],
                                                    &read_sample_rate, &actual_sample_rate,
                                                    &read_bandwidth, &actual_bandwidth);
    if ( status == 0 )
    {
        printf("Info: actual sample rate is %f, actual bandwidth is %u\n",
               actual_sample_rate, actual_bandwidth);
    }
    else
    {
        fprintf(stderr, "Warning: failed to read TX sample rate and bandwidth (result"
                " code %" PRIi32 ")\n", status);
    }

    for ( i = 0; i < nr_handles; i++ )
    {
        hdl = handles[i];
        status = skiq_write_tx_LO_freq(card, hdl, lo_freq);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx LO frequency on handle %s (result code %" PRIi32 ")\n",
                    tx_hdl_cstr( hdl ), status);
            goto cleanup;
        }
    }
    printf("Info: configured Tx LO freq to %" PRIu64 " Hz\n", lo_freq);

    /* set the transmit attenuation across the requested handles */
    for ( i = 0; i < nr_handles; i++ )
    {
        hdl = handles[i];
        status = skiq_write_tx_attenuation(card, hdl, attenuation);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx attenuation on handle %s (result code %" PRIi32 ")\n",
                    tx_hdl_cstr( hdl ), status);
            goto cleanup;
        }
    }
    printf("Info: actual attenuation is %.2f dB, requested attenuation is %u\n",
           ((float)attenuation / 4.0),
           attenuation);

    if( force_cal == true )
    {
        printf("Info: forcing calibration to run\n");

        /* force calibration across the requested handles */
        for ( i = 0; i < nr_handles; i++ )
        {
            hdl = handles[i];
            status = skiq_run_tx_quadcal( card, hdl );
            if( status != 0 )
            {
                printf("Error: calibration failed to run properly on handle %s (result code %" PRIi32 ")\n",
                       tx_hdl_cstr( hdl ), status);
                goto cleanup;
            }
        }
    }

    /* Even though samples are being transmitted with the RAM blocks and not over PCIe, the block
     * size still needs to be set to something appropriate for single channel mode vs dual channel
     * mode to appease libsidekiq */
    block_size_in_words = (chan_mode == skiq_chan_mode_single) ? TX_BLOCK_SIZE_SINGLE_CHAN : TX_BLOCK_SIZE_MULTI_CHAN;

    for ( i = 0; i < nr_handles; i++ )
    {
        hdl = handles[i];
        status = skiq_write_tx_block_size(card, hdl, block_size_in_words);
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to configure Tx block size on handle %s (result code %" PRIi32 ")\n",
                    tx_hdl_cstr( hdl ), status);
            goto cleanup;
        }
    }
    printf("Info: block size set to %u words\n", block_size_in_words);

    // initialize the transmit buffer
    status = init_tx_buffers( handles, nr_handles, input_fp );
    if ( 0 != status )
    {
        fprintf(stderr, "Error: initializing the transmit RAM failed\n");
        status = -1;
        goto cleanup;
    }

    status = enable_stream_from_tx_ram_block();
    if ( 0 != status )
    {
        fprintf(stderr, "Error: unable to enable streaming from transmit RAM block (result code %"
                PRIi32 ")\n", status);
        status = -1;
        goto cleanup;
    }

    status = start_tx_streaming( handles, nr_handles );
    if ( status != 0 )
    {
        fprintf(stderr, "Error: unable to start streaming (result code %" PRIi32 ")\n", status);
        goto cleanup;
    }
    printf("Info: successfully started streaming\n");

    // replay the file the # times specified
    while ( running && ( ( duration > 0 ) || ( duration == -1 ) ) )
    {
        if ( duration > 1 )
        {
            printf("Info: transmitting the RAM contents for %u more seconds\n", duration);
        }
        else if ( duration == 1 )
        {
            printf("Info: transmitting the RAM contents for 1 more second\n");
        }
        else
        {
            printf("Info: transmitting the RAM contents\n");
        }

        if ( duration > 0 )
        {
            duration--;
        }

        sleep(1);
    }
    printf("Info: transmit complete\n");

    status = stop_tx_streaming( handles, nr_handles );
    if ( 0 != status )
    {
        fprintf(stderr, "Error: unable to stop streaming (result code %" PRIi32 ")\n", status);
        status = -1;
        goto cleanup;
    }

    status = disable_stream_from_tx_ram_block();
    if ( 0 != status )
    {
        fprintf(stderr, "Error: unable to disable streaming from transmit RAM block (result code %"
                PRIi32 ")\n", status);
        status = -1;
        goto cleanup;
    }

cleanup:
    printf("Info: shutting down...\n");

    // disable streaming, cleanup and shutdown
    if (skiq_initialized)
    {
        skiq_exit();
        skiq_initialized = false;
    }

    for ( hdl = skiq_tx_hdl_A1; hdl < skiq_tx_hdl_end; hdl++ )
    {
        if (NULL != input_fp[hdl])
        {
            fclose(input_fp[hdl]);
            input_fp[hdl] = NULL;
        }
    }

    return status;
}
