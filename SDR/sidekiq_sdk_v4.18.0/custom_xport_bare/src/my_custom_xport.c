/** @file   my_custom_xport.c
 * @date   Thu Jun  2 16:32:12 2016
 * 
 * @brief This source file contains a skeleton of a full custom transport.  It
 * is designed as a starting point for custom transport implementations.  The
 * primary focus of a custom transport is the card_probe, card_init, and
 * card_exit.  These functions are called from sidekiq_core when a user wishes
 * to discover, initialize, or shutdown available Sidekiq cards respectively.
 * In the custom card_init() implementation, the custom transport developer
 * registers the FPGA, RX, or TX subsystems of the transport interface based on
 * init level and any hardware specifics (i.e. different RX functions may be
 * registered depending on card identifier).
 *
 * Each function below describes the potential input(s) and the expected
 * output(s).
 * 
 * <pre>
 * Copyright 2016 Epiq Solutions, All Rights Reserved
 * </pre>
 */

/***** INCLUDES *****/ 

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

#include "sidekiq_api.h"
#include "sidekiq_xport_api.h"

/***** DEFINES *****/

#define MAX(a,b)                        ( ( (a) > (b) ) ? (a) : (b) )
#define MIN(a,b)                        ( ( (a) < (b) ) ? (a) : (b) )

/***** TYPEDEFS *****/


/***** STRUCTS *****/


/***** LOCAL VARIABLES *****/

/* assigned at the bottom of this file */
static skiq_xport_fpga_functions_t fpga_reg_ops;
static skiq_xport_rx_functions_t rx_ops;
static skiq_xport_tx_functions_t tx_ops;

/***** LOCAL FUNCTIONS *****/


/*****************************************************************************/
/** card_probe() is called during skiq_init() or any time Sidekiq cards
 * are probed.
 *
 * p_uid_list and p_num_cards pointers are provided by caller.
 *
 * p_uid_list points to an array of unique transport identifiers
 * 
 * p_card_list and p_num_cards pointers are provided by caller.
 *
 * p_card_list points to an array of uint8_t values with SKIQ_MAX_NUM_CARDS
 * entries.
 *
 * This function should assign a transport identifier to each card detected.  
 * The UID should uniquely identifier the card at the transport layer and 
 * cannot be duplicated within the p_uid_list.
 *
 * This function should assign *p_num_cards to the number of cards discovered
 * during probe with a maximum of SKIQ_MAX_NUM_CARDS.
 *
 * For example, if there are 3 cards discovered during the probe with card
 * identifiers 1, 3, and 2, then p_uid_list[0]=1, p_uid_list[1]=3,
 * p_uid_list[2]=2, and *p_num_cards=3.
 *
 * @param[out] p_card_list points to an array of uint8_t values (limited from 0 to SKIQ_MAX_NUM_CARDS-1) with SKIQ_MAX_NUM_CARDS entries.
 * @param[out] p_num_cards reference to the number of valid entries in p_card_list array, up to SKIQ_MAX_NUM_CARDS
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_card_probe( uint64_t *p_uid_list,
                              uint8_t *p_num_cards )
{
    printf(" --> %s called with %p %p\n", __FUNCTION__, p_uid_list, p_num_cards);

    printf(" *** THIS IMPLEMENTATION IS A CUSTOM TRANSPORT ***\n");

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    /* pretend there's a card with UID of 2 */
    p_uid_list[0] = 2;
    *p_num_cards = 1;

    return 0;
}


