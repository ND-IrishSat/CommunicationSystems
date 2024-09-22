#ifndef __SIDEKIQ_XPORT_TYPES_H__
#define __SIDEKIQ_XPORT_TYPES_H__

/*****************************************************************************/
/** @file sidekiq_xport_types.h
 *
 * @brief This file contains the type definitions associated with the Sidekiq
 * Transport API (sidekiq_xport_api.h).  The skiq_xport_type_t and
 * skiq_xport_init_level_t specify which transport and at which level to perform
 * card initialization.  The other function pointer structs are used by custom
 * transport developers to implement a transport layer for use by libsidekiq.
 *
 * <pre>
 * Copyright 2016-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

/***** INCLUDES *****/

#include <stdint.h>
#include <stdbool.h>

#include "sidekiq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***** DEFINES *****/

#define SKIQ_XPORT_UID_INVALID          (UINT64_MAX)

/** @brief initializer for the skiq_xport_id_t structure */
#define SKIQ_XPORT_ID_INITIALIZER               \
    {                                           \
        .xport_uid = SKIQ_XPORT_UID_INVALID,    \
        .type = skiq_xport_type_max,            \
    }


/***** TYPEDEFS *****/

/*****************************************************************************/
/** The skiq_xport_type_t enumeration is used to specify a transport or
 * combination of transports.
 */
typedef enum
{
    /*! communicate with Sidekiq entirely over PCIe */
    skiq_xport_type_pcie = 0,

    /*! communicate with Sidekiq entirely over USB */
    skiq_xport_type_usb,

    /*! communicate with Sidekiq entirely using the registered transport
     *  implementation provided by a call to
     *  skiq_register_custom_transport(). */
    skiq_xport_type_custom,

    /*! communicate with Sidekiq entirely over network interface */
    skiq_xport_type_net,

    /*! INTERNAL USE ONLY */
    skiq_xport_type_max,

    /*! automatically detect the transports available and use the preferred one */
    skiq_xport_type_auto,

    /*! INTERNAL USE ONLY */
    skiq_xport_type_unknown,
} skiq_xport_type_t;


/*****************************************************************************/
/** The @ref skiq_xport_init_level_t enumeration is used to specify an
 * initialization level for a specified transport type.  There are two available
 * types.
 */
typedef enum
{
    /*! minimal initialization necessary to bring up the requested transport
     *  interface for register reads/writes, and initialize the mutexes that
     *  serializes access to libsidekiq */
    skiq_xport_init_level_basic = 0,

    /*! Same as level_basic + perform the complete bring up of all hardware
     *  (most applications concerned with sending/receiving RF will use this) */
    skiq_xport_init_level_full,

    /*! INTERNAL USE ONLY */
    skiq_xport_init_level_unknown,
} skiq_xport_init_level_t;

typedef struct
{
    uint64_t xport_uid;
    skiq_xport_type_t type;
} skiq_xport_id_t;


/*****************************************************************************/
/** The skiq_xport_card_functions_t describes a structure of function pointers
 * to be registered for a custom transport using
 * skiq_register_custom_transport().  The .card_probe and .card_init are
 * required functions.
 */
