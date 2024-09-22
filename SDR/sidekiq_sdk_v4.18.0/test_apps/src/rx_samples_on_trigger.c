/**
 * @file rx_samples_on_trigger.c
 *
 * @brief
 * Tune to the user-specified Rx frequency (--frequency) and acquire the number of
 * I/Q sample words (--words) at the requested sample rate (--rate) on all specified
 * Sidekiq cards (--card), storing the output to the specified output file (--destination).
 * If no --card option is specified in the command line, assume ALL cards.
 * Each card starts streaming on the next trigger (--trigger-src)
 * 1PPS edge, synchronously across its handles, or immediately.
 *
 * This application is a batch data capture; storing the samples in RAM buffers until
 * specified number of sample words have been captured. After the capture is complete,
 * the samples are optionally verified and stored into files. Multiple cards and multiple handles
 * for each card is supported. The samples from each card/handle are stored in separate files.
 *
 * The code is multi-threaded; each card gets a thread plus main. Each thread is capable of
 * supporting multiple handles for its card (each card can have a different number of handles). Each
 * thread is responsible for opening and closing its own files. The main thread handles argument
 * parsing, card initialization, and thread management.
 *
 * The goal of encapsulation is to make it easy to use portions of the code if needed.
 * Steps have been taken to encapsulate data and functionality:
 * struct cmd_line_args is used by ((write) arg_parser(), (read) map_arguments_to_radio_config())
 * struct thread_params is used by ((write) main(), (read) receive_thread())
 * struct radio_config is used by ( (write) map_arguments_to_radio_config(),
 *                                  (read/write) configure_radio(),
 *                                  (read) receive_thread(),
 *                                  (read) dump_rconfig())
 * struct thread_variables is used by ((write/read) receive_thread())
 * struct rx_stats is used by ((write/read) receive_thread())
 *
 * <pre>
 * Copyright 2014-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */


/***** INCLUDES *****/
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>

#include <fcntl.h>
#include <pthread.h>

#if (defined __MINGW32__)
#define OUTPUT_PATH_MAX                     260
#else
#include <linux/limits.h>
#define OUTPUT_PATH_MAX                     PATH_MAX
#endif

#define NUM_USEC_IN_MS                      (1000)
#define TRANSFER_TIMEOUT                    (10000)

#include "sidekiq_api.h"
#include "arg_parser.h"

/***** DEFINES *****/

//#define VERBOSE // define to enable debug code
#ifdef VERBOSE
#define VERBOSE_DUMP_RCONFIG(p_rconfig)      dump_rconfig(p_rconfig);
#define VERBOSE_CHECK_POINTER(name, pointer) if( pointer == NULL )            \
                                             {                                \
                                                 fprintf(stderr, "ERROR: "    \
                                                         name                 \
                                                         " pointer is NULL\n");\
                                             }                                
#else
#define VERBOSE_DUMP_RCONFIG(p_rconfig)
#define VERBOSE_CHECK_POINTER(name,pointer)
#endif

#define ERROR_COMMAND_LINE                  -EINVAL
#define ERROR_LIBSIDEKIQ_NOT_INITIALIZED    -EINVAL
#define ERROR_POINTER_NOT_INITIALIZED       -EFAULT
#define ERROR_NO_MEMORY                     -ENOMEM
#define ERROR_UNEXPECTED_DATA_FROM_HANDLE   -EBADMSG
#define ERROR_BLOCK_SIZE                    -EPROTO
#define ERROR_TIMESTAMP                     -ERANGE
#define ERROR_OVERRUN_DETECTED              -ENOBUFS
#define ERROR_CARD_CONFIGURATION            -EPROTO

/* a simple pair of macros to round up integer division */
#define _ROUND_UP(_numerator, _denominator)    (_numerator + (_denominator - 1)) / _denominator
#define ROUND_UP(_numerator, _denominator)     (_ROUND_UP((_numerator),(_denominator)))

/* MACRO used to swap elements, used depending on the IQ order mode before verify_data() */
#define SWAP(a, b) do { typeof(a) t; t = a; a = b; b = t; } while(0)

// [0 ... size] = value is a GCC specific method of initializing an array
// https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html 
#define INIT_ARRAY(size, value)             { [0 ... ((size)-1)] = value }

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER              0
#endif

#ifndef DEFAULT_RX_FREQUENCY
#   define DEFAULT_RX_FREQUENCY             850000000
#endif

#ifndef DEFAULT_RX_HDL
#   define DEFAULT_RX_HDL                   "A1"
#endif

#ifndef DEFAULT_RX_RATE
#   define DEFAULT_RX_RATE                  7680000
#endif

#ifndef DEFAULT_RX_BW
#   define DEFAULT_RX_BW                    6000000
#endif

#ifndef DEFAULT_NUM_SAMPLES
#   define DEFAULT_NUM_SAMPLES              2000
#endif

#ifndef DEFAULT_TRIGGER_SRC
#   define DEFAULT_TRIGGER_SRC              "synced"
#endif

#ifndef DEFAULT_SETTLE_TIME_MS
#   define DEFAULT_SETTLE_TIME_MS           500
#endif

#ifndef DEFAULT_RX_GAIN
#   define DEFAULT_RX_GAIN                  UINT8_MAX
#endif

/* if the user specified a card index, use that, otherwise default to all cards */
#ifndef DEFAULT_CARDS
#   define DEFAULT_CARDS                    0 // If no --card option is present, use ALL cards
#endif

#ifndef DEFAULT_USE_COUNTER
#   define DEFAULT_USE_COUNTER              false
#endif

#ifndef DEFAULT_NUM_PAYLOAD_WORDS_IS_PRESENT
#   define DEFAULT_NUM_PAYLOAD_WORDS_IS_PRESENT    false
#endif

#ifndef DEFAULT_BLOCKING_RX                     
#   define DEFAULT_BLOCKING_RX              false
#endif

#ifndef DEFAULT_DISABLE_DC_CORR                 
#   define DEFAULT_DISABLE_DC_CORR          false
#endif

#ifndef DEFAULT_PERFORM_VERIFY                  
#   define DEFAULT_PERFORM_VERIFY           false
#endif

#ifndef DEFAULT_PACKED                          
#   define DEFAULT_PACKED                   false
#endif

#ifndef DEFAULT_INCLUDE_META                    
#   define DEFAULT_INCLUDE_META             false
#endif

#ifndef DEFAULT_IQ_ORDER
#   define DEFAULT_IQ_ORDER                 skiq_iq_order_qi
#endif

#if (DEFAULT_IQ_ORDER == skiq_iq_order_iq)
#   define DEFAULT_I_THEN_Q                 true
#else
#   define DEFAULT_I_THEN_Q                 false
#endif

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                             str(s)
#define str(s)                              #s

/* Delimiter used when parsing lists provided as input */
#define TOKEN_LIST                          ","


/***** TYPE DEFINITIONS *****/

/* Command line parameters are mapped to this structure.
   The radios are initialized from this structure.
   This is done because not all command line parameters map
   directly to radio parameters. Parameters that do map directly
   are here for consistency and future adaptions.
   The radio_config functions only access the radio_config structure.
 */
struct radio_config
{
    // handles should be referenced as follow -> handles[cards[card_index]][handle_index]
    // handle_index is 0 .. nr_handles[cards[card_index]]
    skiq_rx_hdl_t       handles[SKIQ_MAX_NUM_CARDS][skiq_rx_hdl_end]; 
    skiq_chan_mode_t    chan_mode[SKIQ_MAX_NUM_CARDS];  // skiq_write_chan_mode 
    skiq_trigger_src_t  trigger_src;                    // skiq_write_timestamp_reset_on_1pps
    skiq_1pps_source_t  pps_source;                     // skiq_write_1pps_source
    skiq_iq_order_t     iq_order_mode;                  // skiq_write_iq_order_mode
    uint64_t            lo_freq;                        // skiq_write_rx_LO_freq
    uint32_t            sample_rate;                    // skiq_write_rx_sample_rate_and_bandwidth
    uint32_t            bandwidth;                      // skiq_write_rx_sample_rate_and_bandwidth
    uint8_t             rx_gain;                        // skiq_write_rx_gain
    uint8_t             nr_handles[SKIQ_MAX_NUM_CARDS]; // nr_handles[cards[card_index]]
    uint8_t             num_cards;                      // skiq_get_cards
    uint8_t             cards[SKIQ_MAX_NUM_CARDS];      // skiq_get_cards
    bool                blocking_rx;                    // skiq_set_rx_transfer_timeout
    bool                all_chans;                      // set to indicate RX all handles
    bool                packed;                         // skiq_write_iq_pack_mode
    bool                use_counter;                    // skiq_write_rx_data_src
    bool                disable_dc_corr;                // skiq_write_rx_dc_offset_corr
    bool                skiq_initialized;               // skit_init
    bool                rx_gain_manual;                 // skiq_write_rx_gain_mode 
};

/* Unless otherwise noted, the radio config structure is initialized
   based on the command line data in the function 
   map_arguments_to_radio_config()
*/   
#define RADIO_CONFIG_INITIALIZER                          \
{                                                         \
    .handles            = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, INIT_ARRAY(skiq_rx_hdl_end,skiq_rx_hdl_end)), \
    .chan_mode          = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, skiq_chan_mode_single), /* configure_handles_for_all() */ \
    .nr_handles         = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, 0), \
    .cards              = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, SKIQ_MAX_NUM_CARDS), \
    .trigger_src        = skiq_trigger_src_1pps,          \
    .pps_source         = skiq_1pps_source_unavailable,   \
    .iq_order_mode      = DEFAULT_IQ_ORDER,               \
    .lo_freq            = DEFAULT_RX_FREQUENCY,           \
    .rx_gain            = DEFAULT_RX_GAIN,                \
    .sample_rate        = DEFAULT_RX_RATE,                \
    .bandwidth          = DEFAULT_RX_BW,                  \
    .num_cards          = 0,                              \
    .blocking_rx        = DEFAULT_BLOCKING_RX,            \
    .all_chans          = false,                          /* parse_hdl_list */ \
    .packed             = DEFAULT_PACKED,                 \
    .use_counter        = DEFAULT_USE_COUNTER,            \
    .disable_dc_corr    = DEFAULT_DISABLE_DC_CORR,        \
    .skiq_initialized   = false,                          \
    .rx_gain_manual     = false,                          \
}

struct rx_stats
{
    uint64_t            curr_rf_ts;         // current RF timestamp
    uint64_t            next_rf_ts;         // next RF timestamp
    uint64_t            first_rf_ts;        // first RF timestamp
    uint64_t            last_rf_ts;         // last RF timestamp
    uint64_t            first_sys_ts;       // first system timestamp
    uint64_t            last_sys_ts;        // last system timestamp
    uint32_t            nr_blocks_to_write; // the requested number of blocks to be received
    bool                first_block;        // indicates if the first block has been received
};

#define RX_STATS_INITIALIZER    \
{                               \
    .curr_rf_ts         = 0,    \
    .next_rf_ts         = 0,    \
    .first_rf_ts        = 0,    \
    .last_rf_ts         = 0,    \
    .first_sys_ts       = 0,    \
    .last_sys_ts        = 0,    \
    .nr_blocks_to_write = 0,    \
    .first_block        = true, \
}                               \

/* Parameters passed to threads
   Instantiated by main() and passed to the threads
*/
struct thread_params
{
    pthread_t                   receive_thread; // thread responsible for receiving data
    const struct radio_config*  p_rconfig;
    uint32_t                    num_payload_words_to_acquire;
    char*                       p_file_path;
    uint8_t                     card_index;     // Index into rconfig[cards]
    volatile bool               init_complete;  // flag indicating that the thread has completed init
    bool                        include_meta;
    bool                        perform_verify;
};

#define THREAD_PARAMS_INITIALIZER                             \
{                                                             \
    .receive_thread                 = 0,                      \
    .card_index                     = UINT8_MAX,              \
    .p_rconfig                      = NULL,                   \
    .init_complete                  = false,                  \
    .include_meta                   = DEFAULT_INCLUDE_META,   \
    .perform_verify                 = DEFAULT_PERFORM_VERIFY, \
    .num_payload_words_to_acquire   = 0,                      \
}                                                             \

/* Local variables for each thread
*/
struct thread_variables
{
    FILE*               output_fp;   
    uint32_t            total_num_payload_words_acquired;
    uint32_t            rx_block_cnt;
    uint32_t*           p_next_write;
    uint32_t*           p_rx_data;
    uint32_t*           p_rx_data_start;
    uint32_t            words_received;
    char *              p_file_path;
    bool                last_block;
};