/*****************************************************************************/
/** card_init() is called during skiq_init().  This function performs all
 * the necessary initialization on the UID specified.  It is also the 
 * responsibility of this function to register FPGA, RX, and TX function pointer 
 * structs according the specified level and the card's capabilities.
 *
 * @param[in] level init level to which each card should be initialized
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_card_init( skiq_xport_init_level_t level,
                             uint64_t xport_uid )
{
    int32_t status = 0;
    skiq_xport_id_t xport_id = SKIQ_XPORT_ID_INITIALIZER;

    xport_id.type = skiq_xport_type_custom;
    xport_id.xport_uid = xport_uid;

    printf(" --> %s called with %" PRIu64 "\n", __FUNCTION__, xport_uid);

    printf(" *** THIS IMPLEMENTATION IS A CUSTOM TRANSPORT ***\n");

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/
    
    /* VERIFY CARD EXISTENCE AND INITIALIZE */

    if ( level == skiq_xport_init_level_basic )
    {
        /* if the caller wants basic, register functions for control and
         * unregister for streaming */
        xport_register_fpga_functions( &xport_id, &fpga_reg_ops );
        xport_unregister_rx_functions( &xport_id );
        xport_unregister_tx_functions( &xport_id );
    }
    else if ( level == skiq_xport_init_level_full )
    {
        /* if the caller wants full transport support, register functions
         * for control and streaming */
        xport_register_fpga_functions( &xport_id, &fpga_reg_ops );
        xport_register_rx_functions( &xport_id, &rx_ops );
        xport_register_tx_functions( &xport_id, &tx_ops );
    }

    return status;
}


/*****************************************************************************/
/** card_exit() is called from skiq_exit().  This function performs all steps
 * necessary to shutdown communication with the card hardware specified in the
 * p_card_list array.  It is also the responsibility to unregister FPGA, RX, and
 * TX functionality.
 * 
 * @param[in] level init level to which each card was previously initialized
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_card_exit( skiq_xport_init_level_t level,
                             uint64_t xport_uid )
{
    int32_t status = 0;
    skiq_xport_id_t xport_id = SKIQ_XPORT_ID_INITIALIZER;
    
    xport_id.type = skiq_xport_type_custom;
    xport_id.xport_uid = xport_uid;

    printf(" --> %s called with %u %" PRIu64 "\n", __FUNCTION__, level, xport_uid);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    /* VERIFY CARD EXISTANCE AND SHUTDOWN */

    xport_unregister_fpga_functions( &xport_id );
    xport_unregister_rx_functions( &xport_id );
    xport_unregister_tx_functions( &xport_id );

    return status;
}


/*****************************************************************************/
/** fpga_reg_read() is called widely across libsidekiq's implementation.  This
 * function's responsibility is to either populate p_data with the contents of
 * the FPGA's register at address @a addr or return a non-zero error code.
 * 
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param[in] addr address of the requested FPGA register
 * @param[out] p_data reference to a uint32_t in which to store the register's contents
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_fpga_reg_read( uint64_t xport_uid,
                                 uint32_t addr,
                                 uint32_t* p_data )
{
    printf(" --> %s called with %" PRIu64 " 0x%08x %p\n", __FUNCTION__, xport_uid, addr, p_data);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** fpga_reg_write() is called widely across libsidekiq's implementation.  This
 * function's responsibility is to either write the contents of data to the
 * FPGA's register at address @a addr or return a non-zero error code.
 * 
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param[in] addr address of the destination FPGA register
 * @param[in] data value to store in the register
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_fpga_reg_write( uint64_t xport_uid,
                                  uint32_t addr,
                                  uint32_t data )
{
    printf(" --> %s called with %" PRIu64 " 0x%08X 0x%08X\n", __FUNCTION__, xport_uid, addr, data);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** fpga_down() is called before libsidekiq needs to "disrupt" the transport
 * link.  Currently this only occurs before the FPGA is undergoing
 * re-programming. This function's responsibility is to tear down communications
 * (transport) with the specified card in preparation for re-programming.
 * 
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_fpga_down( uint64_t xport_uid )
{
    printf(" --> %s called with %" PRIu64 "\n", __FUNCTION__, xport_uid);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    /* prepare transport / hardware for the FPGA to go away for re-programming */

    return -1;
}