typedef struct
{
    /**
       card_probe() is called once after a system start-up.  After that, card_hotplug() is
       responsible for updating card presence and/or absence.

       @anchor card_probe

       The @p p_uid_list and @p p_num_cards pointers are provided by caller.

       This function should assign a transport identifier to each transport interface detected.  The
       UID should uniquely identify the transport at the transport layer and cannot be duplicated
       within the @p p_uid_list.

       This function should assign @p *p_num_uids to the number of UIDs discovered during probe
       with a maximum of ::SKIQ_MAX_NUM_CARDS.

       For example, if there are 3 UIDs discovered during the probe with UIDs of 1, 3, and 2, then
       - p_uid_list[0]=1
       - p_uid_list[1]=3
       - p_uid_list[2]=2
       - *p_num_uids=3

       @param[out] p_uid_list points to an array of uint64_t UID values (limited from 0 to ::SKIQ_MAX_NUM_CARDS - 1) with ::SKIQ_MAX_NUM_CARDS entries.
       @param[out] p_num_uids reference to the number of valid entries in @p p_uid_list, up to ::SKIQ_MAX_NUM_CARDS

       @return status where 0=success, anything else is an error.
     */
    int32_t (*card_probe)( uint64_t *p_uid_list,
                           uint8_t *p_num_uids );

    /**********************************************************************************************/
    /**
       The card_hotplug() function pointer may be called during skiq_init() or any time Sidekiq
       cards are probed (e.g. after FPGA reconfiguration).

       @anchor card_hotplug

       @p uid_list and @p p_nr_uids are provided by the caller and are to be populated by this
       function while @p no_probe_uids and @p nr_no_probe_uids are provided by the caller as a list
       of UIDs that are @b NOT permitted to be probed by the transport.  These tranport identifiers
       are in use by another process and can be corrupted if probed.

       @attention This function should @b NOT probe transport identifiers that are listed in the
       @p no_probe_uids list.  This function should @b NOT indicate UIDs from the @p no_probe_uids
       list as detected in the @p uid_list.

       This function should assign a transport identifier to each transport interface detected.  The
       UID should uniquely identify the transport at the transport layer and cannot be duplicated
       within the @p uid_list.

       This function should assign @p *p_nr_uids to the number of UIDs discovered during probe
       with a maximum of ::SKIQ_MAX_NUM_CARDS.

       For example, if there are 3 UIDs discovered during the probe with UIDs of 1, 3, and 2, but 3
       is listed in the @p no_probe_uids list, then:
       - uid_list[0]=1
       - uid_list[2]=2
       - *p_nr_uids=2

        @param[out] uid_list           an array of uint64_t transport UID values with at most ::SKIQ_MAX_NUM_CARDS entries
        @param[out] p_nr_uids          reference to the number of valid entries in @p uid_list array, up to ::SKIQ_MAX_NUM_CARDS
        @param[in]  no_probe_uids      an array of uint64_t transport UID values with at most ::SKIQ_MAX_NUM_CARDS entries
        @param[in]  nr_no_probe_uids   number of valid entries in @p no_probe_uids array, up to ::SKIQ_MAX_NUM_CARDS

        @return status where 0=success, anything else is an error.
     */
    int32_t (*card_hotplug)( uint64_t uid_list[],
                             uint8_t *p_nr_uids,
                             uint64_t no_probe_uids[],
                             uint8_t nr_no_probe_uids );

    /** card_init() is called during skiq_init().  This function performs all
     * the necessary initialization on the UID specified.  It is also the
     * responsibility of this function to register FPGA, RX, and TX function pointer
     * structs according the specified level and the card's capabilities.
     *
     * @anchor card_init
     *
     * @param[in] level init level to which each card should be initialized
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*card_init)( skiq_xport_init_level_t level,
                          uint64_t xport_uid );

    /** card_exit() is called from skiq_exit().  This function performs all steps
     * necessary to shutdown communication with the card hardware specified in the
     * p_card_list array.  It is also the responsibility to unregister FPGA, RX, and
     * TX functionality.
     *
     * @anchor card_exit
     *
     * @param[in] level init level to which each card was previously initialized
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*card_exit)( skiq_xport_init_level_t level,
                          uint64_t xport_uid );

    // read the private data from the xport (used in registration)
    int32_t (*card_read_priv_data)( uint64_t xport_uid,
                                    uint8_t max_num_bytes,
                                    uint8_t *p_num_bytes,
                                    uint8_t *p_private_data );

    // write the xport private data (at least prior to init)
    int32_t (*card_write_priv_data)( uint64_t xport_uid,
                                     uint8_t num_bytes,
                                     uint8_t *p_private_data );

} skiq_xport_card_functions_t;


/*****************************************************************************/
/** The skiq_xport_fpga_functions_t describes a structure of function pointers
 * to be registered for a specified card.  Registration occurs from the
 * .card_init implementation by calling xport_register_fpga_functions.  Clearing
 * registration occurs from the .card_init or .card_exit implementations by
 * calling xport_unregister_fpga_functions.
 */
typedef struct
{
    /*****************************************************************************/
    /** fpga_reg_read() is called widely across libsidekiq's implementation.
     * This function's responsibility is to either populate p_data with the
     * contents of the FPGA's register at address @a addr or return a non-zero
     * error code.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] addr address of the requested FPGA register
     * @param[out] p_data reference to a uint32_t in which to store the register's contents
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*fpga_reg_read)( uint64_t xport_uid,
                              uint32_t addr,
                              uint32_t* p_data );

    /*****************************************************************************/
    /** fpga_reg_write() is called widely across libsidekiq's implementation.
     * This function's responsibility is to either write the contents of data to
     * the FPGA's register at address @a addr or return a non-zero error code.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] addr address of the destination FPGA register
     * @param[in] data value to store in the register
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*fpga_reg_write)( uint64_t xport_uid,
                               uint32_t addr,
                               uint32_t data );

    /*****************************************************************************/
    /** fpga_down() is called before libsidekiq needs to "disrupt" the transport
     * link.  Currently this only occurs before the FPGA is undergoing
     * re-programming. This function's responsibility is to tear down
     * communications (transport) with the specified card in preparation for
     * re-programming.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*fpga_down)( uint64_t xport_uid );

    /*****************************************************************************/
    /** fpga_down_reload() is called when libsidekiq needs to tear down the
     * current FPGA communications (transport) layer with the intent of
     * reprogramming it with a FPGA image stored in flash memory.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] addr the flash address where the FPGA bitstream resides
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*fpga_down_reload)( uint64_t xport_uid,
                                 uint32_t addr );

    /*****************************************************************************/
    /** fpga_up() is called after libsidekiq "disrupts" the transport link to
     * the specified card.  Currently this only occurs after the FPGA has been
     * re-programmed.  This function's responsibility is to bring communications
     * (transport) back up with the specified card.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*fpga_up)( uint64_t xport_uid );

    /*****************************************************************************/
    /** fpga_reg_verify() is called to verify that the specified address contains
     * value specified by data.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] addr register address to be verified
     * @param[in] data value to be verified
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*fpga_reg_verify)( uint64_t xport_uid, uint32_t addr, uint32_t data );

    /*****************************************************************************/
    /** fpga_reg_write_and_verify() is called to write a the value specified by 
     * data to the register address and verify that the data was written successfully.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] addr register address to be verified
     * @param[in] data value to be verified
     *
     * @return status where 0=success, anything else is an error.
     */    
    int32_t (*fpga_reg_write_and_verify)( uint64_t xport_uid, uint32_t addr, uint32_t data );

    int32_t (*fpga_reg_read_64)( uint64_t xport_uid, uint32_t addr, uint64_t *p_data );
    int32_t (*fpga_reg_write_64)( uint64_t xport_uid, uint32_t addr, uint64_t data );

} skiq_xport_fpga_functions_t;


/*****************************************************************************/
/** The skiq_xport_rx_functions_t describes a structure of function pointers to
 * be registered for a specified card.  Registration occurs from the .card_init
 * implementation by calling xport_register_rx_functions.  Clearing registration
 * occurs from the .card_init or .card_exit implementations by calling
 * xport_unregister_rx_functions.
 */
typedef struct
{
    /*****************************************************************************/
    /** rx_configure() is called from skiq_init(), skiq_start_rx_streaming(),
     * skiq_start_rx_streaming_on_1pps(), and
     * skiq_write_rx_sample_rate_and_bandwidth().  This function's
     * responsibility is to inform the transport implementation of the raw data
     * rate (in bytes per second) of the receive stream in case it needs to
     * adjust any transport layer configuration.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] aggregate_data_rate raw date rate in bytes per second for the receive IQ stream
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_configure)( uint64_t xport_uid,
                             uint32_t aggregate_data_rate );

    /*****************************************************************************/
    /** rx_set_block_size() is called from skiq_init(),
     * skiq_start_rx_streaming(), and skiq_start_rx_streaming_on_1pps().  This
     * function's responsibility is to inform the transport implementation of
     * desired the receive block size.  Currently the two possible settings
     * would be 4096 bytes (legacy / high throughput) and 256 bytes (low
     * latency).
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] block_size desired block size in bytes, applies to all receive handles
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_set_block_size)( uint64_t xport_uid,
                                  uint32_t block_size );

    /*****************************************************************************/
    /** rx_set_buffered() is called from skiq_init(),
     * skiq_start_rx_streaming(), and skiq_start_rx_streaming_on_1pps().  This
     * function's responsibility is to inform the transport implementation of
     * whether packet requests should be buffered (i.e. multiple receive packets
     * should be requested with a single transaction)
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] buffered indicates whether the transport packet request should buffer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_set_buffered)( uint64_t xport_uid,
                                bool buffered );

    /*****************************************************************************/
    /** rx_start_streaming() is called from skiq_start_rx_streaming() and
     * skiq_start_rx_streaming_on_1pps().  This function's responsibility is
     * perform actions necessary to start retrieving IQ samples from the
     * specified card and handle over the transport link.
     *
     * @note This function is called by libsidekiq BEFORE the FPGA is commanded
     * to start collecting samples.
     *
     * @note libsidekiq's skiq_start_rx_streaming() calls transport RX functions
     * in the following order:
     *   - rx_pause_streaming
     *   - rx_resume_streaming
     *   - rx_flush
     *   - rx_start_streaming
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @a hdl will always be a valid skiq_rx_hdl_t
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] hdl handle identifier to prepare the receive IQ stream
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_start_streaming)( uint64_t xport_uid,
                                   skiq_rx_hdl_t hdl );

    /*****************************************************************************/
    /**  rx_stop_streaming() is called from skiq_stop_rx_streaming().  This
     * function's responsibility is perform actions necessary to stop retrieving
     * IQ samples from the specified card and handle over the transport link.
     *
     * @note This function is called by libsidekiq BEFORE the FPGA is commanded
     * to stop collecting samples.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @a hdl will always be either a valid skiq_rx_hdl_t
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] hdl identifies a handle to halt the receive IQ stream
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_stop_streaming)( uint64_t xport_uid,
                                  skiq_rx_hdl_t hdl );

    /*****************************************************************************/
    /** rx_pause_streaming() is called from skiq_start_rx_streaming() and
     * skiq_write_rx_sample_rate_and_bandwidth() to signal the transport link to
     * freeze retrieving IQ samples from the FPGA.  For some transport links,
     * this function may be NOP'd or assigned NULL
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
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_pause_streaming)( uint64_t xport_uid );

    /*****************************************************************************/
    /** rx_resume_streaming() is called from skiq_start_rx_streaming() and
     * skiq_write_rx_sample_rate_and_bandwidth() to signal the transport link to
     * continue retrieving IQ samples from the FPGA.  For some transport links,
     * this function may be NOP'd or assigned NULL.
     *
     * @note This function is called by libsidekiq BEFORE the FPGA is commanded
     * to start collecting samples.  Refer to the note in rx_start_streaming for
     * the call order of transport RX functions in skiq_start_rx_streaming().
     *
     * @note This function is called by libsidekiq AFTER the RX sample rate is
     * updated.  Refer to the note in rx_pause_streaming for the call order of
     * transport RX functions in skiq_write_rx_sample_rate_and_bandwidth().
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_resume_streaming)( uint64_t xport_uid );

    /*****************************************************************************/
    /** rx_flush() is called from skiq_start_rx_streaming() and
     * skiq_write_rx_sample_rate_and_bandwidth() to signal the transport layer
     * to dump any data buffered while retrieving IQ samples from the FPGA.
     * This function is used internally to flush "stale data" after a call to
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
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_flush)( uint64_t xport_uid );

    /*****************************************************************************/
    /** rx_set_transfer_timeout() is called from skiq_set_rx_transfer_timeout(),
     * skiq_start_rx_streaming(), skiq_start_rx_streaming_on_1pps(), and
     * skiq_write_rx_sample_rate_and_bandwidth() and is responsible for updating
     * the current receive transfer timeout for the provided card.  The
     * currently permissible range of timeout is RX_TRANSFER_WAIT_FOREVER,
     * RX_TRANSFER_NO_WAIT, or a value between 20 and 1000000.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] timeout_us minimum timeout in microseconds for a blocking skiq_receive. can be RX_TRANSFER_WAIT_FOREVER, RX_TRANSFER_NO_WAIT, or 20-1000000.
     *
     * @return int32_t  status where 0=success, anything else is an error.
     */
    int32_t (*rx_set_transfer_timeout)( uint64_t xport_uid, const int32_t timeout_us );

    /*****************************************************************************/
    /** rx_receive() is called from skiq_receive() and is responsible for
     * providing a reference to a block of IQ data of length
     * SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES and setting *p_data_len to
     * SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[out] pp_data reference to IQ data memory pointer
     * @param[out] p_data_len reference to value SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*rx_receive)( uint64_t xport_uid,
                           uint8_t **pp_data,
                           uint32_t *p_data_len );

} skiq_xport_rx_functions_t;


/*****************************************************************************/
/** The skiq_xport_tx_functions_t describes a structure of function pointers to
 * be registered for a specified card.  Registration occurs from the .card_init
 * implementation by calling xport_register_tx_functions.  Clearing registration
 * occurs from the .card_init or .card_exit implementations by calling
 * xport_unregister_tx_functions.
 */
typedef struct
{
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
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @a tx_transfer_mode will always be either @a skiq_tx_transfer_mode_sync
     * or @a skiq_tx_transfer_mode_async
     *
     * @a num_bytes_to_send will always be a multiple of 1024 bytes
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] tx_transfer_mode desired transfer mode - sync or async
     * @param[in] num_bytes_to_send number of bytes to expect in each tx_transmit call
     * @param[in] num_send_threads number of threads to make available for transmission - value only valid if tx_transfer_mode == skiq_tx_transfer_mode_async
     * @param[in] tx_complete_cb function to call when transmit block has been committed to the FPGA - value only valid if tx_transfer_mode == skiq_tx_transfer_mode_async
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*tx_initialize)( uint64_t xport_uid,
                              skiq_tx_transfer_mode_t tx_transfer_mode,
                              uint32_t num_bytes_to_send,
                              uint8_t num_send_threads,
                              int32_t priority,
                              skiq_tx_callback_t tx_complete_cb );

    /*****************************************************************************/
    /** tx_start_streaming() is called skiq_start_tx_streaming() and
     * skiq_start_tx_streaming_on_1pps() and is responsible for performing steps
     * to prepare the transport link for transmit sample data.
     *
     * @note This function is called AFTER the FPGA is commanded that it will be
     * transmitting samples
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @a hdl should be ignored
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] hdl identifies a handle to prep the transport link for transmission - should be ignored, retained for legacy purposes
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*tx_start_streaming)( uint64_t xport_uid,
                                   skiq_tx_hdl_t hdl );

    /*****************************************************************************/
    /** tx_pre_stop_streaming() is called from skiq_stop_tx_streaming() and
     * skiq_stop_tx_streaming_on_1pps() and is responsible for performing steps
     * to prepare the transport link to stop transmitting sample data.
     *
     * @note This function is called BEFORE the FPGA is commanded to stop
     * transmitting samples
     *
     * @note Threads created as part of skiq_tx_transfer_mode_async mode should
     * be destroyed here.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @a hdl should be ignored
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] hdl identifies a handle to halt the transport link for transmission - should be ignored, retained for legacy purposes
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*tx_pre_stop_streaming)( uint64_t xport_uid,
                                      skiq_tx_hdl_t hdl );


    /*****************************************************************************/
    /** tx_stop_streaming() is called from skiq_stop_tx_streaming() and
     * skiq_stop_tx_streaming_on_1pps() and is responsible for performing steps
     * to halt the transport link for transmit sample data.
     *
     * @note This function is called AFTER the FPGA is commanded to stop
     * transmitting samples
     *
     * @note Threads created as part of skiq_tx_transfer_mode_async mode should
     * be destroyed here.
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @a hdl should be ignored
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] hdl identifies a handle to halt the transport link for transmission - should be ignored, retained for legacy purposes
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*tx_stop_streaming)( uint64_t xport_uid,
                                  skiq_tx_hdl_t hdl );

    /*****************************************************************************/
    /** tx_transmit() is called from skiq_tx_transmit() and is responsible for
     * committing sample data to the FPGA over the transport link either in a
     * synchronous or asynchronous manner.
     *
     * @note It is required that if transmit was initialized as @a
     * skiq_tx_transfer_mode_sync, that this function blocks until the transmit
     * data is received by the FPGA over the transport link.  If transmit was
     * initialized as @a skiq_tx_transfer_mode_async, the function immediately
     * accepts the sample block (as buffer space allows).
     *
     * @note xport_uid is the UID that is provided in the init function and the
     * card is "active" (has gone through initialization)
     *
     * @param[in] xport_uid unique ID used to identifer the card at the transport layer
     * @param[in] hdl identifies a handle for sample data transmission
     * @param[in] p_samples reference to sample data of length num_bytes_send (from tx_initialize)
     *
     * @return status where 0=success, anything else is an error.
     */
    int32_t (*tx_transmit)( uint64_t xport_uid,
                            skiq_tx_hdl_t hdl,
                            int32_t *p_samples,
                            void *p_private );

} skiq_xport_tx_functions_t;


/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif  /* __SIDEKIQ_XPORT_TYPES_H__ */