#define THREAD_VARIABLES_INITIALIZER        \
{                                           \
    .output_fp = NULL,                      \
    .total_num_payload_words_acquired = 0,  \
    .rx_block_cnt = 0,                      \
    .p_next_write = NULL,                   \
    .p_rx_data_start = NULL,                \
    .last_block = false,                    \
    .p_file_path = NULL,                    \
    .words_received = 0,                    \
}                                           \

struct cmd_line_args
{
    char*               p_file_path;
    char*               p_hdl;
    char*               p_trigger_src;
    char*               p_pps_source;
    uint8_t             card_id;
    uint32_t            num_payload_words_to_acquire;
    uint32_t            settle_time;
    uint64_t            lo_freq;
    uint32_t            sample_rate;
    uint32_t            bandwidth;
    uint8_t             rx_gain;
    bool                use_counter;
    bool                num_payload_words_is_present;
    bool                blocking_rx;
    bool                disable_dc_corr;
    bool                perform_verify;
    bool                packed;
    bool                include_meta;
    bool                card_is_present;
    bool                i_then_q;
    bool                rx_gain_manual;
};

#define COMMAND_LINE_ARGS_INITIALIZER                                           \
{                                                                               \
    .p_file_path                     = NULL,                                    \
    .p_hdl                           = DEFAULT_RX_HDL,                          \
    .p_trigger_src                   = DEFAULT_TRIGGER_SRC,                     \
    .p_pps_source                    = NULL,                                    \
    .num_payload_words_to_acquire    = DEFAULT_NUM_SAMPLES,                     \
    .settle_time                     = DEFAULT_SETTLE_TIME_MS,                  \
    .lo_freq                         = DEFAULT_RX_FREQUENCY,                    \
    .sample_rate                     = DEFAULT_RX_RATE,                         \
    .bandwidth                       = DEFAULT_RX_BW,                           \
    .rx_gain                         = DEFAULT_RX_GAIN,                         \
    .rx_gain_manual                  = false,                                   \
    .card_id                         = DEFAULT_CARDS,                           \
    .card_is_present                 = false,                                   \
    .use_counter                     = DEFAULT_USE_COUNTER,                     \
    .num_payload_words_is_present    = DEFAULT_NUM_PAYLOAD_WORDS_IS_PRESENT,    \
    .blocking_rx                     = DEFAULT_BLOCKING_RX,                     \
    .disable_dc_corr                 = DEFAULT_DISABLE_DC_CORR,                 \
    .perform_verify                  = DEFAULT_PERFORM_VERIFY,                  \
    .packed                          = DEFAULT_PACKED,                          \
    .include_meta                    = DEFAULT_INCLUDE_META,                    \
    .i_then_q                        = DEFAULT_I_THEN_Q,                        \
}

/***** LOCAL FUNCTIONS *****/

// Helper Functions
static const char *hdl_cstr(                    skiq_rx_hdl_t hdl );

static skiq_rx_hdl_t str2hdl(                   const char *str );

static const char *pps_source_cstr(             skiq_1pps_source_t pps_source );

static const char *trigger_src_desc_cstr(       skiq_trigger_src_t src );

static const char *chan_mode_desc_cstr(         skiq_chan_mode_t mode );

static const char *iq_order_desc_cstr(          skiq_iq_order_t order );

void dump_rconfig(                              const struct radio_config *p_rconfig);

static void hex_dump(                           uint8_t* p_data, 
                                                uint32_t length);

static void print_block_contents(               skiq_rx_block_t* p_block,
                                                int32_t block_size_in_bytes );

static int16_t sign_extend(                     int16_t in );

static void unpack_data(                        int16_t *packed_data,
                                                int16_t *unpacked_data,
                                                uint32_t num_unpacked_samples,
                                                int32_t block_size_in_words,
                                                bool include_meta );

// Command line parsing
static int32_t parse_hdl_list(                  const char *handle_str,
                                                skiq_rx_hdl_t rx_handles[], //Assumed to be skiq_rx_hdl_end elements long 
                                                uint8_t *p_nr_handles,
                                                skiq_chan_mode_t *p_chan_mode );

// Verification 
static int32_t verify_data(                     uint8_t card, 
                                                int16_t* p_data, 
                                                uint32_t num_samps, 
                                                int32_t block_size_in_words, 
                                                bool include_meta,
                                                bool packed,
                                                skiq_iq_order_t iq_order,
                                                const char* p_hdl_str );

static uint64_t compare_timestamps(             uint64_t ts1, 
                                                uint64_t ts2 );

static bool cmp_timestamp_pair(                 uint8_t card,
                                                const char *ts_desc,
                                                uint64_t ts1,
                                                skiq_rx_hdl_t hdl1,
                                                uint64_t ts2,
                                                skiq_rx_hdl_t hdl2 );

static bool cmp_timestamp_pair_fuzzy(           uint8_t card,
                                                const char *ts_desc,
                                                int delta,
                                                uint64_t ts1,
                                                skiq_rx_hdl_t hdl1,
                                                uint64_t ts2,
                                                skiq_rx_hdl_t hdl2 );

static bool cmp_timestamps_by_hdl(              uint8_t card,
                                                uint32_t sample_rate,
                                                struct rx_stats rxs[],
                                                skiq_rx_hdl_t hdl1,
                                                skiq_rx_hdl_t hdl2,
                                                bool packed );

// Radio configuration
static int32_t map_arguments_to_radio_config(   const struct cmd_line_args *p_cmd_line_args, 
                                                struct radio_config *p_rconfig );

static int32_t get_all_handles(                 uint8_t card, 
                                                skiq_rx_hdl_t rx_handles[], //Assumed to be skiq_rx_hdl_end elements long 
                                                uint8_t *p_nr_handles,
                                                skiq_chan_mode_t *p_chan_mode );

static int32_t configure_radio(                 uint8_t card, 
                                                struct radio_config *p_rconfig );

// RX thread
static void update_rx_stats(                    struct rx_stats *p_rx_stats, 
                                                skiq_rx_block_t *p_rx_block );

static int32_t open_files(                      FILE **output_fp, 
                                                uint8_t card, 
                                                const char *handle_str,
                                                const char *p_file_path );

static void *receive_card(                      void *params );

static void app_cleanup(                        int signum );


/***** GLOBAL DATA *****/

static pthread_cond_t           g_sync_start = PTHREAD_COND_INITIALIZER;  // used to sync threads
static pthread_mutex_t          g_sync_lock = PTHREAD_MUTEX_INITIALIZER; // used to sync thread
static struct cmd_line_args     g_cmd_line_args = COMMAND_LINE_ARGS_INITIALIZER;
static struct thread_params     g_thread_parameters[SKIQ_MAX_NUM_CARDS] = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, THREAD_PARAMS_INITIALIZER);

/* running is written to true here and only here.
   Setting 'running' to false will cause the threads to close and the 
   application to terminate.
*/
static volatile bool g_running=true;


/*************************** COMMAND LINE VARIABLES ***************************/
/******************************************************************************/

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- capture RX data from multiple cards and/or handles starting on a specified trigger";
static const char* p_help_long = "\
   Tune to the user-specified Rx frequency (--frequency) and acquire the number of\n\
   I/Q sample words (--words) at the requested sample rate (--rate) on all specified\n\
   Sidekiq cards (--card), storing the output to the specified output file (--destination).\n\
   If no --card option is specified in the command line, assume ALL cards.\n\n\
   Each card starts streaming on the next trigger (--trigger-src)\n\
   1PPS edge, synchronously across its handles, or immediately.\n\n\
   The data is stored in the file as 16-bit I/Q pairs with 'I' samples\n\
   stored in the the lower 16-bits of each word, and 'Q' samples stored\n\
   in the upper 16-bits of each word, resulting in the following format:\n\
           -31-------------------------------------------------------0-\n\
           |         12-bit I0           |       12-bit Q0            |\n\
    word 0 | (sign extended to 16 bits   | (sign extended to 16 bits) |\n\
           ------------------------------------------------------------\n\
           |         12-bit I1           |       12-bit Q1            |\n\
    word 1 | (sign extended to 16 bits   | (sign extended to 16 bits) |\n\
           ------------------------------------------------------------\n\
           |         12-bit I2           |       12-bit Q2            |\n\
    word 2 |  (sign extended to 16 bits  | (sign extended to 16 bits) |\n\
           ------------------------------------------------------------\n\
           |           ...               |          ...               |\n\
           ------------------------------------------------------------\n\n\
   Each I/Q sample is little-endian, twos-complement, signed, and sign-extended\n\
   from 12-bits to 16-bits.\n\
\n\
Defaults:\n\
  --frequency="xstr(DEFAULT_RX_FREQUENCY) "\n\
  --handle="xstr(DEFAULT_RX_HDL) "\n\
  --rate="xstr(DEFAULT_RX_RATE) "\n\
  --trigger-src="xstr(DEFAULT_TRIGGER_SRC) "\n\
  --bandwidth="xstr(DEFAULT_RX_BW) "\n\
  --words="xstr(DEFAULT_NUM_SAMPLES) "\n\
";