/*****************************************************************************/
/** fpga_up() is called after libsidekiq "disrupts" the transport link to the
 * specified card.  Currently this only occurs after the FPGA has been
 * re-programmed.  This function's responsibility is the bring communications
 * (transport) back up with the specified card.
 * 
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 * 
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_fpga_up( uint64_t xport_uid )
{
    printf(" --> %s called with %" PRIu64 "\n", __FUNCTION__, xport_uid);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    /* prepare transport / hardware for FPGA's return from re-programming */

    return -1;
}


/*****************************************************************************/
/** rx_start_streaming() is called from skiq_start_rx_streaming().  This
 * function's responsibility is perform actions necessary to start retrieving
 * IQ samples from the specified card and handle over the transport link.
 * 
 * @note This function is called by libsidekiq BEFORE the FPGA is commanded to
 * start collecting samples.
 *
 * @note libsidekiq's skiq_start_rx_streaming() calls transport RX functions
 * in the following order:
 *   - rx_pause_streaming
 *   - rx_resume_streaming
 *   - rx_flush
 *   - rx_start_streaming
 *
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 *
 * @a hdl will always be either @a skiq_rx_hdl_A1 or @a skiq_rx_hdl_A2
 * 
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param[in] hdl handle identifier to prepare the receive IQ stream
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_rx_start_streaming( uint64_t xport_uid,
                                      skiq_rx_hdl_t hdl )
{
    printf(" --> %s called with %" PRIu64 " %u\n", __FUNCTION__, xport_uid, hdl);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/**  rx_stop_streaming() is called from skiq_stop_rx_streaming().  This
 * function's responsibility is perform actions necessary to stop retrieving
 * IQ samples from the specified card and handle over the transport link.
 *
 * @note This function is called by libsidekiq BEFORE the FPGA is commanded to
 * stop collecting samples.
 *
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 * 
 * @a hdl will always be either @a skiq_rx_hdl_A1 or @a skiq_rx_hdl_A2
 * 
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param[in] hdl identifies a handle to halt the receive IQ stream
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_rx_stop_streaming( uint64_t xport_uid,
                                     skiq_rx_hdl_t hdl )
{
    printf(" --> %s called with %" PRIu64 " %u\n", __FUNCTION__, xport_uid, hdl);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** rx_pause_streaming() is called from skiq_start_rx_streaming() and
 * skiq_write_rx_sample_rate_and_bandwidth() to signal the transport link to
 * freeze retrieving IQ samples from the FPGA.  For some transport links, this
 * function may be NOP'd or assigned NULL
 * 
 * @note This function is called by libsidekiq BEFORE the FPGA is commanded
 * to start collecting samples.  Refer to the note in rx_start_streaming for
 * the call order of transport RX functions.
 * 
 * @note This function is called by libsidekiq AFTER the RX sample rate is
 * updated.  libsikdeiq's skiq_write_rx_sample_rate_and_bandwidth() calls
 * transport functions in the following order:
 *   - rx_pause_streaming
 *   - rx_resume_streaming
 *   - rx_flush
 *
 * 
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_rx_pause_streaming( uint64_t xport_uid )
{
    printf(" --> %s called with %" PRIu64 "\n", __FUNCTION__, xport_uid);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** rx_resume_streaming() is called from skiq_start_rx_streaming() and
 * skiq_write_rx_sample_rate_and_bandwidth() to signal the transport link to
 * continue retrieving IQ samples from the FPGA.  For some transport links, this
 * function may be NOP'd or assigned NULL.
 * 
 * @note This function is called by libsidekiq BEFORE the FPGA is commanded
 * to start collecting samples.  Refer to the note in rx_start_streaming for
 * the call order of transport RX functions in skiq_start_rx_streaming().
 *
 * @note This function is called by libsidekiq AFTER the RX sample rate is
 * updated.  Refer to the note in rx_pause_streaming for the call order of
 * transport RX functions in skiq_write_rx_sample_rate_and_bandwidth().
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_rx_resume_streaming( uint64_t xport_uid )
{
    printf(" --> %s called with %" PRIu64 "\n", __FUNCTION__, xport_uid);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** rx_flush() is called from skiq_start_rx_streaming() and
 * skiq_write_rx_sample_rate_and_bandwidth() to signal the transport layer to
 * dump any data buffered while retrieving IQ samples from the FPGA.  This
 * function is used internally to flush "stale data" after a call to
 * rx_pause_streaming().
 *
 * @note This function is called by libsidekiq BEFORE the FPGA is commanded
 * to start collecting samples.  Refer to the note in rx_start_streaming for
 * the call order of transport RX functions in skiq_start_rx_streaming().
 *
 * @note This function is called by libsidekiq AFTER the RX sample rate is
 * updated.  Refer to the note in rx_pause_streaming for the call order of
 * transport RX functions in skiq_write_rx_sample_rate_and_bandwidth().
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_rx_flush( uint64_t xport_uid )
{
    printf(" --> %s called with %" PRIu64 "\n", __FUNCTION__, xport_uid);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** rx_receive() is called from skiq_receive() and is responsible for providing
 * a reference to a block of IQ data of length SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES
 * and setting *p_data_len to SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES.
 * 
 * @a card will always be in a valid range (0 <= card < SKIQ_MAX_NUM_CARDS) and
 * is "active" (has gone through initialization)
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param[out] pp_data reference to IQ data memory pointer
 * @param[out] p_data_len reference to value SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_rx_receive( uint64_t xport_uid,
                              uint8_t **pp_data,
                              uint32_t *p_data_len )
{
    printf(" --> %s called with %" PRIu64 " %p %p\n", __FUNCTION__, xport_uid, pp_data, p_data_len);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** tx_initialize() is called from skiq_start_tx_streaming() and
 * skiq_start_tx_streaming_on_1pps() and is responsible initializing the
 * transmit parameters.
 * 
 * @note The skiq_tx_transfer_mode_sync setting should not use threads.  The
 * skiq_tx_transfer_mode_async should create @a num_send_threads number of
 * threads for use in an asynchronous mode.  The callback tx_complete_cb
 * function is called in async mode when the relevant sample block has been
 * committed.
 *
 * @note Threads should be torn down in a call to .tx_stop_streaming().
 *
 * @a tx_transfer_mode will always be either @a skiq_tx_transfer_mode_sync or @a
 * skiq_tx_transfer_mode_async
 * 
 * @a num_bytes_to_send will always be a multiple of 1024 bytes
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param tx_transfer_mode desired transfer mode - sync or async
 * @param num_bytes_to_send number of bytes to expect in each tx_transmit call
 * @param num_send_threads number of threads to make available for transmission - value only valid if tx_transfer_mode == skiq_tx_transfer_mode_async
 * @param tx_complete_cb function to call when transmit block has been committed to the FPGA - value only valid if tx_transfer_mode == skiq_tx_transfer_mode_async
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_tx_initialize( uint64_t xport_uid, 
                                 skiq_tx_transfer_mode_t tx_transfer_mode,
                                 uint32_t num_bytes_to_send,
                                 uint8_t num_send_threads,
                                 int32_t priority,
                                 skiq_tx_callback_t tx_complete_cb )
{
    printf(" --> %s called with %" PRIu64 " %u %u %u %d %p\n", __FUNCTION__,
           xport_uid, tx_transfer_mode, num_bytes_to_send, num_send_threads, priority, tx_complete_cb);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** tx_start_streaming() is called skiq_start_tx_streaming() and
 * skiq_start_tx_streaming_on_1pps() and is responsible for performing steps to
 * prepare the transport link for transmit sample data.
 *
 * @note This function is called AFTER the FPGA is commanded that it will be
 * transmitting samples
 * 
 * 
 * @a hdl should be ignored
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param hdl identifies a handle to prep the transport link for transmission - should be ignored, retained for legacy purposes
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_tx_start_streaming( uint64_t xport_uid,
                                      skiq_tx_hdl_t hdl )
{
    printf(" --> %s called with %" PRIu64 " %u\n", __FUNCTION__, xport_uid, hdl);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** tx_stop_streaming() is called from skiq_stop_tx_streaming() and
 * skiq_stop_tx_streaming_on_1pps() and is responsible for performing steps to
 * halt the transport link for transmit sample data.
 * 
 * @note This function is called AFTER the FPGA is commanded to stop
 * transmitting samples
 *
 * @note Threads created as part of skiq_tx_transfer_mode_async mode should be
 * destroyed here.
 * 
 * @a hdl should be ignored
 *
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param hdl identifies a handle to halt the transport link for transmission - should be ignored, retained for legacy purposes
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_tx_stop_streaming( uint64_t xport_uid,
                                     skiq_tx_hdl_t hdl )
{
    printf(" --> %s called with %" PRIu64 " %u\n", __FUNCTION__, xport_uid, hdl);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return -1;
}


/*****************************************************************************/
/** tx_transmit() is called from skiq_tx_transmit() and is responsible for
 * committing sample data to the FPGA over the transport link either in a
 * synchronous or asynchronous manner.
 * 
 * @note It is required that if transmit was initialized as @a
 * skiq_tx_transfer_mode_sync, that this function blocks until the transmit data
 * is received by the FPGA over the transport link.  If transmit was initialized
 * as @a skiq_tx_transfer_mode_async, the function immediately accepts the
 * sample block (as buffer space allows).
 * 
 * @param[in] xport_uid unique ID used to identifer the card at the transport layer
 * @param hdl identifies a handle for sample data transmission
 * @param p_samples reference to sample data of length num_bytes_send (from tx_initialize)
 * 
 * @return status where 0=success, anything else is an error.
 */
static int32_t my_tx_transmit( uint64_t xport_uid,
                               skiq_tx_hdl_t hdl,
                               int32_t *p_samples,
                               void *p_private )
{
    printf(" --> %s called with %" PRIu64 " %u %p %p\n", __FUNCTION__, xport_uid, hdl, p_samples, p_private);

    /*****************************************************************************/
    /********************** CUSTOM IMPLEMENTATION GOES HERE **********************/
    /*****************************************************************************/

    return 0;
}


/***** ADDITIONAL LOCAL VARIABLES *****/

/* these three function pointer structs are referenced in my_card_init() when
 * cards are registered with transport functions based on init level */
static skiq_xport_fpga_functions_t fpga_reg_ops = {
    .fpga_reg_read  = my_fpga_reg_read,
    .fpga_reg_write = my_fpga_reg_write,
    .fpga_down      = my_fpga_down,
    .fpga_up        = my_fpga_up,
};

static skiq_xport_rx_functions_t rx_ops = {
    .rx_configure            = NULL,
    .rx_set_block_size       = NULL,
    .rx_set_buffered         = NULL,
    .rx_start_streaming      = my_rx_start_streaming,
    .rx_stop_streaming       = my_rx_stop_streaming,
    .rx_pause_streaming      = my_rx_pause_streaming,
    .rx_resume_streaming     = my_rx_resume_streaming,
    .rx_flush                = my_rx_flush,
    .rx_set_transfer_timeout = NULL,
    .rx_receive              = my_rx_receive,
};

static skiq_xport_tx_functions_t tx_ops = {
    .tx_initialize      = my_tx_initialize,
    .tx_start_streaming = my_tx_start_streaming,
    .tx_stop_streaming  = my_tx_stop_streaming,
    .tx_transmit        = my_tx_transmit,
};

/***** GLOBAL DATA *****/

/* referenced from the main application when it registers this custom transport
 * with skiq_register_custom_transport() */
skiq_xport_card_functions_t card_ops = {
    .card_probe     = my_card_probe,
    .card_init      = my_card_init,
    .card_exit      = my_card_exit,
};