/* the command line arguments available to this application */
static struct application_argument p_args[] =
{
    APP_ARG_OPT("bandwidth",
                'b',
                "Bandwidth in hertz",
                "Hz",
                &g_cmd_line_args.bandwidth,
                UINT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("card",
                'c',
                "Use specified Sidekiq card",
                "ID",
                &g_cmd_line_args.card_id,
                UINT8_VAR_TYPE,
                &g_cmd_line_args.card_is_present),
    APP_ARG_REQ("destination",
                'd',
                "Output file to store Rx data",
                "PATH",
                &g_cmd_line_args.p_file_path,
                STRING_VAR_TYPE),
    APP_ARG_OPT("frequency",
                'f',
                "Frequency to receive samples at in Hertz",
                "Hz",
                &g_cmd_line_args.lo_freq,
                UINT64_VAR_TYPE),
    APP_ARG_OPT("rate",
                'r',
                "Sample rate in Hertz",
                "Hz",
                &g_cmd_line_args.sample_rate,
                UINT32_VAR_TYPE),
    APP_ARG_OPT_PRESENT("gain",
                'g',
                "Manually configure the gain by index rather than using automatic",
                "index",
                 &g_cmd_line_args.rx_gain,
                 UINT8_VAR_TYPE,
                 &g_cmd_line_args.rx_gain_manual),     
    APP_ARG_OPT_PRESENT("words",
                'w',
                "Number of I/Q sample words to acquire",
                "N",
                &g_cmd_line_args.num_payload_words_to_acquire,
                UINT32_VAR_TYPE,
                &g_cmd_line_args.num_payload_words_is_present),
    APP_ARG_OPT("handle",
                0,
                "Rx handle to use",
                "[\"A1\",\"A2\",\"B1\",\"B2\",\"C1\",\"D1\",\"ALL\"]",
                &g_cmd_line_args.p_hdl,
                STRING_VAR_TYPE),
    APP_ARG_OPT("trigger-src",
                0,
                "Source of start streaming trigger",
                "[\"1pps\",\"immediate\",\"synced\"]",
                &g_cmd_line_args.p_trigger_src,
                STRING_VAR_TYPE),
    APP_ARG_OPT("pps-source",
                0,
                "The PPS input source (only valid when --trigger-src=1pps)",
                "[\"external\",\"host\"]",
                &g_cmd_line_args.p_pps_source,
                STRING_VAR_TYPE),
    APP_ARG_OPT("settle-time",
                0,
                "Minimum time to delay after configuring radio prior to receiving samples",
                "msec",
                &g_cmd_line_args.settle_time,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("counter",
                0,
                "Receive sequential counter data",
                "-used for testing",
                &g_cmd_line_args.use_counter,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("meta",
                0,
                "Save metadata with samples (increases output file size)",
                NULL,
                &g_cmd_line_args.include_meta,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("packed",
                0,
                "Use packed mode for I/Q samples",
                NULL,
                &g_cmd_line_args.packed,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("blocking",
                0,
                "Perform blocking during skiq_receive call",
                NULL,
                &g_cmd_line_args.blocking_rx,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("disable-dc",
                0,
                "Disable DC offset correction",
                NULL,
                &g_cmd_line_args.disable_dc_corr,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("perform-verify",
                0,
                "Perform timestamp pair verification, conflicts with --trigger-src=immediate",
                NULL,
                &g_cmd_line_args.perform_verify,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("sample-order-iq",
                0,
                "If set, store samples in 'I then Q' order otherwise samples are stored in"
                  " 'Q then I' order",
                NULL,
                &g_cmd_line_args.i_then_q,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,
};


/*************************** ARG PARSER FUNCTIONS *****************************/
/******************************************************************************/

/*****************************************************************************/
/** Convert string containing list delimited by TOKEN_LIST into skiq_rx_hdl_t 
    constant.

    @param[in]  *handle_str:   input string containing list of handles
    @param[out] *rx_handles:   array of skiq_rx_hdl_t and size skiq_rx_hdl_end 
    @param[out] *p_nr_handles: pointer to number of handles ( 0 for ALL )
    @param[out] *p_chan_mode:  pointer to skiq_chan_mode_t

    @return int32_t:           - ERROR_NO_MEMORY
                               - ERROR_COMMAND_LINE
                               - 0 success
*/
static int32_t
parse_hdl_list( const char *handle_str,
                skiq_rx_hdl_t rx_handles[], //Assumed to be skiq_rx_hdl_end elements long 
                uint8_t *p_nr_handles,
                skiq_chan_mode_t *p_chan_mode )
{
    bool handle_requested[skiq_rx_hdl_end];
    skiq_rx_hdl_t rx_hdl;
    char *handle_str_dup;
    int32_t status = 0;

    *p_nr_handles = 0;

    for ( rx_hdl = skiq_rx_hdl_A1; rx_hdl < skiq_rx_hdl_end; rx_hdl++ )
    {
        handle_requested[rx_hdl] = false;
    }

    handle_str_dup = strdup( handle_str );
    if ( handle_str_dup != NULL )
    {
        char *token = strtok( handle_str_dup, TOKEN_LIST );

        /**
           There are 3 exit criteria for this 'while' loop:

           1. status is non-zero     (invalid handle or duplicate handle)
           2. token is NULL          (strtok has exhausted the string)
           3. handle_str_dup is NULL ('ALL' token was found in the string)
        */
        while ( ( status == 0 ) && ( token != NULL ) && ( handle_str_dup != NULL ) )
        {
            rx_hdl = str2hdl( token );
            if ( rx_hdl == skiq_rx_hdl_end )
            {
                if( 0 == strcasecmp(handle_str, "ALL") )
                {
                    free( handle_str_dup );
                    handle_str_dup = NULL;
                }
                else
                {
                    fprintf(stderr, "Error: invalid handle specified: %s\n",token);
                    status = ERROR_COMMAND_LINE;
                }
            }
            else
            {
                if (handle_requested[rx_hdl] == true)
                {
                    fprintf(stderr, "Error: handle specified multiple times: %s\n",token);
                    status = ERROR_COMMAND_LINE;
                }
                else
                {
                    handle_requested[rx_hdl] = true;
                    if ((*p_nr_handles) < skiq_rx_hdl_end)
                    {
                        rx_handles[(*p_nr_handles)++] = rx_hdl;
                    }
                }
            }

            if ( ( status == 0 ) && ( handle_str_dup != NULL ) )
            {
                token = strtok( NULL, TOKEN_LIST );
            }
        }

        if ( handle_str_dup == NULL )
        {
            /* User specified 'ALL' in the list of handles, set number of handles to 0 and return
             * success */
            *p_nr_handles = 0;
            return 0;
        }
        else
        {
            free( handle_str_dup );
            handle_str_dup = NULL;
        }

        /* set chan_mode based on whether one of the second handles in each pair is requested */
        if ( handle_requested[skiq_rx_hdl_A2] || handle_requested[skiq_rx_hdl_B2] )
        {
            *p_chan_mode = skiq_chan_mode_dual;
        }
        else
        {
            *p_chan_mode = skiq_chan_mode_single;
        }
    }
    else
    {
        fprintf(stderr, "Error: unable to allocate memory for parsing.\n");
        status = ERROR_NO_MEMORY;
    }

    if( *p_nr_handles == 0 )
    {
        fprintf(stderr, "Error: No handles specified.\n");
        status = ERROR_COMMAND_LINE;
    }

    return status;
}


/**************************** HELPER FUNCTIONS *******************************/
/*****************************************************************************/

/*****************************************************************************/
/** Convert string representation to handle constant

    @param[in] *str: string representation of handle

    @return hdl:     skiq_rx_hdl_t (enum for handle)
*/
static skiq_rx_hdl_t 
str2hdl( const char *str )
{
    return \
        ( 0 == strcasecmp( str, "A1" ) ) ? skiq_rx_hdl_A1 :
        ( 0 == strcasecmp( str, "A2" ) ) ? skiq_rx_hdl_A2 :
        ( 0 == strcasecmp( str, "B1" ) ) ? skiq_rx_hdl_B1 :
        ( 0 == strcasecmp( str, "B2" ) ) ? skiq_rx_hdl_B2 :
        ( 0 == strcasecmp( str, "C1" ) ) ? skiq_rx_hdl_C1 :
        ( 0 == strcasecmp( str, "D1" ) ) ? skiq_rx_hdl_D1 :
        skiq_rx_hdl_end;
}


/******************************************************************************/
/** Convert skiq_rx_hdl_t constant to string representation

    @param[in] hdl:  skiq_rx_hdl_t (enum for handle)

    @return char*:   string representation of handle
*/
static const char *
hdl_cstr( skiq_rx_hdl_t hdl )
{
    return \
        (hdl == skiq_rx_hdl_A1) ? "A1" :
        (hdl == skiq_rx_hdl_A2) ? "A2" :
        (hdl == skiq_rx_hdl_B1) ? "B1" :
        (hdl == skiq_rx_hdl_B2) ? "B2" :
        (hdl == skiq_rx_hdl_C1) ? "C1" :
        (hdl == skiq_rx_hdl_D1) ? "D1" :
        "unknown";
}


/*****************************************************************************/
/** Convert skiq_1pps_source_t to string representation

    @param[in] source:  skiq_1pps_source_t (enum for pps source)

    @return    char*:   string representation of pps source
*/
static const char *
pps_source_cstr( skiq_1pps_source_t source )
{
    return \
        (skiq_1pps_source_unavailable == source) ? "unavailable" :
        (skiq_1pps_source_external == source) ? "external" :
        (skiq_1pps_source_host == source) ? "host" :
        "unknown";
}


/*****************************************************************************/
/** Convert skiq_trigger_src_t constant to string representation

    @param[in] src: skiq_trigger_src_t (enum for trigger)

    @return char*:  string representation of handle
*/
static const char *
trigger_src_desc_cstr( skiq_trigger_src_t src )
{
    return \
        (src == skiq_trigger_src_immediate) ? "immediately" :
        (src == skiq_trigger_src_1pps) ? "on next 1PPS pulse" :
        (src == skiq_trigger_src_synced) ? "with aligned timestamps" :
        "unknown";
}


/*****************************************************************************/
/** Convert skiq_chan_mode_t constant to string representation

    @param[in] src: skiq_chan_mode_t (enum for trigger)

    @return char*:  string representation of handle
*/
static const char *
chan_mode_desc_cstr( skiq_chan_mode_t mode )
{
    return \
        (mode == skiq_chan_mode_dual) ? "dual" :
        (mode == skiq_chan_mode_single) ? "single" :
        "unknown";
}


/*****************************************************************************/
/** Convert skiq_iq_order_t constant to string representation

    @param[in] order: skiq_iq_order_t

    @return char*: string representation of handle
*/
static const char *
iq_order_desc_cstr( skiq_iq_order_t order )
{
    return \
        (order == skiq_iq_order_qi) ? "Q then I" :
        (order == skiq_iq_order_iq) ? "I then Q" :
        "unknown";
}


#ifdef VERBOSE
/*****************************************************************************/
/** Dump rconfig to console for debugging

    @param[in] *p_rconfig:  radio config pointer 

    @return char*:          string representation of handle
*/
void
dump_rconfig(const struct radio_config *p_rconfig)
{
    if( p_rconfig != NULL )
    {

        printf("\nDEBUG: rconfig dump");
        printf("\nDEBUG: number of cards   %" PRIu8,  p_rconfig->num_cards );
        printf("\nDEBUG: blocking_rx:      %s",       p_rconfig->blocking_rx      ? "true" : "false");
        printf("\nDEBUG: all_chans:        %s",       p_rconfig->all_chans        ? "true" : "false");
        printf("\nDEBUG: packed:           %s",       p_rconfig->packed           ? "true" : "false");
        printf("\nDEBUG: use_counter:      %s",       p_rconfig->use_counter      ? "true" : "false");
        printf("\nDEBUG: disable_dc_corr:  %s",       p_rconfig->disable_dc_corr  ? "true" : "false");
        printf("\nDEBUG: skiq_initialized: %s",       p_rconfig->skiq_initialized ? "true" : "false");
        printf("\nDEBUG: rx_gain_manual:   %s",       p_rconfig->rx_gain_manual   ? "true" : "false");
        printf("\nDEBUG: lo_freq:          %" PRIu64, p_rconfig->lo_freq );
        printf("\nDEBUG: rx_gain:          %" PRIu8,  p_rconfig->rx_gain );
        printf("\nDEBUG: sample_rate:      %" PRIu32, p_rconfig->sample_rate );
        printf("\nDEBUG: bandwidth:        %" PRIu32, p_rconfig->bandwidth );
        printf("\nDEBUG: trigger_src:      %s",       trigger_src_desc_cstr(p_rconfig->trigger_src) );
        printf("\nDEBUG: pps_source:       %s",       pps_source_cstr(p_rconfig->trigger_src) );
        printf("\nDEBUG: iq_order_mode:    %s",       iq_order_desc_cstr(p_rconfig->iq_order_mode) );

        {
            uint8_t card_index;
            uint8_t num_cards = p_rconfig->num_cards;
            for( card_index = 0; card_index < num_cards; card_index++ )
            {
                uint8_t card_id     = p_rconfig->cards[card_index];
                uint8_t num_handles = p_rconfig->nr_handles[card_id];

                printf("\nDEBUG: card        %" PRIu8, card_id);
                printf("\n       chan mode   %s",      chan_mode_desc_cstr(p_rconfig->chan_mode[card_id]));
                printf("\n       num handles %" PRIu8, num_handles);
                printf("\n       handles   - ");

                uint8_t handle_index;
                for( handle_index = 0; handle_index < num_handles; handle_index++ )
                {
                    printf("%s,", hdl_cstr(p_rconfig->handles[card_id][handle_index]));
                }
            }
        }

        printf("\n\n");
    }    
}
#endif


/*****************************************************************************/
/** This function prints contents of raw data

    @param[in] *p_data: a reference to raw bytes
    @param[in] length:  the number of bytes references by p_data

    @return:   void
*/
static void 
hex_dump( uint8_t* p_data, 
          uint32_t length)
{
    unsigned int i, j;

    for (i = 0; i < length; i += 16)
    {
        /* print offset */
        printf("%08X:", i);

        /* print HEX */
        for (j = 0; j < 16; j++)
        {
            if ( ( j % 2 ) == 0 ) printf(" ");
            if ( ( j % 8 ) == 0 ) printf(" ");
            if ( i + j < length )
                printf("%02X", p_data[i + j]);
            else
                printf("  ");
        }
        printf("\n");
    }
}


/*****************************************************************************/
/** This function prints contents of a receive block

    @param[in] *p_block:            a reference to the receive block
    @param[in] block_size_in_bytes: the # of 32-bit samples to print

    @return:   void
*/
static void 
print_block_contents( skiq_rx_block_t* p_block,
                      int32_t block_size_in_bytes )
{
    printf("    RF Timestamp: %20" PRIu64 " (0x%016" PRIx64 ")\n",
            p_block->rf_timestamp, p_block->rf_timestamp);
    printf("System Timestamp: %20" PRIu64 " (0x%016" PRIx64 ")\n",
            p_block->sys_timestamp, p_block->sys_timestamp);
    printf(" System Metadata: %20u (0x%06" PRIx32 ")\n",
            p_block->system_meta, p_block->system_meta);
    printf("    RFIC Control: %20u (0x%04" PRIx32 ")\n",
            p_block->rfic_control, p_block->rfic_control);
    printf("     RF Overload: %20u\n", p_block->overload);
    printf("       RX Handle: %20u\n", p_block->hdl);
    printf("   User Metadata: %20u (0x%08" PRIx32 ")\n",
            p_block->user_meta, p_block->user_meta);

    printf("Header:\n");
    hex_dump( (uint8_t*)p_block, SKIQ_RX_HEADER_SIZE_IN_BYTES );

    printf("Samples:\n");
    hex_dump( (uint8_t*)p_block->data,
               block_size_in_bytes - SKIQ_RX_HEADER_SIZE_IN_BYTES );
}

/*****************************************************************************/
/** This function performs sign extension for the 12-bit value passed in.

    @param[in]: 12-bit value to be sign extended

    @return:    sign extended representation of value passed in
*/
static int16_t 
sign_extend( int16_t in )
{
    int16_t out=0;

    if( in & 0x800 )
    {
        out = in | 0xF000;
    }
    else
    {
        out = in;
    }

    return out;
}

/*****************************************************************************/
/** This unpacks the sample data, packed as 12-bits and unpacked to 16-bits.

    @param[in]  *packed_data:         pointer to packed data
    @param[out] *unpacked_data:       pointer to unpacked data
    @param[in]  num_unpacked_samples: number of samples contained in sample data
    @param[in]  block_size_in_words:  block size in words
    @param[in]  include_meta:         set to include meta data

    @return:    sign extended representation of value passed in
*/
static void 
unpack_data( int16_t  *packed_data,
             int16_t  *unpacked_data,
             uint32_t num_unpacked_samples,
             int32_t  block_size_in_words,
             bool     include_meta)
{
    uint32_t num_samples=0;
    uint32_t packed_offset=0;

    if( include_meta == true )
    {
        packed_offset += SKIQ_RX_HEADER_SIZE_IN_WORDS;
    }

    // loop through all the samples and unpack them
    for( num_samples=0; num_samples < (num_unpacked_samples*2); num_samples+=8 )
    {
        int32_t *data;

        // determine if the metadata needs to be skipped over in the unpacking
        if( (include_meta == true) && ((packed_offset % (block_size_in_words-1))==0) )
        {
            // increment over the header
            packed_offset += SKIQ_RX_HEADER_SIZE_IN_WORDS;
        }
        data = (int32_t*)(&(packed_data[packed_offset*2]));

        /* When the mode is set to true, then the 12-bit samples are packed in to make optimal use of the available bits,
           and packed as follows:
                    -31-------------------------------------------------------0-
            word 0 |I0b11|...|I0b0|Q0b11|.................|Q0b0|I1b11|...|I1b4|
                    ------------------------------------------------------------
            word 1 |I1b3|...|I1b0|Q1b11|...|Q1b0|I2b11|...|I2b0|Q2b11|...|Q2b8|
                    -31-------------------------------------------------------0-
            word 2 |Q2b7|...|Q2b0|I3b11|.................|I3b0|Q1311|....|Q3b4|
                    ------------------------------------------------------------
        */
        // unpack the data - save the unpacked data
        // q0 = bits 19(inclusive) - 8(inclusive) of word 0
        unpacked_data[num_samples] = sign_extend((data[0] & 0x000FFF00) >> 8);
        // i0 = bits 31(inclusive) - 20(inclusive) of word 0
        unpacked_data[num_samples+1] = sign_extend((data[0] & 0xFFF00000) >> 20);
        // q1 = bits 27(inclusive) - 16(inclusive) of word 1
        unpacked_data[num_samples+2] = sign_extend((data[1] & 0x0FFF0000) >> 16);
        // l1 = bits 7(inclusive) - 0(inclusive) of word 0 with bits 31(inclusive) - 28(inclusive) or word 1
        unpacked_data[num_samples+3] = sign_extend(((data[0] & 0x000000FF) << 4) | ((data[1] & 0xF0000000)>>28));
        // q2 = bits 3(inclusive) - 0(inclusive) of word 1 with bits 31(inclusive) - 24(inclusive) of word 2
        unpacked_data[num_samples+4] = sign_extend(((data[1] & 0x0000000F) << 8) | ((data[2] & 0xFF000000) >> 24));
        // i2 = bits 15(inclusive) to 4(inclusive)
        unpacked_data[num_samples+5] = sign_extend((data[1] & 0x0000FFF0) >> 4);
        // q3 = lower 12 bits of word 2
        unpacked_data[num_samples+6] = sign_extend((data[2] & 0x00000FFF));
        // i3 = bits 23(inclusive) - 12 of word 2
        unpacked_data[num_samples+7] = sign_extend(((data[2] & 0x00FFF000) >> 12));

        // every 3 words of data contain 4 samples in packed mode
        packed_offset += 3;
    }

}


/************************ VERIFICATION FUNCTIONS *****************************/
/*****************************************************************************/

/*****************************************************************************/
/** This function verifies that the received sample data is a monotonically
    increasing counter.

    @param[in] card:                card id
    @param[in] *p_data:             a pointer to the first 32-bit sample in the buffer to
                                    be verified
    @param[in] num_samps:           the number of samples to verify
    @param[in] block_size_in_words: the number of samples in a block (in words)
    @param[in] include_meta:        true if p_data contains meta data
    @param[in] packed:              true if p_data is packed
    @param[in] iq_order:            the sample order used on the collected data
    @param[in] p_hdl_str:           the string representation of the handle in use

    @return:   0 on success
*/
static int32_t 
verify_data( uint8_t     card, 
             int16_t*    p_data, 
             uint32_t    num_samps, 
             int32_t     block_size_in_words, 
             bool        include_meta, 
             bool        packed,
             skiq_iq_order_t iq_order,
             const char* p_hdl_str )
{
    uint8_t rx_resolution = 0;
    int32_t status        = 0;

    status = skiq_read_rx_iq_resolution( card, &rx_resolution );
    if(status != 0)
    {
        fprintf(stderr, "Error: card %" PRIu8 " getting IQ resolution (status: %" PRIi32 ") for handle %s\n", 
                card, status, p_hdl_str);
    }
    else
    {
        if(rx_resolution != 0)
        {
            uint32_t offset   = 0;
            int16_t last_data = 0;
            int16_t max_data  = 0;

            max_data = (1 << (rx_resolution-1))-1;

            printf("Info: card %" PRIu8 " verifying counter data, number of samples %" PRIu32 " (RX resolution %" PRIu8 " bits) for handle %s\n", 
                    card, num_samps, rx_resolution, p_hdl_str);

            if( include_meta == true )
            {
                offset = (SKIQ_RX_HEADER_SIZE_IN_WORDS*2);
            }

            /* Check the IQ ordering mode.  If IQ ordering, use the SWAP macro on the
            received array so the original verify_data() function can be used (which was
            designed for QI ordering). */
            if (iq_order == skiq_iq_order_iq)
            {
                for(uint32_t temp_offset=offset; temp_offset<(num_samps*2); temp_offset+=2 )
                {
                    SWAP(p_data[temp_offset], p_data[temp_offset+1]);
                }
            }

            last_data = p_data[offset]+1;
            offset++;
            for( ; (offset<(num_samps*2)) && (status == 0); offset++ )
            {
                if( (include_meta == true) &&
                    ((offset % (block_size_in_words * 2))==0) &&
                    (packed == false) )
                {
                    /*
                        If the metadata is included in the sample data and we've reached the end of
                        a block, skip over the metadata header
                    */
                    offset += (SKIQ_RX_HEADER_SIZE_IN_WORDS*2);
                }

                if( last_data != p_data[offset] )
                {
                    fprintf(stderr, "Error: at sample %u, expected 0x%x but got 0x%x for handle %s\n",
                           offset, (uint16_t)last_data, (uint16_t)p_data[offset], p_hdl_str);
                    status = ERROR_COMMAND_LINE;
                    continue;
                }

                last_data = p_data[offset]+1;
                /* see if we need to wrap the data */
                if( last_data == (max_data+1) )
                {
                    last_data = -(max_data+1);
                }
            }
        }
        else
        {
            fprintf(stderr, "Error: card %" PRIu8 " illegal IQ resolution ( %" PRIu8 " bits); verification skipped for handle %s\n", 
                    card, rx_resolution, p_hdl_str);
            status = ERROR_CARD_CONFIGURATION;
        }
    }

    if( status == 0 )
    {
        printf("Info: card %" PRIu8 " verification completed successfully for handle %s\n", card, p_hdl_str);
    }

    return(status);
}


/*****************************************************************************/
/** Compare timestamps

    @param[in] ts1: timestamp
    @param[in] ts2: timestamp

    @return:        absolute value of delta
*/
static uint64_t
compare_timestamps( uint64_t ts1,
                    uint64_t ts2 )
{
    uint64_t delta = 0;

    if ( ts1 > ts2 )
    {
        delta = ( ts1 - ts2 );
    }
    else if ( ts2 > ts1 )
    {
        delta = ( ts2 - ts1 );
    }

    return delta;
}


/*****************************************************************************/
/** Compare timestamps and output error to stderr if different

    @param[in] card:        card #
    @param[in] *ts_desc:    string describing timestamps compared
    @param[in] ts1:         timestamp 1
    @param[in] hdl1:        handle for timestamp 1
    @param[in] ts2:         timestamp 2
    @param[in] hdl2:        handle for timestamp 2

    @return bool:           true if match
*/
static bool
cmp_timestamp_pair( uint8_t card,
                    const char *ts_desc,
                    uint64_t ts1,
                    skiq_rx_hdl_t hdl1,
                    uint64_t ts2,
                    skiq_rx_hdl_t hdl2 )
{
    uint64_t ts_delta = compare_timestamps(ts1, ts2);

    if ( ts_delta != 0 )
    {
        fprintf(stderr, "Error: %s timestamps MISMATCH for card %" PRIu8 " --> %s: 0x%016" PRIx64
                " <> %s: 0x%016" PRIx64 " ", ts_desc, card, hdl_cstr(hdl1), ts1,
                hdl_cstr(hdl2), ts2);
        fprintf(stderr, "(delta %" PRIu64 ")\n", ts_delta);
        return false;
    }

    return true;
}


/******************************************************************************/
/** In FPGA bitstreams v3.11.0, there is a known issue where an extraneous 
    sample block from a handle may arrive after synchronously stopping streaming.  
    This comparison function accounts for the extra block by allowing timestamps 
    to either match or be +/- one sample block different from one another.

    @param[in] card:        card #
    @param[in] *ts_desc:    string describing timestamps compared
    @param[in] ts1:         timestamp 1
    @param[in] hdl1:        handle for timestamp 1
    @param[in] ts2:         timestamp 2
    @param[in] hdl2:        handle for timestamp 2

    @return bool:           true if match
 */
static bool
cmp_timestamp_pair_fuzzy( uint8_t card,
                          const char *ts_desc,
                          int delta,
                          uint64_t ts1,
                          skiq_rx_hdl_t hdl1,
                          uint64_t ts2,
                          skiq_rx_hdl_t hdl2 )
{
    uint64_t ts_delta = compare_timestamps(ts1, ts2);

    if ( ts_delta != 0 )
    {
        /* if mismatched, check to see if the timestamps differ by the expected delta.  If they do,
         * emit a warning, otherwise it's a true mismatch. */
        if( ts_delta == delta )
        {
            fprintf(stderr, "Warning: %s timestamps MISMATCH for card %" PRIu8 " --> %s: 0x%016" PRIx64
                    " <> %s: 0x%016" PRIx64 ", but only by a single block\n", ts_desc, card,
                    hdl_cstr(hdl1), ts1, hdl_cstr(hdl2), ts2);
        }
        else
        {
            fprintf(stderr, "Error: %s timestamps MISMATCH for card %" PRIu8 " --> %s: 0x%016" PRIx64
                    " <> %s: 0x%016" PRIx64 " ", ts_desc, card, hdl_cstr(hdl1), ts1,
                    hdl_cstr(hdl2), ts2);
            fprintf(stderr, "(delta %" PRIu64 ")\n", ts_delta);
        }

        return false;
    }

    return true;
}


/*****************************************************************************/
/** Compare timestamps by handle - only works for 2 handles

    @param[in] card:        card #
    @param[in] sample_rate: sample rate - used to calculate delta
    @param[in] rx_stats:    struct rx_stats
    @param[in] hdl1:        handle for timestamp 1
    @param[in] hdl2:        handle for timestamp 2

    @return bool:           true if match
*/
static bool
cmp_timestamps_by_hdl( uint8_t card,
                       uint32_t sample_rate,
                       struct rx_stats rxs[],
                       skiq_rx_hdl_t hdl1,
                       skiq_rx_hdl_t hdl2,
                       bool packed )
{
    bool verified = true;

    /* if both first_rf_ts values are strictly greater than 0, then the handles were likely
     * requested and ultimately received samples */
    if ( ( rxs[hdl1].first_rf_ts > 0 ) && ( rxs[hdl2].first_rf_ts > 0 ) )
    {
        uint64_t rf_ts_delta, sys_ts_delta;
        int nr_samples_in_block;
        uint64_t sys_ts_freq;
        int32_t block_size_in_words = skiq_read_rx_block_size( card, skiq_rx_stream_mode_high_tput );
        int32_t status;

        if( block_size_in_words < 0 )
        {
            fprintf(stderr, "Error: Failed to read RX block size for card %" PRIu8 " with status %" PRIi32 "\n", 
                    card, block_size_in_words);
            return false;
        }

        nr_samples_in_block = ( block_size_in_words - SKIQ_RX_HEADER_SIZE_IN_BYTES ) / 4;

        /* read system timestamp reference to determine the expected system timestamp delta */
        status = skiq_read_sys_timestamp_freq( card, &sys_ts_freq );
        if ( status != 0 )
        {
            fprintf(stderr, "Error: unable to read the system timestamp frequency for card %" PRIu8 " (status=%" PRIi32 ")\n",
                    card, status);
            return false;
        }

        if ( packed )
        {
            rf_ts_delta = SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK(nr_samples_in_block);
        }
        else
        {
            rf_ts_delta = nr_samples_in_block;
        }
        sys_ts_delta = rf_ts_delta * ( (double) sys_ts_freq / (double) sample_rate );

        /* compare all four timestamp pairs */
        if ( !cmp_timestamp_pair( card, "First RF",
                                  rxs[hdl1].first_rf_ts, hdl1,
                                  rxs[hdl2].first_rf_ts, hdl2 ) )
        {
            verified = false;
        }

        if ( !cmp_timestamp_pair( card, "First System",
                                  rxs[hdl1].first_sys_ts, hdl1,
                                  rxs[hdl2].first_sys_ts, hdl2 ) )
        {
            verified = false;
        }


        if ( !cmp_timestamp_pair_fuzzy( card, "Last RF", rf_ts_delta,
                                        rxs[hdl1].last_rf_ts, hdl1,
                                        rxs[hdl2].last_rf_ts, hdl2 ) )
        {
            verified = false;
        }

        if ( !cmp_timestamp_pair_fuzzy( card, "Last System", sys_ts_delta,
                                        rxs[hdl1].last_sys_ts, hdl1,
                                        rxs[hdl2].last_sys_ts, hdl2 ) )
        {
            verified = false;
        }

        if (verified)
        {
            printf("Info: All timestamp pairs MATCH for card %" PRIu8 " on handles %s and %s\n", card,
                   hdl_cstr(hdl1), hdl_cstr(hdl2));
        }
    }

    return verified;
}


/********************* RADIO CONFIGURATION FUNCTIONS *************************/
/*****************************************************************************/

/*****************************************************************************/
/** get ALL handles for a specific card

    @param[in]  card:          card_id
    @param[out] *rx_handles:   array of skiq_rx_hdl_t and size skiq_rx_hdl_end 
    @param[out] *p_nr_handles: pointer to number of handles
    @param[out] *p_chan_mode:  pointer to skiq_chan_mode_t

    @return int32_t:           - skiq_read_num_rx_chans 
                               - skiq_read_rx_stream_handle_conflict
                               - 0 success
*/
static int32_t 
get_all_handles( uint8_t card, 
                 skiq_rx_hdl_t rx_handles[], //Assumed to be skiq_rx_hdl_end elements long 
                 uint8_t *p_nr_handles,
                 skiq_chan_mode_t *p_chan_mode )
{
    uint8_t         hdl_idx;
    skiq_rx_hdl_t   hdl;
    int32_t         status;
    skiq_param_t    params;

    status = skiq_read_parameters( card, &params );
    if (0 != status)
    {
        fprintf(stderr, "Error: failed to read parameters on card %" PRIu8 " with status %"
                PRIi32 "\n", card, status);
    }
    else
    {
        for ( hdl_idx = 0; hdl_idx < params.rf_param.num_rx_channels; hdl_idx++ )
        {
            skiq_rx_hdl_t hdl_conflicts[skiq_rx_hdl_end] = INIT_ARRAY(skiq_rx_hdl_end, skiq_rx_hdl_end);
            uint8_t       num_conflicts = 0;

            /* Assign the receive handle associated with the hdl_idx index */
            hdl = params.rf_param.rx_handles[hdl_idx];

            /* Determine which handles conflict with the current handle */
            status = skiq_read_rx_stream_handle_conflict(card, hdl, hdl_conflicts,&num_conflicts);
            if (status != 0)
            {
                fprintf(stderr, "Error: failed to read rx_stream_handle_conflict on"
                       " card %" PRIu8 "  with status %"
                       PRIi32 "\n", card, status);
            }
            else
            {
                uint8_t hdl_conflict_idx = 0;
                bool    safe_to_add      = true;

                /* Add the current handle if none of the conflicting handles have already been added */
                for( hdl_conflict_idx=0; (hdl_conflict_idx<num_conflicts) && (status==0) && (safe_to_add); hdl_conflict_idx++ )
                {
                    uint8_t configured_hdls_idx = 0;
                    for ( configured_hdls_idx=0;configured_hdls_idx < *p_nr_handles; configured_hdls_idx++)
                    {
                        if (rx_handles[configured_hdls_idx] == hdl_conflicts[hdl_conflict_idx])
                        {
                            safe_to_add = false;
                            break;
                        }
                    }
                }
                if (safe_to_add)
                {
                    rx_handles[(*p_nr_handles)++] = hdl;
                }
            }

            if( params.rf_param.num_rx_channels > 1 )
            {
                *p_chan_mode = skiq_chan_mode_dual;
            }
            else
            {
                *p_chan_mode = skiq_chan_mode_single;
            }
        }

        if( status == 0 )
        {
            printf("Info: card %" PRIu8 " using all Rx handles (total number of channels is %" PRIu8
                   ") mode: %s\n", card, *p_nr_handles, chan_mode_desc_cstr(*p_chan_mode));
        }
    }

    return status;
}


/*****************************************************************************/
/** Map command line arguments to rconfig structure
    Set the following fields in radio_config:
        trigger_src
        pps_source
        all_chan
        chan_mode
        all_chans
        lo_freq          (set to p_cmd_line_args->lo_freq)
        sample_rate      (set to p_cmd_line_args->sample_rate)
        bandwidth        (set to p_cmd_line_args->bandwidth)
        packed           (set to p_cmd_line_args->packed)
        use_counter      (set to p_cmd_line_args->use_counter)
        disable_dc_corr  (set to p_cmd_line_args->disable_dc_corr)
        blocking_rx      (set to p_cmd_line_args->blocking_rx)
        iq_swap          (set to p_cmd_line_args->iq_swap)
        rx_gain_manual   (set to p_cmd_line_args->rx_gain_manual)
        gain             (set to p_cmd_line_args->rx_gain)

    @param[in]   *cmd_line_args: (read) pointer to command line args
    @param[out]  *rconfig:       (write) pointer to radio_config structure

    @return int32_t:             - ERROR_COMMAND_LINE
                                 - ERROR_NO_MEMORY
                                 - 0 success
*/
static int32_t
map_arguments_to_radio_config(  const struct cmd_line_args *p_cmd_line_args, 
                                struct radio_config *p_rconfig )
{
    int32_t status = 0;

    /* Copy data that (at this time) does not require conversion from command line args */
    p_rconfig->lo_freq          = p_cmd_line_args->lo_freq;
    p_rconfig->sample_rate      = p_cmd_line_args->sample_rate;
    p_rconfig->bandwidth        = p_cmd_line_args->bandwidth;
    p_rconfig->packed           = p_cmd_line_args->packed;
    p_rconfig->use_counter      = p_cmd_line_args->use_counter;
    p_rconfig->disable_dc_corr  = p_cmd_line_args->disable_dc_corr;
    p_rconfig->blocking_rx      = p_cmd_line_args->blocking_rx;
    p_rconfig->rx_gain_manual   = p_cmd_line_args->rx_gain_manual;
    p_rconfig->rx_gain          = p_cmd_line_args->rx_gain;

    /* Convert the IQ order from a boolean to the enum value */
    if ( p_cmd_line_args->i_then_q == true )
    {
        p_rconfig->iq_order_mode = skiq_iq_order_iq;
    }
    else
    {
        p_rconfig->iq_order_mode = skiq_iq_order_qi;
    }

    /* if the user specified a card index, use that, otherwise default to all cards */
    if ( p_cmd_line_args->card_is_present == false )
    {
        /*
            in      xport type  [skiq xport type t] transport type to detect card
            out     num cards   pointer to where to store the number of cards
            out     cards       pointer to where to store the card numbers of the Sidekiqs available.
                                There should be room to store at least SKIQ MAX NUM CARDS at this
                                location.
        */
        status = skiq_get_cards( skiq_xport_type_auto, &p_rconfig->num_cards, p_rconfig->cards );
        if( status != 0 )
        {
            fprintf(stderr, "Error: unable to acquire number of cards (result code %" PRIi32 ")\n", 
                    status);
        }
        else
        {
            if( p_rconfig->num_cards == 0 )
            {
                fprintf(stderr, "Error: no cards detected\n");
                status = ERROR_CARD_CONFIGURATION;
            }
        }
    }
    else
    {
        if ( (SKIQ_MAX_NUM_CARDS - 1) < p_cmd_line_args->card_id )
        {
            fprintf(stderr, "Error: card ID %" PRIu8 " exceeds the maximum card ID"
                    " (%" PRIu8 ")\n", p_cmd_line_args->card_id, (SKIQ_MAX_NUM_CARDS - 1));
            status = ERROR_COMMAND_LINE;
        }
        else
        {
            p_rconfig->cards[0] = p_cmd_line_args->card_id;
            p_rconfig->num_cards = 1;
        }
    }

    if( status == 0 )
    {
        skiq_rx_hdl_t handles[skiq_rx_hdl_end]  = INIT_ARRAY(skiq_rx_hdl_end, skiq_rx_hdl_end);
        skiq_chan_mode_t chan_mode              = skiq_chan_mode_single;
        uint8_t nr_handles                      = 0;
        /* map argument values to Sidekiq specific variable values */
        status = parse_hdl_list( p_cmd_line_args->p_hdl, handles, &nr_handles, &chan_mode );
        if ( status != 0 )
        {
            fprintf(stderr, "Error: parsing handles\n");
            status = ERROR_COMMAND_LINE;
        }
        else
        {
            /* If nr_handles == 0 then use ALL handles */
            if( nr_handles == 0 )
            {
                p_rconfig->all_chans = true;
            }
            else
            {
                /* copy the handles to all the card thread parameters */
                uint8_t i, j;
                for( i=0; i<p_rconfig->num_cards; i++ )
                {
                    uint8_t card = p_rconfig->cards[i]; 
                    p_rconfig->nr_handles[card] = nr_handles;
                    p_rconfig->chan_mode[card] = chan_mode;
                    for( j=0; j<skiq_rx_hdl_end; j++ )
                    {
                        /* p_rconfig->handles must be of length skiq_rx_hdl_end+1
                        */
                        p_rconfig->handles[card][j] = handles[j];
                    }
                }

                if ( (p_cmd_line_args->perform_verify == true) && (nr_handles == 1) )
                {
                    fprintf(stderr, "Error: --perform-verify requires more than 1 handle\n");
                    status = ERROR_COMMAND_LINE;
                }
            }
        }
    }

    if( status == 0 )
    {
        if ( 0 == strcasecmp( p_cmd_line_args->p_trigger_src, "immediate" ) )
        {
            p_rconfig->trigger_src = skiq_trigger_src_immediate;
        }
        else if ( 0 == strcasecmp( p_cmd_line_args->p_trigger_src, "1pps" ) )
        {
            p_rconfig->trigger_src = skiq_trigger_src_1pps;
        }
        else if ( 0 == strcasecmp( p_cmd_line_args->p_trigger_src, "synced" ) )
        {
            p_rconfig->trigger_src = skiq_trigger_src_synced;
        }
        else
        {
            fprintf(stderr, "Error: invalid trigger source '%s' specified\n", p_cmd_line_args->p_trigger_src);
            status = ERROR_COMMAND_LINE;
        }
    }
    if ( p_cmd_line_args->perform_verify && ( p_rconfig->trigger_src == skiq_trigger_src_immediate ) )
    {
        fprintf(stderr, "Error: --perform-verify conflicts with --trigger-src=immediate\n");
        status = ERROR_COMMAND_LINE;
    }


    if ( (status == 0) && (p_cmd_line_args->p_pps_source != NULL) )
    {
        if ( p_rconfig->trigger_src == skiq_trigger_src_1pps )
        {
            if( 0 == strcasecmp(p_cmd_line_args->p_pps_source, "HOST") )
            {
                p_rconfig->pps_source = skiq_1pps_source_host;
            }
            else if( 0 == strcasecmp(p_cmd_line_args->p_pps_source, "EXTERNAL") )
            {
                p_rconfig->pps_source = skiq_1pps_source_external;
            }
            else
            {
                fprintf(stderr, "Error: invalid 1PPS source '%s' specified", p_cmd_line_args->p_pps_source);
                status = ERROR_COMMAND_LINE;
            }
        }
        else
        {
            fprintf(stderr, "Error: cannot use --pps-source without specifying '1pps' with the "
                    "--trigger-src option\n");
            status = ERROR_COMMAND_LINE;
        }
    }

    if( status == 0 )
    {
        if ( p_cmd_line_args->include_meta == true )
        {
            printf("Info: including metadata in capture output\n");
        }
    }

    return status;
}


/*****************************************************************************/
/** Configures the radio card at index 'card' given a radio_config structure

    @param[in]     card:        card to initialize
    @param[in/out] *rconfig:    pointer to radio_config structure

    @return status:             indicating status
*/
static int32_t 
configure_radio( uint8_t card, 
                 struct radio_config *p_rconfig )
{
    int32_t status = 0;

    if( p_rconfig->skiq_initialized == false )
    {
        /*************************** init libsidekiq **************************/
        /* initialize libsidekiq at a full level 
        */
        printf("Info: initializing libsidekiq\n");

        status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_full, p_rconfig->cards, p_rconfig->num_cards);
        if( status != 0 )
        {
            if ( EBUSY == status )
            {
                fprintf(stderr, "Error: unable to initialize libsidekiq; one or more cards"
                       " seem to be in use (result code %" PRIi32 ")\n", status);
            }
            else if ( -EINVAL == status )
            {
                fprintf(stderr, "Error: unable to initialize libsidekiq; was a valid card"
                       " specified? (result code %" PRIi32 ")\n", status);
            }
            else
            {
                fprintf(stderr, "Error: unable to initialize libsidekiq with status %" PRIi32
                       "\n", status);
            }
        }

        /******************** get handles for each card ***********************/
        /* if the user requested ALL handles, iterate through the cards to get
           the handles for each card
        */
        if( (status == 0) && (p_rconfig->all_chans) )
        {
            uint8_t i;

            for( i=0; (i < p_rconfig->num_cards) && (status == 0); i++ )
            {
                uint8_t card_id = p_rconfig->cards[i]; 
                status = get_all_handles( card_id, p_rconfig->handles[card_id], 
                        &p_rconfig->nr_handles[card_id], &p_rconfig->chan_mode[card_id] );
            }
        }

        p_rconfig->skiq_initialized = true;
    }

    if( p_rconfig->skiq_initialized == true )
    {
        uint8_t i;

        printf("Info: card %" PRIu8 " starting configuration\n", card);

        status = skiq_write_iq_order_mode(card, p_rconfig->iq_order_mode);
        if( 0 != status )
        {
            fprintf(stderr, "Error: card %" PRIu8 " failed to set iq_order_mode"
                   " (status %" PRIi32 ")\n", card, status);
        }

        /* configure the 1PPS source for each of the cards */
        if( ( status == 0 ) && ( p_rconfig->pps_source != skiq_1pps_source_unavailable ))
        {
            status = skiq_write_1pps_source( card, p_rconfig->pps_source );
            if( status != 0 )
            {
                fprintf(stderr, "Error: card %" PRIu8 " unable to configure PPS source to %s (status=%" PRIi32 ")\n",
                      card, pps_source_cstr( p_rconfig->pps_source ), status );
            }
            else
            {
                printf("Info: card %" PRIu8 " configured 1PPS source to %s\n", card, pps_source_cstr( p_rconfig->pps_source) );
            }
        }

        /* set the sample rate and bandwidth */
        for ( i = 0; (i < p_rconfig->nr_handles[card]) && (status == 0); i++)
        {
            status = skiq_write_rx_sample_rate_and_bandwidth(card, p_rconfig->handles[card][i],
                                                                   p_rconfig->sample_rate,
                                                                   p_rconfig->bandwidth);
            if (status != 0)
            {
                fprintf(stderr, "Error: card %" PRIu8 " failed to set Rx sample rate or bandwidth and "
                        "handle %s ... status is %" PRIi32 "\n", card, hdl_cstr(p_rconfig->handles[card][i]),
                        status);
            }
            else
            {
                uint32_t read_sample_rate;
                uint32_t read_bandwidth;
                uint32_t actual_bandwidth;
                double   actual_sample_rate;

                /* read back the sample rate and bandwidth to determine the achieved bandwidth */
                status = skiq_read_rx_sample_rate_and_bandwidth(card,
                                                                p_rconfig->handles[card][i],
                                                                &read_sample_rate,
                                                                &actual_sample_rate,
                                                                &read_bandwidth,
                                                                &actual_bandwidth);
                if ( status != 0 )
                {
                    fprintf(stderr, "Error: card %" PRIu8 " failed to read sample rate and bandwidth" 
                            " ... status is %" PRIi32 "\n", 
                            card, status);
                }
                else
                {
                    printf("Info: card %" PRIu8 " actual sample rate is %f, actual bandwidth is %" PRIu32 " and "
                           "handle %s\n", card, actual_sample_rate, actual_bandwidth,
                           hdl_cstr(p_rconfig->handles[card][i]));

                    p_rconfig->sample_rate = actual_sample_rate;
                    p_rconfig->bandwidth = actual_bandwidth;
                }
            }
        }

        if( status == 0 )
        {
            /* initialize the receive parameters for a given card */
            status = skiq_write_chan_mode(card, p_rconfig->chan_mode[card]);
            if ( status != 0 )
            {
                fprintf(stderr, "Error: card %" PRIu8 " failed to set channel mode ... status is %" PRIi32 "\n", card, status);
            }

            if ( ( status == 0 ) && p_rconfig->blocking_rx )
            {
                /* set a modest rx timeout */
                status = skiq_set_rx_transfer_timeout( card, TRANSFER_TIMEOUT );
                if( status != 0 )
                {
                    fprintf(stderr, "Error: card %" PRIu8 " unable to set RX transfer timeout ... status is %" PRIi32 "\n", card,
                           status);
                }
            }
        }

        /* tune the Rx chain to the requested freq for each specified handle */
        for ( i = 0; (i < p_rconfig->nr_handles[card]) && (status == 0); i++)
        {
            status = skiq_write_rx_LO_freq(card, p_rconfig->handles[card][i], p_rconfig->lo_freq);
            if ( status != 0 )
            {
                fprintf(stderr, "Error: card %" PRIu8 " failed to set LO freq on handle %s ... status is %" PRIi32 "\n",
                       card, hdl_cstr(p_rconfig->handles[card][i]), status);
            }
        }

        if ( status == 0 )
        {
            skiq_rx_gain_t gain_mode;

            /* set the gain_mode based on rx_gain from the arguments */
            if (p_rconfig->rx_gain_manual)
            {
                gain_mode = skiq_rx_gain_manual;
            }
            else
            {
                gain_mode = skiq_rx_gain_auto;
            }

            for ( i = 0; (i < p_rconfig->nr_handles[card]) && (status == 0); i++)
            {
                status = skiq_write_rx_gain_mode( card, p_rconfig->handles[card][i], gain_mode );
                if (status != 0)
                {
                    fprintf(stderr, "Error: card %" PRIu8 " failed to set Rx gain mode on handle "
                            "%s ... status is %" PRIi32 "\n", card, hdl_cstr(p_rconfig->handles[card][i]), status);
                }
                else if( gain_mode == skiq_rx_gain_manual )
                {
                    status=skiq_write_rx_gain(card, p_rconfig->handles[card][i], p_rconfig->rx_gain);
                    if (status != 0)
                    {
                        fprintf(stderr,"Error: card %" PRIu8 " failed to set gain index to %" PRIu8 " (status %" PRIi32 ")\n", 
                                p_rconfig->rx_gain, card, status);
                    }
                    printf("Info: card %" PRIu8 " set gain index to %" PRIu8 " on handle %s\n", 
                            p_rconfig->rx_gain, card,hdl_cstr(p_rconfig->handles[card][i]) );
                }
            }
        }

        if ( status == 0 )
        {
            /* set the mode (packed) 
               An interface defaults to using un-packed mode if the skiq write iq pack mode()
               is not called.*/
            if( p_rconfig->packed )
            {
                status = skiq_write_iq_pack_mode(card, p_rconfig->packed);
                if ( status == -ENOTSUP )
                {
                    fprintf(stderr, "Error: card %" PRIu8 " packed mode is not supported on this Sidekiq product "
                            "\n", card);
                }
                else if ( status != 0 )
                {
                    fprintf(stderr, "Error: card %" PRIu8 " unable to set the packed mode (status %" PRIi32 ")\n",
                            card, status);
                }
                else
                {
                    printf("Info: card %" PRIu8 " configured for packed data mode\n", card);
                }
            }
        }

        if ( status == 0 )
        {
            /* set the data source (IQ or counter) */
            if ( p_rconfig->use_counter )
            {
                printf("Info: card %" PRIu8 " configured for counter data mode\n", card);
                for ( i = 0; (i < p_rconfig->nr_handles[card]) && (status == 0); i++)
                {
                    status = skiq_write_rx_data_src(card, p_rconfig->handles[card][i], skiq_data_src_counter);
                    if ( status != 0 )
                    {
                        fprintf(stderr, "Error: card %" PRIu8 " failed to set data source to counter mode on handle "
                               "%s ... status is %" PRIi32 "\n", card, hdl_cstr(p_rconfig->handles[card][i]), status);
                    }
                }
            }
            else
            {
                printf("Info: card %" PRIu8 " configured for I/Q data mode\n", card);
            }
        }

        if ( status == 0 )
        {
            if ( p_rconfig->disable_dc_corr )
            {
                printf("Info: card %" PRIu8 " disabling DC offset correction\n",
                        card);
                for ( i = 0; (i < p_rconfig->nr_handles[card]) && (status == 0); i++)
                {
                    status = skiq_write_rx_dc_offset_corr(card,
                                p_rconfig->handles[card][i], false);
                    if (status != 0)
                    {
                        fprintf(stderr, "Error: card %" PRIu8 " failed to disable DC offset correction"
                               " and handle %s with status %"
                               PRIi32 "\n", card, hdl_cstr(p_rconfig->handles[card][i]),
                               status);
                    }
                }
            }
        }
    } // if( *initialized == true )
    else
    {
        fprintf(stderr, "Error: libsidekiq not initialized\n");
         status = ERROR_LIBSIDEKIQ_NOT_INITIALIZED;
    }
 
    return status;
}


/*************************** FILES OPEN-CLOSE *********************************/
/******************************************************************************/

/******************************************************************************/
/** Open a file for each handle.

    @param[out] *output_fp:   file pointer
    @param[in]  card:         card #
    @param[in]  *handle_str:  pointer to handle string
    @param[in]  *p_file_path: pointer to file path string

    @return status: indicating status
*/
static int32_t
open_files( FILE **output_fp, 
            uint8_t card, 
            const char *handle_str, 
            const char *p_file_path  )
{
    char p_filename[OUTPUT_PATH_MAX];
    const char *dev_prefix = "/dev/";
    ssize_t compare_len = strlen(dev_prefix);
    ssize_t filename_len = (ssize_t)strlen(p_file_path);

    /*
      Append a suffix to the given filename unless the user specifies a file in the "/dev/"
      directory (most likely, "/dev/null").
    */
    if (filename_len < compare_len)
    {
        compare_len = filename_len;
    }
    if (0 == strncasecmp(p_file_path, dev_prefix, compare_len))
    {
        snprintf( p_filename, OUTPUT_PATH_MAX, "%s", p_file_path);
    }
    else
    {
        snprintf( p_filename, (OUTPUT_PATH_MAX-1), "%s.%" PRIu8 ".%s", p_file_path, card,
                  handle_str);
    }

    errno=0;
    *output_fp = fopen(p_filename,"wb");
    if (*output_fp == NULL)
    {
        fprintf(stderr, "Error: card %" PRIu8 " unable to open output file %s (%" PRIi32 ": '%s')\n", 
                card, p_filename, errno, strerror(errno));
        return (-errno);
    }
    printf("Info: card %" PRIu8 " opened file %s for output\n", 
            card, p_filename);

    return 0;
}


/******************************** THREADS *************************************/
/******************************************************************************/

/*****************************************************************************/
/** Update the rx stats structure - called from thread

    @param[in] *p_rx_stats:  pointer to struct rx_stats
    @param[in] *p_rx_block:  pointer to skiq_rx_block

    @return bool:            true if match
*/
static void
update_rx_stats( struct rx_stats *p_rx_stats,
                 skiq_rx_block_t *p_rx_block )
{
    if ( p_rx_stats->first_block )
    {
        p_rx_stats->first_rf_ts = p_rx_block->rf_timestamp;
        p_rx_stats->first_sys_ts = p_rx_block->sys_timestamp;
    }

    p_rx_stats->curr_rf_ts = p_rx_block->rf_timestamp;
    p_rx_stats->last_rf_ts = p_rx_block->rf_timestamp;
    p_rx_stats->last_sys_ts = p_rx_block->sys_timestamp;
}


/******************************************************************************/
/** This is the main function for receiving data for a specific card.

    @param[in/out] *params: thread params structure

    @return void*:          indicating status
*/
static void *receive_card( void *params )
{
    struct thread_params *p_thread_params = params;

    // Declare the read only parameters const.       
    const struct    radio_config *p_rconfig           = p_thread_params->p_rconfig;
    const uint8_t   card                              = p_rconfig->cards[p_thread_params->card_index];
    const uint32_t  num_payload_words_to_acquire      = p_thread_params->num_payload_words_to_acquire;
    const char*     p_file_path                       = p_thread_params->p_file_path;
    const bool      include_meta                      = p_thread_params->include_meta;
    const bool      perform_verify                    = p_thread_params->perform_verify;

    /* variables that need to be tracked per rx handle */
    struct thread_variables tv[skiq_rx_hdl_end] = { [0 ... (skiq_rx_hdl_end-1)] = THREAD_VARIABLES_INITIALIZER };
    struct rx_stats rx_stats[skiq_rx_hdl_end] = { [0 ... (skiq_rx_hdl_end-1)] = RX_STATS_INITIALIZER };

    /* pointer to file to save the samples */

    int32_t  num_blocks                 = 0;
    int32_t  block_size_in_words        = 0;
    uint32_t payload_words              = 0;
    uint32_t num_words_read             = 0;
    uint32_t num_blocks_received        = 0;
    uint8_t  num_hdl_rcv                = 0;
    int32_t  status                     = 0;
    uint32_t num_words_written          = 0;
    skiq_rx_stream_mode_t stream_mode   = skiq_rx_stream_mode_high_tput;
    skiq_rx_hdl_t curr_rx_hdl           = skiq_rx_hdl_end;

    uint8_t i;
    uint32_t len;                    // length (in bytes) of received data
    skiq_rx_status_t rx_status;
    skiq_rx_block_t* p_rx_block;

    /* initialize rx_stats for each handle, regardless if it's been requested or not */
    for ( i = 0; i < p_rconfig->nr_handles[card]; i++ )
    {
        skiq_rx_hdl_t hdl;
        hdl = p_rconfig->handles[card][i];

        rx_stats[hdl].nr_blocks_to_write = num_payload_words_to_acquire;

        /******************************* open files *******************************/
        status = open_files(&tv[hdl].output_fp, card, hdl_cstr(hdl), p_file_path);
        if(status != 0)
        {
            g_running = false; // Signal system that we have an error and need to stop
            goto thread_exit;
        }
    }

    /************************* calculate buffer sizes *************************/
    /* read the expected RX block size and convert to 
       number of words: ( words = bytes / 4 )
    */
    block_size_in_words = skiq_read_rx_block_size( card, stream_mode ) / 4;
    if ( block_size_in_words < 0 )
    {
        fprintf(stderr, "Error: Card %" PRIu8 " Failed to read RX block size for specified stream mode with status %" PRIi32 "\n", 
                card, block_size_in_words);
        status = ERROR_BLOCK_SIZE;
        g_running = false; // Signal system that we have an error and need to stop
        goto thread_close_files;
    }
    else if (block_size_in_words < SKIQ_RX_HEADER_SIZE_IN_WORDS)
    {
        fprintf(stderr, "Error: Card %" PRIu8 "invalid block size: %" PRIi32 ", must be > SKIQ_RX_HEADER_SIZE_IN_WORDS.\n", 
                card, block_size_in_words);
        status = ERROR_BLOCK_SIZE;
        g_running = false; // Signal system that we have an error and need to stop
        goto thread_close_files;
    }

    if( p_rconfig->packed == true )
    {
        payload_words = SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK((block_size_in_words-SKIQ_RX_HEADER_SIZE_IN_WORDS));
    }
    else
    {
        payload_words = block_size_in_words - SKIQ_RX_HEADER_SIZE_IN_WORDS;
    }

    /* set up the # of blocks to acquire according to the command line arguments */
    num_blocks = ROUND_UP(num_payload_words_to_acquire, payload_words);
    printf("Info: card %" PRIu8 " acquiring %" PRIi32 " blocks at %" PRIu32 " words per block\n", 
            card, num_blocks, payload_words);

    // determine how large a block of data is based on whether we're including metadata
    if ( include_meta == false )
    {
        block_size_in_words -= SKIQ_RX_HEADER_SIZE_IN_WORDS;
    }

    /************************* buffer allocation ******************************/
    /* allocate memory to hold the data when it comes in */
    for ( i = 0; i < p_rconfig->nr_handles[card]; i++ )
    {
        skiq_rx_hdl_t hdl;
        hdl = p_rconfig->handles[card][i];
        tv[hdl].p_rx_data = (uint32_t*)calloc(block_size_in_words * num_blocks, sizeof(uint32_t));
        if (tv[hdl].p_rx_data == NULL)
        {
            fprintf(stderr,"Error: card %" PRIu8 " didn't successfully allocate %" PRIi32 " words to hold"
                   " unpacked iq\n", card, block_size_in_words*num_blocks);
            status = ERROR_NO_MEMORY;
            g_running = false; // Signal system that we have an error and need to stop
            goto thread_close_files;
        }
        memset( tv[hdl].p_rx_data, 0, block_size_in_words * num_blocks * sizeof(uint32_t) );
        tv[hdl].p_next_write = tv[hdl].p_rx_data;
        tv[hdl].p_rx_data_start = tv[hdl].p_rx_data;
        tv[hdl].rx_block_cnt = 0;
        tv[hdl].total_num_payload_words_acquired = 0;
    }

    /************************** start Rx data flowing *************************/
    if ( p_rconfig->trigger_src == skiq_trigger_src_1pps )
    {
        /* setup the timestamps to reset on the next PPS */
        status = skiq_write_timestamp_reset_on_1pps(card, 0);
        if( status != 0 )
        {
            fprintf(stderr,"Error: card %" PRIu8 " failed to reset timestamp, status code %" PRIi32 "\n", 
                    card, status);
            g_running = false; // Signal system that we have an error and need to stop
        }
    }

    if( g_running == true )
    {
        /* To avoid a potential race condition and to better sync immediate triggers, 
           a sync mechanism is used so all the threads call start_rx_streaming as close
           to each other in time as possible. 
        */
        pthread_mutex_lock(&g_sync_lock);
        p_thread_params->init_complete = true;
        pthread_cond_wait(&g_sync_start, &g_sync_lock);
        pthread_mutex_unlock(&g_sync_lock);

        /* start streaming on the specified trigger */
        status = skiq_start_rx_streaming_multi_on_trigger( card, (skiq_rx_hdl_t *)p_rconfig->handles[card],
                                                           p_rconfig->nr_handles[card],
                                                           p_rconfig->trigger_src, 0 );
        if ( status == 0 )
        {
            printf("Info: card %" PRIu8 " starting %u Rx handle(s) on trigger %s\n", card, p_rconfig->nr_handles[card],
               trigger_src_desc_cstr( p_rconfig->trigger_src ));
        }
        else
        {
            fprintf(stderr,"Error: card %" PRIu8 " receive streaming failed to start with status code %" PRIi32 "\n", 
                    card, status);
            g_running = false; // Signal system that we have an error and need to stop
        }
    }
    num_hdl_rcv = p_rconfig->nr_handles[card];

    /****************************** RX to buffers ****************************/
    /* Acquire the requested # of data words per handle and decrement num_hdl_rcv.
       Once num_hdl_rcv is 0 it indicated the requested number of samples have been received  
    */
    while ( (num_hdl_rcv > 0) && (g_running==true) )
    {
        rx_status = skiq_receive(card, &curr_rx_hdl, &p_rx_block, &len);
        if ( skiq_rx_status_success == rx_status )
        {
            if ( ( ( curr_rx_hdl < skiq_rx_hdl_end ) && ( tv[curr_rx_hdl].output_fp == NULL ) ) ||
                 ( curr_rx_hdl >= skiq_rx_hdl_end ) )
            {
                fprintf(stderr,"Error: card %" PRIu8 " received unexpected data from unspecified hdl %u\n", 
                        card, curr_rx_hdl);
                print_block_contents(p_rx_block, len);
                status = ERROR_UNEXPECTED_DATA_FROM_HANDLE;
                g_running = false; // Signal system that we have an error and need to stop
                goto thread_stop_streaming;
            }

            /* If we have data, process it */
            if ( NULL != p_rx_block )
            {
                /* Update timestamp data */
                update_rx_stats( &(rx_stats[curr_rx_hdl]), p_rx_block );
                if ( rx_stats[curr_rx_hdl].first_block )
                {
                    printf("Info: card %" PRIu8 " First timestamps for handle %s are RF=0x%016" PRIx64
                        " System=0x%016" PRIx64 "\n", card, hdl_cstr(curr_rx_hdl),
                        rx_stats[curr_rx_hdl].first_rf_ts, rx_stats[curr_rx_hdl].first_sys_ts);

                    rx_stats[curr_rx_hdl].first_block = false;
                    /* will be incremented properly below for next time through */
                    rx_stats[curr_rx_hdl].next_rf_ts = rx_stats[curr_rx_hdl].curr_rf_ts;
                }
                /* validate the timestamp */
                else if ( !tv[curr_rx_hdl].last_block && 
                        (rx_stats[curr_rx_hdl].curr_rf_ts != rx_stats[curr_rx_hdl].next_rf_ts) )
                {
                    int64_t diff_ts;

                    diff_ts = rx_stats[curr_rx_hdl].curr_rf_ts - rx_stats[curr_rx_hdl].next_rf_ts;
                    fprintf(stderr, "Error: card %" PRIu8 " timestamp error for handle %s (blk %u) ... "
                            "expected 0x%016" PRIx64 " but got 0x%016" PRIx64 " (delta %" PRId64 ")\n",
                            card, hdl_cstr(curr_rx_hdl), num_blocks_received, rx_stats[curr_rx_hdl].next_rf_ts,
                            rx_stats[curr_rx_hdl].curr_rf_ts, diff_ts);
                    /* Timestamp error detected indicating a gap in data.  To maintain sample alignment
                       between handles, a possible resolution would be to advance the output buffer the same
                       number of missed samples */
                    print_block_contents(p_rx_block, len);
                    status = ERROR_TIMESTAMP;
                    goto thread_stop_streaming;
                }

                num_words_read = (len/4); /* len is in bytes */
                num_blocks_received++;

                /* copy over all the data if this isn't the last block */
                if( (tv[curr_rx_hdl].total_num_payload_words_acquired + payload_words) < num_payload_words_to_acquire )
                {
                    if( include_meta == true )
                    {
                        memcpy(tv[curr_rx_hdl].p_next_write,((uint32_t*)p_rx_block),
                               (num_words_read)*sizeof(uint32_t));
                        tv[curr_rx_hdl].p_next_write += num_words_read;
                    }
                    else
                    {
                        num_words_read = num_words_read - SKIQ_RX_HEADER_SIZE_IN_WORDS;
                        memcpy(tv[curr_rx_hdl].p_next_write,
                               (void *)p_rx_block->data,
                               (num_words_read)*sizeof(uint32_t));
                        tv[curr_rx_hdl].p_next_write += (num_words_read);
                    }
                    // update the # of words received and number of samples received
                    tv[curr_rx_hdl].words_received += num_words_read;
                    tv[curr_rx_hdl].total_num_payload_words_acquired += \
                        payload_words;
                    tv[curr_rx_hdl].rx_block_cnt++;
                }
                else
                {
                    if( tv[curr_rx_hdl].last_block == false )
                    {
                        /* determine the number of words still remaining */
                        uint32_t last_block_num_payload_words = \
                            num_payload_words_to_acquire - tv[curr_rx_hdl].total_num_payload_words_acquired;
                        uint32_t num_words_to_copy=0;

                        // if running in packed mode the # words to copy does
                        // not match the # words received
                        if( p_rconfig->packed == true )
                        {
                            // every 3 words contains 4 samples when packed
                            num_words_to_copy = SKIQ_NUM_WORDS_IN_PACKED_BLOCK(last_block_num_payload_words);
                        }
                        else
                        {
                            num_words_to_copy = last_block_num_payload_words;
                        }

                        // if the metadata is included, make sure to increment
                        // # words to copy and update the offset into the last
                        // block of data
                        if (include_meta)
                        {
                            num_words_to_copy += SKIQ_RX_HEADER_SIZE_IN_WORDS;
                            memcpy(tv[curr_rx_hdl].p_next_write,
                                   ((uint32_t*)p_rx_block),
                                   num_words_to_copy*sizeof(uint32_t));
                            tv[curr_rx_hdl].p_next_write += num_words_to_copy;
                        }
                        else
                        {
                            memcpy(tv[curr_rx_hdl].p_next_write,
                                   (void *)p_rx_block->data,
                                   num_words_to_copy*sizeof(uint32_t));
                            tv[curr_rx_hdl].p_next_write += num_words_to_copy;
                        }

                        tv[curr_rx_hdl].total_num_payload_words_acquired += \
                            num_words_to_copy;
                        tv[curr_rx_hdl].rx_block_cnt++;

                        num_hdl_rcv--;
                        tv[curr_rx_hdl].last_block = true;

                        tv[curr_rx_hdl].words_received += num_words_to_copy;
                    }
                }
            }

            rx_stats[curr_rx_hdl].next_rf_ts += (payload_words);
        }
        else if (skiq_rx_status_error_overrun == rx_status)
        {
            fprintf(stderr, "Error: card %" PRIu8 " I/Q sample overrun detected at block %u\n", card, tv[curr_rx_hdl].rx_block_cnt);
            status = ERROR_OVERRUN_DETECTED;
            g_running = false; // Signal system that we have an error and need to stop
            goto thread_stop_streaming;
        }
        else if (skiq_rx_status_no_data != rx_status)
        {
            //What to do here?
        }
    } // while ( (num_hdl_rcv > 0) && (running==true) )

thread_stop_streaming: // Does not over write status
    /************************ stop streaming *************************/
    /* all done, so stop streaming */
    if ( g_running == true )
    {
        int32_t local_status = 0;

        /* stop streaming on the Rx handle(s) */
        printf("Info: card %" PRIu8 " stopping %u Rx handle(s)\n", card, p_rconfig->nr_handles[card]);
        local_status = skiq_stop_rx_streaming_multi_immediate(card, (skiq_rx_hdl_t *)p_rconfig->handles[card], p_rconfig->nr_handles[card]);

        if ( local_status == 0 )
        {
            printf("Info: card %" PRIu8 " streaming stopped\n", card);
        }
        else
        {
            fprintf(stderr, "Error: card %" PRIu8 " stopping streaming FAILED with status %" PRIi32 "\n",
                    card, local_status);
            if( status == 0 ) 
            {
                status = local_status;
            }
        }
    }

    /* verify data if a counter was used instead of real I/Q data */
    if ( (p_rconfig->use_counter == true) && (g_running==true) )
    {
        int16_t *unpacked_data = NULL;

        for ( i = 0; (status == 0) && (i < p_rconfig->nr_handles[card]); i++ )
        {
            int32_t tmp_status;
            skiq_rx_hdl_t hdl;
            hdl = p_rconfig->handles[card][i];
            if( p_rconfig->packed == true )
            {
                uint32_t num_samples=tv[hdl].total_num_payload_words_acquired;
                uint32_t header_words=0;
                if( include_meta == true )
                {
                    header_words = tv[hdl].rx_block_cnt*SKIQ_RX_HEADER_SIZE_IN_WORDS;
                }
                // allocate the memory
                unpacked_data = calloc( num_samples + header_words, sizeof(int32_t) );
                if( unpacked_data != NULL )
                {
                    unpack_data( (int16_t*)(tv[hdl].p_rx_data_start), unpacked_data, num_samples, 
                            block_size_in_words, include_meta);
                    tmp_status = verify_data(   card, 
                                                unpacked_data, 
                                                num_samples, 
                                                block_size_in_words, 
                                                include_meta, 
                                                p_rconfig->packed,
                                                p_rconfig->iq_order_mode,
                                                hdl_cstr(hdl) );
                    free(unpacked_data);
                    unpacked_data = NULL;
                    if( (tmp_status != 0) && (status == 0) )
                    {
                        /*
                            the verification failed; only overwrite the status code if it's marked
                            as a success
                        */
                        status = tmp_status;
                    }
                }
                else
                {
                    fprintf(stderr, "Error: card %" PRIu8 "unable to allocate space for unpacking samples\n",
                            card);
                    status = ERROR_NO_MEMORY;
                }
            }
            else
            {
                tmp_status = verify_data(   card, 
                                            (int16_t*)(tv[hdl].p_rx_data_start),
                                            tv[hdl].words_received,
                                            block_size_in_words,
                                            include_meta,
                                            p_rconfig->packed, 
                                            p_rconfig->iq_order_mode,
                                            hdl_cstr(hdl));
                if( (tmp_status != 0) && (status == 0) )
                {
                    /*
                        the verification failed; only overwrite the status code if it's marked
                        as a success
                    */
                    status = tmp_status;
                }
            }
        }
    }

    /*************** write buffers to output files ********************/
    /* all file data has been verified, write off to our output files */
    if (g_running == false)
    {
        printf("Shutdown detected, skipping write to output files\n");
    }

    for ( i = 0; (i < p_rconfig->nr_handles[card]) && (g_running == true) && (status == 0); i++ )
    {
        skiq_rx_hdl_t hdl;
        hdl = p_rconfig->handles[card][i];
        printf("Info: card %" PRIu8 " done receiving, start write to file for hdl %s\n",
               card, hdl_cstr(hdl));
        if( tv[hdl].output_fp != NULL )
        {
            num_words_written = fwrite(tv[hdl].p_rx_data_start,
                                       sizeof(uint32_t),
                                       tv[hdl].words_received,
                                       tv[hdl].output_fp);
        }
        else
        {
            fprintf(stderr, "Error: card %" PRIu8 " failed to write %" PRIu32 
                    " words to output file\n", 
                    card, tv[hdl].words_received);
        }

        if( num_words_written < tv[hdl].words_received )
        {
            fprintf(stderr, "Warning: card %" PRIu8 " attempted to write %" PRIu32 
                    " words to output file but only wrote %" PRIu32  "\n", 
                    card, tv[hdl].words_received, num_words_written);
        }
        (void)fflush(tv[hdl].output_fp);
#if (defined __MINGW32__)
        (void)_commit(fileno(tv[hdl].output_fp));
#else
        (void)fsync(fileno(tv[hdl].output_fp));
#endif

    }

    /************************** free buffers **************************/
    /* make sure to free the receive buffers */
    for ( i = 0; i < p_rconfig->nr_handles[card]; i++)
    {
        skiq_rx_hdl_t hdl;
        hdl = p_rconfig->handles[card][i];
        if( tv[hdl].p_rx_data_start != NULL )
        {
            free(tv[hdl].p_rx_data_start);
            tv[hdl].p_rx_data_start = NULL;
        }
    }

    /******************** verify timestamps *****************************/
    /* last timestamp received for each handle */
    for ( i = 0; i < p_rconfig->nr_handles[card]; i++)
    {
        skiq_rx_hdl_t hdl;
        hdl = p_rconfig->handles[card][i];
        printf("Info: card %" PRIu8 " Last timestamps for handle %s are RF=0x%016" PRIx64
               " System=0x%016" PRIx64 "\n", card, hdl_cstr(hdl),
               rx_stats[hdl].last_rf_ts, rx_stats[hdl].last_sys_ts);
    }

    /* verify that first and last timestamps match for skiq_trigger_src_synced and
     * skiq_trigger_src_1pps trigger sources */
    if ( ( status == 0 ) && perform_verify && 
         ( ( p_rconfig->trigger_src == skiq_trigger_src_1pps ) ||
           ( p_rconfig->trigger_src == skiq_trigger_src_synced ) ) )
    {
        bool verified = true;

        printf("Info: card %" PRIu8 " verifying timestamps\n", card);


        /* A1 and A2 are expected to have matching RF / System timestamps */
        if ( !cmp_timestamps_by_hdl( card, p_rconfig->sample_rate, rx_stats, skiq_rx_hdl_A1, skiq_rx_hdl_A2, p_rconfig->packed ) )
        {
            verified = false;
        }

        /* if available, B1 and B2 are expected to have matching RF / System timestamps */
        if ( !cmp_timestamps_by_hdl( card, p_rconfig->sample_rate, rx_stats, skiq_rx_hdl_B1, skiq_rx_hdl_B2, p_rconfig->packed) )
        {
            verified = false;
        }

        if ( !verified )
        {
            status = ERROR_TIMESTAMP;
        }
    }

thread_close_files: // Does not over write status
    for ( curr_rx_hdl = skiq_rx_hdl_A1; curr_rx_hdl < skiq_rx_hdl_end; curr_rx_hdl++)
    {
        if( tv[curr_rx_hdl].output_fp != NULL )
        {
            fclose( tv[curr_rx_hdl].output_fp );
            tv[curr_rx_hdl].output_fp = NULL;
        }
    }

thread_exit:
    return (void *)(intptr_t)status;
}


/****************************** APPLICATION **********************************/
/*****************************************************************************/

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param[in] signum: the signal number that occurred

    @return void
*/
static void app_cleanup(int signum)
{
    uint8_t i;

    printf("Info: received signal %" PRIi32 ", cleaning up libsidekiq\n", signum);
    g_running = false;

    // p_rconfig is only initialized for valid cards
    for( i=0; (i < SKIQ_MAX_NUM_CARDS) && (g_thread_parameters[i].p_rconfig != NULL); i++ )
    {
        pthread_cancel( g_thread_parameters[i].receive_thread );
    }
}


/*****************************************************************************/
/** This is the main function for executing the rx_samples_on_trigger app.

    @param[in] argc:    the # of arguments from the command line
    @param[in] *argv:   a vector of ASCII string arguments from the command line

    @return int:        ERROR_COMMAND_LINE
*/
int main( int argc, char *argv[])
{
    int32_t status = 0;

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);

    /************************ parse command line ******************************/
    /* Parse command line into rconfig and pps_source. 
    */

    if( 0 == arg_parser(argc, argv, p_help_short, p_help_long, p_args) )
    {
        struct radio_config rconfig = RADIO_CONFIG_INITIALIZER;

        /* map command line arguments to radio config */
        status = map_arguments_to_radio_config((const struct cmd_line_args *)&g_cmd_line_args, &rconfig);
        if ( status == 0 )
        {
            uint8_t i;

            printf("Info: initializing %" PRIu8 " card(s)...\n", rconfig.num_cards);

            /* perform some initialization for all of the cards */
            for ( i = 0; (i < rconfig.num_cards) && (status == 0); i++ )
            {
                // configure radio expects the actual card ID, not the index
                status = configure_radio(rconfig.cards[i],&rconfig);
            }

            VERBOSE_DUMP_RCONFIG(&rconfig)

            if( status == 0 )
            {
                /*************************** kickoff threads ******************************/
                /* For each card, create a receive thread.
                */
                /* start the receive threads for each card */
                for ( i = 0; (i < rconfig.num_cards) && (g_running==true); i++ )
                {
                    int create_rvalue;

                    g_thread_parameters[i].p_rconfig                      = &rconfig;
                    g_thread_parameters[i].card_index                     = i;
                    g_thread_parameters[i].init_complete                  = false;
                    g_thread_parameters[i].include_meta                   = g_cmd_line_args.include_meta;
                    g_thread_parameters[i].perform_verify                 = g_cmd_line_args.perform_verify;
                    g_thread_parameters[i].p_file_path                    = g_cmd_line_args.p_file_path;
                    g_thread_parameters[i].num_payload_words_to_acquire   = g_cmd_line_args.num_payload_words_to_acquire;

                    create_rvalue = pthread_create( &(g_thread_parameters[i].receive_thread), 
                            NULL, receive_card, &g_thread_parameters[i] );
                    if( create_rvalue != 0 )
                    {
                        g_running = false; // Tell all the threads to terminate
                    }
                }

                /* wait for all of the threads to complete initialization */
                if( g_running == true )
                {
                    uint8_t started = 0;
                    do
                    {
                        started = 0;
                        for( i = 0; i < rconfig.num_cards; i++ )
                        {
                            if( g_thread_parameters[i].init_complete == true)
                            {
                                started++;
                            }
                        }
                        usleep(100*100); // wait for a bit and check again
                    } while( (started < rconfig.num_cards) && (g_running==true) );
                }

                if( g_cmd_line_args.settle_time != 0 )
                {
                    printf("Info: waiting %" PRIu32 " ms prior to streaming\n", 
                        g_cmd_line_args.settle_time);
                    usleep(NUM_USEC_IN_MS*g_cmd_line_args.settle_time);
                }

                /* Sync the threads - they should all execute start rx stream as close as possible
                   to each other
                */
                if( g_running == true )
                {
                    printf("Info: start streaming on all cards\n");
                }
                else
                {
                    printf("Info: threads exiting due to error or CTRL-c\n");
                }

                status = pthread_cond_broadcast(&g_sync_start);
                if( status != 0 )
                {
                    fprintf(stderr, "Error: Failed to sync threads\n");
                    g_running = false;
                }

                /********************* wait for threads to complete ***********************/

                for ( i = 0; (i < rconfig.num_cards); i++ )
                {
                    intptr_t thread_status;

                    printf("Info: card %" PRIu8 " waiting for receive thread to complete\n", rconfig.cards[i]);
                    pthread_join( g_thread_parameters[i].receive_thread, (void *)&thread_status );

                    printf("Info: card %" PRIu8 " completed receive\n", rconfig.cards[i]);

                    if( thread_status != 0 )
                    {
                        status = thread_status;
                    }
                }
            }

            if( rconfig.skiq_initialized == true )
            {
                skiq_exit();
            }
        }
        else
        {
            arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        }
    }
    else // arg parser error
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        status = ERROR_COMMAND_LINE;
    }

    return status;
}
