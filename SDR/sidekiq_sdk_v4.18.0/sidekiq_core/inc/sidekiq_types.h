#ifndef SIDEKIQ_TYPES_H
#define SIDEKIQ_TYPES_H

/*! \file sidekiq_types.h
 * \brief This file contains the public type definitions of the sidekiq_api
 * provided by libsidekiq.
 *
 * <pre>
 * Copyright 2016-2023 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#ifdef __cplusplus
extern "C" {
#endif

/***** INCLUDES *****/

#include <stdint.h>
#if (defined _MSC_VER)
#include <complex.h>
#endif

/***** TYPEDEFS *****/

/***** DEFINES *****/
/* For the MinGW compiler, we'll define this so that symbols are exported for our DLL with the __imp__ prefix */
#ifdef DLL_EXPORT
#pragma message ("DLL_EXPORT is defined")
#endif /* DLL_EXPORT */

#ifdef DLL_EXPORT
#ifdef __cplusplus
//#pragma message ("API type: C++ language exported DLL function")
#define EPIQ_API extern "C" __declspec(dllexport)
#else
//#pragma message ("API type: C language exported DLL function")
#define EPIQ_API extern __declspec(dllexport)
#endif /* __cplusplus */

#else

#if defined(DLL_IMPORT)
//&& !defined(STATICALLY_LINKING)
#pragma message ("API type: C language imported DLL function")
#define EPIQ_API extern "C" __declspec(dllimport)
#else
#define EPIQ_API extern
#endif /* DLL_IMPORT */

#endif /* DLL_EXPORT */

/* @cond IGNORE */
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#if !defined static_assert && !defined __cplusplus
 #if (GCC_VERSION >= 40600)
  #define static_assert(_cond,_desc) _Static_assert(_cond,_desc)
 #else
  #define static_assert(_cond,_desc)
 #endif
#endif
/* @endcond */

/** @brief ::SKIQ_MAX_SAMPLE_SHIFT_NV100 defines the maximum sample shift value
 * used by skiq_write_rx_sample_shift() function.
 * This is currently supported only for NV100 */
#define SKIQ_MAX_SAMPLE_SHIFT_NV100 4

/** @addtogroup cardfunctions
 * @{
 */

/** @brief Number of bytes contained in the serial number (including '\0') */
#define SKIQ_SERIAL_NUM_STRLEN (6)
/** @brief Number of bytes contained in the part number (including '\0') */
#define SKIQ_PART_NUM_STRLEN (7)
/** @brief Number of bytes contained in the revision (including '\0') */
#define SKIQ_REVISION_STRLEN (3)
/** @brief Number of bytes contained in the variant (including '\0') */
#define SKIQ_VARIANT_STRLEN  (3)

/** @brief Maximum number of filters available for a handle

    @see skiq_read_rx_filters_avail
 */
#define SKIQ_MAX_NUM_FILTERS (20)

/** @} */

/** @brief Maximum number of TX packets that can be queued when running in @ref
    skiq_tx_transfer_mode_async

    @ingroup txfunctions
*/
#define SKIQ_MAX_NUM_TX_QUEUED_PACKETS (50)

/** @brief Maximum number of frequencies that can be specified in a hopping list

    @ingroup txfunctions
*/    
#define SKIQ_MAX_NUM_FREQ_HOPS (512) 

/** @brief Defines the memory alignment of a transmit block when allocated using
    ::SKIQ_TX_BLOCK_INITIALIZER, ::SKIQ_TX_BLOCK_INITIALIZER_BY_WORDS,
    ::SKIQ_TX_BLOCK_INITIALIZER_BY_BYTES, skiq_tx_block_allocate() or
    skiq_tx_block_allocate_by_bytes()

    @ingroup txfunctions
*/
#if (!defined SKIQ_TX_BLOCK_MEMORY_ALIGN)
# define SKIQ_TX_BLOCK_MEMORY_ALIGN      4096
#endif

/***** ENUMS *****/

/**
    @brief Tx supports transmitting on System Timestamp or RF Timestamp on
    certain Sidekiq products.
    
    Configuration of which timestamp should be used will generally be needed
    for applications where timing of the transmission is a critical factor.
    
    Call skiq_read_tx_timestamp_base() after enabling/initializing a card to
    determine what the default value for the card is.

    @warning When skiq_tx_rf_timestamp_base is configured the transmission
    of data will not occur until the next clock of the RF timestamp after
    the system timestamp occured. This can make a significant impact on
    transmit timing.

    @since Function added in API @b v4.16.0

    @ingroup txfunctions
    @see skiq_read_tx_timestamp_base
    @see skiq_write_tx_timestamp_base
*/
typedef enum
{
    /**
       @brief The FPGA design compares a ::skiq_tx_block_t's transmit timestamp to the transmit @b
       sample counter which typically increments at the transmit sample rate

       @see skiq_read_curr_tx_timestamp
    */
    skiq_tx_rf_timestamp=0,

    /**
       @brief The FPGA design compares a ::skiq_tx_block_t's transmit timestamp to the transmit @b
       system counter increments at the system clock frequency

       @see skiq_read_curr_sys_timestamp
    */
    skiq_tx_system_timestamp,

} skiq_tx_timestamp_base_t;

/**
    @brief There are several different data flow modes that can be used when
    transmitting data on a Sidekiq Tx interface:

    - tx immediate - I/Q data is transmitted as soon as possible, without
    regard to timestamps.

    - tx with timestamps - I/Q data is queued up by software and/or the FPGA,
    and only transmitted out when the appropriate timestamp has occurred.

    - tx allow late timestamps - this is similar to "tx with timestamps" mode,
    though data with timestamps that have already passed will still be
    transmitted.

    Tx immediate mode is generally used for applications where a
    transmission isn't synchronized to any other time-critical signal,
    and just needs to be sent out as soon as possible.  Note that each
    packet of Tx data transferred to libsidekiq is still queued up in
    a FIFO, so the order of transmission is still preserved though there
    is no reliance on a timestamp to drive any transmission.  It simply
    happens as quickly as possible.

    Tx with timestamps mode is generally used for applications where
    the timing of the transmission is critical (such as in a TDMA
    protocol).

    @ingroup txfunctions
    @see skiq_read_tx_data_flow_mode
    @see skiq_write_tx_data_flow_mode
*/
typedef enum
{
    /**
       @brief I/Q data is transmitted as soon as possible, without regard to timestamps
    */
    skiq_tx_immediate_data_flow_mode=0,

    /**
       @brief I/Q data is queued up by software and/or the FPGA, and only transmitted
       out when the appropriate timestamp has occurred. Data with a timestamp
       that already passed (late) at the time of transmit will be discarded.
    */
    skiq_tx_with_timestamps_data_flow_mode,

    /**
       @brief I/Q data is queued up by software and/or the FPGA, and only transmitted
       out when the appropriate timestamp has occurred. Data with a timestamp
       that already passed (late) at the time of transmit will be transmitted.
    */
    skiq_tx_with_timestamps_allow_late_data_flow_mode,

} skiq_tx_flow_mode_t;

/**
    @brief There are different transfer modes that can be used when transmitting data:

    @note For improved efficieny in transmitting, the @ref skiq_tx_transfer_mode_async is
    recommended.

    @attention @ref skiq_tx_transfer_mode_async may not be available on all
    Sidekiq products, check with the latest release to confirm functionality.
    If there are any questions, please feel free to reach out on the Epiq
    support forum.

    @ingroup txfunctions
    @see skiq_read_tx_transfer_mode
    @see skiq_write_tx_transfer_mode
*/
typedef enum
{
    /**
       @brief This mode transfers packets to the FPGA synchronously.  In this mode, the @ref
       skiq_transmit function will block until the FPGA has received the packet of data.  The FPGA
       FIFO for Tx packets is relatively small (see ::skiq_fpga_tx_fifo_size_t), so when the FIFO is
       full, the @ref skiq_transmit call will block until the packet is transmitted.  The length of
       time to actually transmit the packet depends on the sample rate.
    */
    skiq_tx_transfer_mode_sync=0,

    /**
        @brief This mode transfers packets to the FPGA asynchronously.  In this mode, the @ref
        skiq_transmit function will schedule the packet to be transferred as long as there is enough
        room in the buffer for the packet.  If there is not enough room to store the packet, the
        @ref skiq_transmit function will return immediately with an result of
        ::SKIQ_TX_ASYNC_SEND_QUEUE_FULL.  In order to run in this mode, the OS must support the
        ablility to schedule real-time threads and lock those threads to a specific core.  When
        running in this mode, a callback function can be registered with the @ref
        skiq_register_tx_complete_callback, which will be called once the packet transfer to the
        FPGA has been completed.
    */
    skiq_tx_transfer_mode_async,
} skiq_tx_transfer_mode_t;

/** @brief An Rx interface is typically configured to generate complex
    I/Q samples.  However, there are test cases where it is useful to
    have the I/Q data replaced with an incrementing counter.  This
    is treated as a different "data source", which can be configured
    by the user at run-time before an Rx interface is started.

    @ingroup rxfunctions
    @see skiq_read_rx_data_src
    @see skiq_write_rx_data_src
*/
typedef enum
{
    skiq_data_src_iq=0,
    skiq_data_src_counter
} skiq_data_src_t;


/** @brief An interface is configured to transmit or receive complex
    I/Q samples.  By default samples are received/transmitted as I/Q pairs 
    with 'Q' sample occurring first, followed by the 'I' sample.   Ordering
    can be configured by the user at run-time before an Rx interface is started.

    <pre>
              skiq_iq_order_qi: (default)                 skiq_iq_order_iq:
            -15--------------------------0-       -15--------------------------0-  
            |         12-bit Q0_A1        |       |         12-bit I0_A1        |
  index 0   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |
            -------------------------------       -------------------------------
            |         12-bit I0_A1        |       |         12-bit Q0_A1        |
  index 1   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |
            -------------------------------       -------------------------------
            |         12-bit Q1_A1        |       |         12-bit I1_A1        |
  index 2   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |
            -------------------------------       -------------------------------
            |         12-bit I1_A1        |       |         12-bit Q1_A1        |
  index 3   | (sign extended to 16 bits)  |       | (sign extended to 16 bits)  |
            -------------------------------       -------------------------------
            |             ...             |       |             ...             |
            -------------------------------       -------------------------------
            |             ...             |       |             ...             |
            -15--------------------------0-       -15--------------------------0-
    </pre>

    @ingroup cardfunctions
    @see skiq_read_iq_order_mode
    @see skiq_write_iq_order_mode

*/
typedef enum
{
    skiq_iq_order_qi=0,
    skiq_iq_order_iq
} skiq_iq_order_t;



/** @brief Sidekiq supports three different receive stream modes that change the
    relative IQ sample block latency (::skiq_rx_block_t) between the FPGA and
    host CPU.  The ::skiq_rx_stream_mode_high_tput setting is business as usual
    and provides the same receive latency that exists in the previous releases
    of libsidekiq.  The ::skiq_rx_stream_mode_low_latency setting provides a
    smaller block of IQ samples from skiq_receive() more often and effectively
    lowers the latency from RF reception to host CPU.  The ::skiq_rx_stream_mode_balanced
    is a compromise between the high_tput and low_latency modes which has a reduced
    overall throughput relative to high_tput but results in a larger number of samples
    per packet than the low_latency mode.

    @attention Since ::skiq_rx_stream_mode_low_latency setting delivers smaller
    blocks of IQ samples (with metadata) more often, it is only effective up to
    8-10Msps (~3Msps on 32-bit ARM systems).  The user will encounter timestamp
    gaps if using this mode in conjunction with sample rates above this
    limitation.

    @since definition added in @b v4.6.0, skiq_rx_stream_mode_balanced added in @b v4.7.0
    skiq_rx_stream_mode_low_latency requires FPGA @b v3.9.0 or later

    @ingroup rxfunctions
    @see skiq_read_rx_stream_mode
    @see skiq_write_rx_stream_mode
*/
typedef enum
{
    skiq_rx_stream_mode_high_tput=0,
    skiq_rx_stream_mode_low_latency,
    skiq_rx_stream_mode_balanced,
    /* add all new stream modes above this line */
    skiq_rx_stream_mode_end,
} skiq_rx_stream_mode_t;


/** @brief A trigger source may be specified when starting or stopping multiple handle streaming.
    The trigger may be specified as 'immediate' and happens without any synchronization between
    handles.  It may also be specified as '1PPS' so all specified handles would start streaming
    synchronized on a PPS edge.  If the FPGA bitstream supports it (>= 3.11.0), a value of
    ::skiq_trigger_src_synced causes all specified handles to start or stop streaming immediately,
    but also synchronized (RF timestamps are aligned).  A similar application may be used when
    stopping multiple handles from streaming.

    @note Presently limited to receive handles

    @since Definition added in @b v4.5.0, ::skiq_trigger_src_synced added in @b v4.8.0

    @ingroup rxfunctions

    @see skiq_start_rx_streaming_multi_on_trigger
    @see skiq_stop_rx_streaming_multi_on_trigger
*/
typedef enum
{
    skiq_trigger_src_immediate=0,
    skiq_trigger_src_1pps,
    skiq_trigger_src_synced,
} skiq_trigger_src_t;


/** @brief Sidekiq supports several Rx interface handles.  The ::skiq_rx_hdl_t
    enum is used to define the different Rx interface handles.

    @ingroup rxfunctions
*/
typedef enum
{
    skiq_rx_hdl_A1=0,
    skiq_rx_hdl_A2=1,
    skiq_rx_hdl_B1=2,
    skiq_rx_hdl_B2=3,
    skiq_rx_hdl_C1=4,
    skiq_rx_hdl_D1=5,
    skiq_rx_hdl_end
} skiq_rx_hdl_t;

/** @brief Sidekiq supports a single Tx interface handles.  The ::skiq_tx_hdl_t
    enum is used to define the Tx interface handle.

    @ingroup txfunctions
*/
typedef enum
{
    skiq_tx_hdl_A1=0,
    skiq_tx_hdl_A2=1,
    skiq_tx_hdl_B1=2,
    skiq_tx_hdl_B2=3,
    skiq_tx_hdl_end
} skiq_tx_hdl_t;

/** @brief Each RF path in Sidekiq has integrated filter options that can be
    software-controlled.  By default, the filter is automatically selected based
    on the requested LO frequency.  The ::skiq_filt_t enum is used to specify a
    filter selection.  Note: not all filter options are available for hardware
    variants.  Available filter variants can be queried with @ref
    skiq_read_rx_filters_avail.

    @ingroup cardfunctions
    @see skiq_read_rx_filters_avail
    @see skiq_read_filter_range
    @see skiq_read_rx_preselect_filter_path
    @see skiq_read_tx_filters_avail
*/
typedef enum
{
    skiq_filt_invalid = -1,

    skiq_filt_0_to_3000_MHz=0,
    skiq_filt_3000_to_6000_MHz,

    skiq_filt_0_to_440MHz,
    skiq_filt_440_to_6000MHz,

    skiq_filt_440_to_580MHz,
    skiq_filt_580_to_810MHz,
    skiq_filt_810_to_1170MHz,
    skiq_filt_1170_to_1695MHz,
    skiq_filt_1695_to_2540MHz,
    skiq_filt_2540_to_3840MHz,
    skiq_filt_3840_to_6000MHz,

    skiq_filt_0_to_300MHz,
    skiq_filt_300_to_6000MHz,

    skiq_filt_50_to_435MHz,
    skiq_filt_435_to_910MHz,
    skiq_filt_910_to_1950MHz,
    skiq_filt_1950_to_6000MHz,

    skiq_filt_0_to_6000MHz,
    skiq_filt_390_to_620MHz,
    skiq_filt_540_to_850MHz,
    skiq_filt_770_to_1210MHz,
    skiq_filt_1130_to_1760MHz,
    skiq_filt_1680_to_2580MHz,
    skiq_filt_2500_to_3880MHz,
    skiq_filt_3800_to_6000MHz,

    skiq_filt_47_to_135MHz,
    skiq_filt_135_to_145MHz,
    skiq_filt_145_to_150MHz,
    skiq_filt_150_to_162MHz,
    skiq_filt_162_to_175MHz,
    skiq_filt_175_to_190MHz,
    skiq_filt_190_to_212MHz,
    skiq_filt_212_to_230MHz,
    skiq_filt_230_to_280MHz,
    skiq_filt_280_to_366MHz,
    skiq_filt_366_to_475MHz,
    skiq_filt_475_to_625MHz,
    skiq_filt_625_to_800MHz,
    skiq_filt_800_to_1175MHz,
    skiq_filt_1175_to_1500MHz,
    skiq_filt_1500_to_2100MHz,
    skiq_filt_2100_to_2775MHz,
    skiq_filt_2775_to_3360MHz,
    skiq_filt_3360_to_4600MHz,
    skiq_filt_4600_to_6000MHz,

    skiq_filt_30_to_450MHz,
    skiq_filt_450_to_600MHz,
    skiq_filt_600_to_800MHz,
    skiq_filt_800_to_1200MHz,
    skiq_filt_1200_to_1700MHz,
    skiq_filt_1700_to_2700MHz,
    skiq_filt_2700_to_3600MHz,
    skiq_filt_3600_to_6000MHz,

    skiq_filt_max
} skiq_filt_t;

/** @brief String representation of skiq_filt_t enumeration

    @ingroup cardfunctions
    @see skiq_filt_t
 */
EPIQ_API const char* SKIQ_FILT_STRINGS[skiq_filt_max];

/** @brief String representation of skiq_rx_stream_mode_t

    @ingroup rxfunctions
    @see skiq_rx_stream_mode_t
 */
EPIQ_API const char* SKIQ_RX_STREAM_MODE_STRINGS[skiq_rx_stream_mode_end];

/** @addtogroup cardfunctions
 * @{
 */

/** @brief String representation of the Sidekiq mPCIe 001 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_MPCIE_001;
/** @brief String representation of the Sidekiq mPCIe 002 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_MPCIE_002;
/** @brief String representation of the Sidekiq m.2 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_M2;
/** @brief String representation of the Sidekiq X2 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_X2;
/** @brief String representation of the Sidekiq Z2 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_Z2;
/** @brief String representation of the Sidekiq X4 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_X4;
/** @brief String representation of the Sidekiq M.2 2280 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_M2_2280;
/** @brief String representation of the Sidekiq Z2+ part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_Z2P;
/** @brief String representation of the Sidekiq Z3u part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_Z3U;
/** @brief String representation of the Sidekiq NV100 part */
EPIQ_API const char* SKIQ_PART_NUM_STRING_NV100;

/** @} */

/** @brief Rx gain can be controlled either manually or automatically.  The
    ::skiq_rx_gain_t enum is used to specify the mode of gain control.

    @ingroup rxfunctions
    @see skiq_read_rx_gain_mode
    @see skiq_write_rx_gain_mode
*/
typedef enum
{
    skiq_rx_gain_manual = 0,
    skiq_rx_gain_auto
} skiq_rx_gain_t;

/** @brief Rx attenuation mode

    @attention This is only supported for <a
    href="https://epiqsolutions.com/sidekiq-x2/">Sidekiq X2</a>.

    @ingroup rxfunctions
    @see skiq_read_rx_attenuation
    @see skiq_read_rx_attenuation_mode
    @see skiq_write_rx_attenuation
    @see skiq_write_rx_attenuation_mode
 */
typedef enum
{
    /**
       @brief User is responsible for writing Rx attenuation value.
    */
    skiq_rx_attenuation_mode_manual=0,

    /**
       @brief Software automatically configures attenuation to optimize for the best
       noise figure across all frequencies.
    */
    skiq_rx_attenuation_mode_noise_figure,

    /**
       @brief Software automatically configures attenuation to optimize for equal
       gain response across all frequencies.
    */
    skiq_rx_attenuation_mode_normalized,

} skiq_rx_attenuation_mode_t;

/** @brief Sidekiq can run either in single Rx or dual channel Rx mode.
    The ::skiq_chan_mode_t enum is used to specify the Rx/Tx channel mode.

    @warning Dual channel mode is only supported with SKIQ-001 hardware.

    @ingroup rxfunctions
    @see skiq_read_chan_mode
    @see skiq_write_chan_mode
*/
typedef enum
{
    skiq_chan_mode_single=0,  /**< only A1 is enabled for Rx/Tx */
    skiq_chan_mode_dual       /**< both A1 and A2 are enabled for Rx/Tx */
} skiq_chan_mode_t;

/** @brief Sidekiq Part

    @ingroup cardfunctions
    @see skiq_part_string
    @see skiq_hardware_vers_string
    @see skiq_product_vers_string
 */
typedef enum
{
    skiq_mpcie=0,
    skiq_m2,
    skiq_x2,
    skiq_z2,
    skiq_x4,
    skiq_m2_2280,
    skiq_z2p,
    skiq_z3u,
    skiq_nv100,
    // add all parts above this line
    skiq_part_invalid
} skiq_part_t;

/** @brief Sidekiq Part Information

    @ingroup cardfunctions
 */
typedef struct
{
    char number_string[SKIQ_PART_NUM_STRLEN];
    char revision_string[SKIQ_REVISION_STRLEN];
    char variant_string[SKIQ_VARIANT_STRLEN];
} skiq_part_info_t;

/** @brief Sidekiq has multiple revisions of the hardware.  The
    ::skiq_hw_vers_t enum defines the revisions that have been deployed.

    @note   These constants are NOT used in the EEPROM; hardware.c maps skiq_eeprom_hw_vers_
            to this constant. You can safely change these constants without worrying about
            backwards compatability (in terms of EE).

    @deprecated
 */
typedef enum
{
    /* mPCIe hardware versions */
    skiq_hw_vers_mpcie_a=1,
    skiq_hw_vers_mpcie_b=2,
    skiq_hw_vers_mpcie_c=3,
    skiq_hw_vers_mpcie_d=4,
    skiq_hw_vers_mpcie_e=5,
    /* m.2 hardware versions */
    skiq_hw_vers_m2_b=6,
    skiq_hw_vers_m2_c=7,
    skiq_hw_vers_m2_d=8,
    /* add any new version above this line */
    skiq_hw_vers_reserved,
    /*
        newer mPCIe cards masquerade as legacy cards; this enum value should point to
        the version they masquerade as
    */
    skiq_hw_vers_mpcie_masquerade = skiq_hw_vers_mpcie_c,
    /*
        newer m.2 cards masquerade as legacy cards; this enum values should point to
        the version they masquerade as
    */
    skiq_hw_vers_m2_masquerade = skiq_hw_vers_m2_c,
    skiq_hw_vers_invalid=0xFFF,
} skiq_hw_vers_t;

/** @brief There are multiple products controllable through libsidekiq.
    The ::skiq_product_t enum defines the Sidekiq product types.

    @deprecated
*/
typedef enum
{
    /* mPCIe Sidekiq products */
    skiq_product_mpcie_001=0,    /**< supports RxA1/A2 and TxA1 */
    skiq_product_mpcie_002=1,    /**< supports RxA1 and TxA1 only */
    /* m.2 Sidekiq products */
    skiq_product_m2_001=2,       /**< supports RxA1/A2 and TxA1/A2 */
    skiq_product_m2_002=3,       /**< supports RxA1 and TxA1 */
    /* add any new version above this line */
    skiq_product_reserved,
    skiq_product_invalid=0xF,
} skiq_product_t;

/** @brief Rx FIR filter gain settings, applied to the Rx FIR used in the Rx
    channel bandwidth configuration

    @ingroup rxfunctions
    @see skiq_read_rx_fir_gain
    @see skiq_write_rx_fir_gain
*/
typedef enum
{
    skiq_rx_fir_gain_neg_12=3,  /**< designates a receive FIR gain of -12 dB */
    skiq_rx_fir_gain_neg_6=2,   /**< designates a receive FIR gain of -6 dB */
    skiq_rx_fir_gain_0=1,       /**< designates a receive FIR gain of 0 dB */
    skiq_rx_fir_gain_6=0        /**< designates a receive FIR gain of +6 dB */
} skiq_rx_fir_gain_t;

/** @brief Tx FIR filter gain settings, applied to the Tx FIR used in the Tx
    channel bandwidth configuration

    @ingroup txfunctions
    @see skiq_read_tx_fir_gain
    @see skiq_write_tx_fir_gain

*/
typedef enum
{
    skiq_tx_fir_gain_neg_6=1,   /**< designates a Tx FIR gain of -6 dB */
    skiq_tx_fir_gain_0=0        /**< designates a Tx FIR gain of 0 dB */
} skiq_tx_fir_gain_t;

/** @brief Reference clock setting.

    @warning This setting is @b NOT software configurable for Sidekiq mPCIe ::skiq_hw_vers_mpcie_b

    @ingroup cardfunctions
    @see skiq_read_ext_ref_clock_freq
    @see skiq_read_ref_clock_select
*/
typedef enum
{
    skiq_ref_clock_internal=0,  /**< use the default internal reference clock */
    skiq_ref_clock_external,    /**< an external, user-accessible reference clock */
    skiq_ref_clock_host,        /**< reference clock originated from host
                                   @since enum added in @b v4.5.0 */
    skiq_ref_clock_carrier_edge,    /**< reference clock originated from carrier */
    skiq_ref_clock_invalid,
} skiq_ref_clock_select_t;

/** @brief FPGA Tx FIFO Size.  The FIFO size is the number of packets
    the FPGA can hold prior to actually transmitting the data

    @ingroup fpgafunctions
    @see skiq_read_fpga_tx_fifo_size
*/
typedef enum
{
    skiq_fpga_tx_fifo_size_unknown  = 0,  /**< FPGA versions prior to 2.0 did
                                             not support reporting FIFO size */
    skiq_fpga_tx_fifo_size_4k       = 1,  /**< 4k 32-bit words deep */
    skiq_fpga_tx_fifo_size_8k       = 2,  /**< 8k 32-bit words deep */
    skiq_fpga_tx_fifo_size_16k      = 3,  /**< 16k 32-bit words deep */
    skiq_fpga_tx_fifo_size_32k      = 4,  /**< 32k 32-bit words deep */
    skiq_fpga_tx_fifo_size_64k      = 5   /**< 64k 32-bit words deep */
} skiq_fpga_tx_fifo_size_t;

/** @brief Possible return codes from @ref skiq_receive

    @ingroup rxfunctions
 */
typedef enum
{
    skiq_rx_status_success                = 0,   /**< new data is available */
    skiq_rx_status_no_data                = -1,  /**< no new data is ready */
    skiq_rx_status_error_generic          = -6,  /**< a generic error was
                                                      encountered when trying to
                                                      receive */
    skiq_rx_status_error_overrun          = -11, /**< an overrun occurred. An
                                                      overrun occurs when the
                                                      FPGA streams data faster
                                                      than software retrieves
                                                      it, resulting in the data
                                                      not yet retrieved by
                                                      software to be
                                                      overwritten.  This
                                                      condition is reset upon
                                                      each skiq_receive call. */
    skiq_rx_status_error_packet_malformed = -12, /**< packet was incorrectly structured/formatted */
    skiq_rx_status_error_card_not_active = -19,  /**< requested card not active in current
                                                      session */
    skiq_rx_status_error_not_streaming = -29,    /**< no receive handles streaming, cannot receive a
                                                      block */
} skiq_rx_status_t;

/** @brief RF port configuration options of Sidekiq

    @ingroup rfportfunctions
    @see skiq_read_rf_port_config_avail
    @see skiq_read_rf_port_config
    @see skiq_write_rf_port_config
    @see skiq_read_rf_port_operation
    @see skiq_write_rf_port_operation
 */
typedef enum
{
    skiq_rf_port_config_fixed = 0,  /**< a single RF port can be used for either
                                         Rx OR Tx but not both */
    skiq_rf_port_config_tdd,        /**< TDD: a single RF port can switch
                                         between Rx AND Tx, duplexed over time
                                         with the skiq_write_rf_port_operation()
                                         API
                                         @deprecated use @ref skiq_rf_port_config_trx */
    skiq_rf_port_config_trx,       /**< TRx ports can be used for either receive or transmit,
                                       duplexed over time with the
                                       skiq_write_rf_port_operation() API */
    skiq_rf_port_config_invalid,
} skiq_rf_port_config_t;

/** @brief RF ports of Sidekiq

    @ingroup rfportfunctions
    @see skiq_read_rx_rf_ports_avail_for_hdl
    @see skiq_read_rx_rf_port_for_hdl
    @see skiq_write_rx_rf_port_for_hdl
    @see skiq_read_tx_rf_ports_avail_for_hdl
    @see skiq_read_tx_rf_port_for_hdl
    @see skiq_read_tx_rf_port_for_hdl
*/
typedef enum
{
    skiq_rf_port_unknown=-1,

    skiq_rf_port_J1 = 0,  /**< J1 */
    skiq_rf_port_J2,      /**< J2 */
    skiq_rf_port_J3,      /**< J3 */
    skiq_rf_port_J4,      /**< J4 */
    skiq_rf_port_J5,      /**< J5 */
    skiq_rf_port_J6,      /**< J6 */
    skiq_rf_port_J7,      /**< J7 */
    skiq_rf_port_J300,    /**< J300 */
    skiq_rf_port_Jxxx_RX1, /**< labeled Rx1 */
    skiq_rf_port_Jxxx_TX1RX2, /**< labeled Tx1/Rx2 */
    skiq_rf_port_J8,     /**< J8 */

    skiq_rf_port_max,
} skiq_rf_port_t;

/** @brief TX Quadrature Calibration Mode

    @ingroup txfunctions
    @see skiq_read_tx_quadcal_mode
    @see skiq_write_tx_quadcal_mode
    @see skiq_run_tx_quadcal
*/
typedef enum
{
    skiq_tx_quadcal_mode_auto=0,  /**< automatically run TX quadrature algorithm */
    skiq_tx_quadcal_mode_manual,  /**< do not automatically run the TX quadrature algorithm */
} skiq_tx_quadcal_mode_t;

/** @brief RX Calibration Mode

    @ingroup rxfunctions
    @see skiq_read_rx_cal_mode
    @see skiq_write_rx_cal_mode
    @see skiq_run_rx_cal
*/
typedef enum
{
    skiq_rx_cal_mode_auto=0,  /**< automatically run RX calibration algorithms */
    skiq_rx_cal_mode_manual,  /**< do not automatically run the RX algorithms */
} skiq_rx_cal_mode_t;

/** @brief RX Calibration Types

    @ingroup rxfunctions
    @see skiq_write_rx_cal_type_mask
    @see skiq_read_rx_cal_type_mask
    @see skiq_read_rx_cal_types_avail
*/
typedef enum
{
    skiq_rx_cal_type_none = 0x00000000,        /***< no calibration algorithms */
    skiq_rx_cal_type_dc_offset = 0x00000001,   /***< DC offset calibration */
    skiq_rx_cal_type_quadrature = 0x00000002,  /***< RX quadrature calibration */
} skiq_rx_cal_type_t;

/** @brief Source of 1PPS.  Note that not all products support all configurations.

    @ingroup ppsfunctions
    @see skiq_read_1pps_source
    @see skiq_write_1pps_source
*/
typedef enum
{
    skiq_1pps_source_unavailable=-1,
    skiq_1pps_source_external=0,  /**< 1PPS source from external connector */
    skiq_1pps_source_host=1,      /**< 1PPS source from host connector */
} skiq_1pps_source_t;

/** @brief Frequency Tune mode.  Note that not all products support all configurations.

    @ingroup hopfunctions
    @see skiq_write_rx_freq_tune_mode
    @see skiq_read_rx_freq_tune_mode
    @see skiq_write_tx_freq_tune_mode
    @see skiq_read_tx_freq_tune_mode

    @see skiq_write_rx_freq_hop_list
    @see skiq_read_rx_freq_hop_list
    @see skiq_write_tx_freq_hop_list
    @see skiq_read_tx_freq_hop_list

    @see skiq_perform_rx_freq_hop
    @see skiq_perform_tx_freq_hop

    @see skiq_read_curr_rx_freq_hop
    @see skiq_read_curr_tx_freq_hop
*/
typedef enum
{
    /**
       @brief LO frequency adjusted with either skiq_write_rx_LO_freq() or skiq_write_tx_LO_freq() depending on the handle in use
    */
    skiq_freq_tune_mode_standard=0,

    /**
       @brief hop list index used to control LO, tuning happens ASAP
    */
    skiq_freq_tune_mode_hop_immediate,

    /**
       @brief hop list index used to control LO, tuning initiated on timestamp
    */
    skiq_freq_tune_mode_hop_on_timestamp,
} skiq_freq_tune_mode_t;


typedef enum
{
    /** @brief Applies to those Sidekiq devices that are not FMC form factor */
    skiq_fmc_carrier_not_applicable,

    /** @brief Queries of the Sidekiq device failed to find a supported FMC carrier */
    skiq_fmc_carrier_unknown,

    /**
       @brief Annapolis Micro Systems WILDSTAR WB3XZD
       (https://www.annapmicro.com/products/wildstar-ultrakvp-zp-dram-3u-openvpx/)
    */
    skiq_fmc_carrier_ams_wb3xzd,

    /** @brief HiTech Global K800 (http://www.hitechglobal.com/Boards/Kintex-UltraScale.htm) */
    skiq_fmc_carrier_htg_k800,

    /**
       @brief Annapolis Micro Systems WILDSTAR WB3XBM
       (https://www.annapmicro.com/products/wildstar-3xbm-3u-openvpx-fpga-processor-wb3xbm/)
    */
    skiq_fmc_carrier_ams_wb3xbm,

    /**
       @brief HiTech Global K810
       (http://www.hitechglobal.com/Boards/KintexUltraScale_COMExpress.htm)
    */
    skiq_fmc_carrier_htg_k810,

} skiq_fmc_carrier_t;


typedef enum
{
    /** @brief Queries of the Sidekiq device failed to find a supported FPGA device */
    skiq_fpga_device_unknown,

    /** @brief Xilinx Spartan-6 LXT */
    skiq_fpga_device_xc6slx45t,

    /** @brief Xilinx Artix-7 50T */
    skiq_fpga_device_xc7a50t,

    /** @brief Xilinx Zynq-7000 */
    skiq_fpga_device_xc7z010,

    /** @brief Xilinx Kintex Ultrascale 60 */
    skiq_fpga_device_xcku060,

    /** @brief Xilinx Kintex Ultrascale 115 */
    skiq_fpga_device_xcku115,

    /** @brief Xilinx Zynq Ultrascale+ ZU3 */
    skiq_fpga_device_xczu3eg,

} skiq_fpga_device_t;

typedef enum
{
    skiq_rfic_pin_control_sw=0,
    skiq_rfic_pin_control_fpga_gpio,
} skiq_rfic_pin_mode_t;

/**
    @brief  The status of GPSDO support on a given card / FPGA bitstream

    @since  Definition added in @b v4.15.0

    @ingroup    gpsdofunctions
*/
typedef enum
{
    /** @brief  The GPSDO support is unknown or cannot be read for this card */
    skiq_gpsdo_support_unknown = 0,
    /** @brief  The card and FPGA bitstream support GPSDO functionality */
    skiq_gpsdo_support_is_supported,
    /** @brief  The Sidekiq card does not support GPSDO functionality */
    skiq_gpsdo_support_card_not_supported,
    /** @brief  The loaded FPGA bitstream does not support GPSDO functionality */
    skiq_gpsdo_support_fpga_not_supported,
    /**
        @brief  The card and/or FPGA bitstream are capable of GPSDO functionality but have indicated
                that it is not currently supported
    */
    skiq_gpsdo_support_not_supported,
} skiq_gpsdo_support_t;

/** @brief String representation of ::skiq_rf_port_t enumeration

    @ingroup rfportfunctions
    @see skiq_rf_port_t
 */
EPIQ_API const char* SKIQ_RF_PORT_STRINGS[skiq_rf_port_max];

/*****************************************************************************/
/** Sidekiq Transmit Block static initializer, user specifies the number of
    desired bytes.

    @hideinitializer
    @note Sidekiq Transmit Blocks statically allocated should be typecast to a
    @ref skiq_tx_block_t reference when calling @ref skiq_transmit to avoid
    compiler warnings

    @since MACRO added in @b v4.0.0

    @ingroup txfunctions

    @param[in] var_name desired variable name
    @param[in] data_size_in_bytes desired payload size (bytes)
 */
#define SKIQ_TX_BLOCK_INITIALIZER_BY_BYTES(var_name, data_size_in_bytes) \
    _SKIQ_TX_BLOCK_INITIALIZER(var_name, data_size_in_bytes)

/*****************************************************************************/
/** Sidekiq Transmit Block static initializer, user specifies the number of
    desired words

    @hideinitializer
    @note Sidekiq Transmit Blocks statically allocated should be typecast to a
    @ref skiq_tx_block_t reference when calling @ref skiq_transmit to avoid
    compiler warnings

    @since MACRO added in @b v4.0.0

    @ingroup txfunctions

    @param[in] var_name desired variable name
    @param[in] data_size_in_words desired payload size (words)
*/
#define SKIQ_TX_BLOCK_INITIALIZER_BY_WORDS(var_name, data_size_in_words) \
    _SKIQ_TX_BLOCK_INITIALIZER(var_name, (data_size_in_words) * 4)

/*****************************************************************************/
/** Sidekiq Transmit Block static initializer, allocates the maximum transmit
    block size

    @hideinitializer
    @note Sidekiq Transmit Blocks statically allocated should be typecast to a
    @ref skiq_tx_block_t reference when calling @ref skiq_transmit to avoid
    compiler warnings

    @since MACRO added in @b v4.0.0

    @ingroup txfunctions

    @param[in] var_name desired variable name
*/
#define SKIQ_TX_BLOCK_INITIALIZER(var_name) \
    SKIQ_TX_BLOCK_INITIALIZER_IN_WORDS(var_name, SKIQ_MAX_TX_BLOCK_SIZE_IN_WORDS)

/*****************************************************************************/
/* creates an anonymous structure that is the 'right size' and matches the
 * structure of skiq_tx_block_t */
/* @cond IGNORE */
#define _SKIQ_TX_BLOCK_INITIALIZER(_var, _data_size_in_bytes)           \
    struct __attribute__((__packed__, aligned(SKIQ_TX_BLOCK_MEMORY_ALIGN))) { \
        uint32_t miscHigh;                                              \
        uint32_t miscLow;                                               \
        uint64_t timestamp;                                             \
        int16_t data[(_data_size_in_bytes) / 2];                        \
    } _var;                                                             \
    static_assert(_data_size_in_bytes <= (4 * SKIQ_MAX_TX_BLOCK_SIZE_IN_WORDS), "data size cannot exceed SKIQ_MAX_TX_BLOCK_SIZE_IN_WORDS")
/* @endcond */

/*****************************************************************************/
/** @brief Sidekiq Transmit Block type definition for use with @ref
    skiq_transmit and @ref skiq_tx_callback_t

    @since Type definition added in @b v4.0.0

    @ingroup txfunctions
    @see skiq_transmit
    @see skiq_tx_callback_t
*/
#ifndef WIN32
typedef struct /* @cond IGNORE */ __attribute__((__packed__, aligned(SKIQ_TX_BLOCK_MEMORY_ALIGN))) /* @endcond */
#else
#pragma pack(push,1)
typedef struct
#endif /* WIN32 */
{
    /* @{ */
    uint32_t miscHigh;          /**< @brief high word of metadata (32 bits) (unused) */
    uint32_t miscLow;           /**< @brief low word of metadata (32 bits) (unused) */
    uint64_t timestamp;         /**< @brief RF timestamp (64 bits) for transmitted block when transmit flow mode is @ref skiq_tx_with_timestamps_data_flow_mode */
    int16_t data[];             /**< @brief array of unpacked IQ samples (16 bits per I or Q value) */
    /* @} */
} skiq_tx_block_t;
#ifdef WIN32
#pragma pack(pop)
#endif /* WIN32 */

/*****************************************************************************/
/** @brief Transmit callback function type definition

    @ingroup txfunctions
    @see skiq_register_tx_complete_callback
 */
typedef void (*skiq_tx_callback_t)(int32_t status, skiq_tx_block_t *p_block, void *p_user);

/*****************************************************************************/
/** @brief Transmit enabled callback function type definition where card is the
    Sidekiq card whose transmitter has been enabled and status is the error code
    associated with enabling the transmitter (0 is success)

    @ingroup txfunctions
    @see skiq_register_tx_enabled_callback
*/
typedef void (*skiq_tx_ena_callback_t)(uint8_t card, int32_t status);


/*****************************************************************************/
/** @struct skiq_rx_block_t

    @brief Sidekiq Receive Block type definition for use with @ref skiq_receive

    @since Type definition added in @b v4.0.0

    @ingroup rxfunctions
    @see skiq_receive
*/
#ifndef WIN32
typedef struct /* @cond IGNORE */ __attribute__((__packed__, aligned(8))) /* @endcond */
#else
#pragma pack(push,1)
typedef struct
#endif /* WIN32 */
{
    /* @{ */
    volatile uint64_t rf_timestamp;           /**< @brief RF timestamp (64 bits) associated with the
                                               * received sample block */
    volatile uint64_t sys_timestamp;          /**< @brief System timestamp (64 bits) associated with
                                               * the received sample block */
    union
    {
        /* @cond INTERNAL */
        volatile uint64_t __raw_meta;             /**< for internal use only */
        /* @endcond */
        struct
        {
            /* the structure is defined bitwise starting with the LSb */
            volatile uint64_t hdl:6;          /**< @brief Receive handle (6 bits) indicating the
                                               * receive handle associated with the received sample
                                               * block */
            volatile uint64_t overload:1;     /**< @brief RF Overload (1 bit) indicating whether or
                                               * not the RF input was overloaded for the received
                                               * sample block */
            volatile uint64_t rfic_control:8; /**< @brief RFIC control word (8 bits) carries
                                               * metadata from the RFIC, typically the receive gain
                                               * index @see skiq_write_rfic_control_output_config */
            volatile uint64_t id:8;           /**< @brief Channel ID (8 bits) used by channelizer */
            volatile uint64_t system_meta:6;  /**< @brief System metadata (6 bits) (unused /
                                               * reserved) */
            volatile uint64_t version:3;      /**< @brief Packet version field (3 bits) */
            volatile uint64_t user_meta:32;   /**< @brief User metadata (32 bits) typically
                                               * populated by a custom FPGA build */
        };
    };
    volatile int16_t data[];                  /**< @brief array of unpacked IQ samples (16 bits per
                                               * I or Q value).  Q0 is @a data[0], I0 is @a data[1],
                                               * Q1 is @a data[2], I1 is @a data[3], and so on. */
    /* @} */
} skiq_rx_block_t;
#ifdef WIN32
#pragma pack(pop)
#endif /* WIN32 */


/*****************************************************************************/
/** Sidekiq Receive Block static initializer, user specifies the number of
    desired bytes.

    @hideinitializer
    @note Sidekiq Receive Blocks statically allocated should be typecast to a
    @ref skiq_rx_block_t reference when calling @ref skiq_receive to avoid
    compiler warnings

    @since MACRO added in @b v4.0.0

    @ingroup rxfunctions

    @param[in] var_name desired variable name
    @param[in] data_size_in_bytes desired payload size (bytes)
 */
#define SKIQ_RX_BLOCK_INITIALIZER_BY_BYTES(var_name, data_size_in_bytes) \
    _SKIQ_RX_BLOCK_INITIALIZER(var_name, data_size_in_bytes)


/*****************************************************************************/
/** Sidekiq Receive Block static initializer, user specifies the number of
    desired words

    @hideinitializer
    @note Sidekiq Receive Blocks statically allocated should be typecast to a
    @ref skiq_rx_block_t reference when calling @ref skiq_receive to avoid
    compiler warnings

    @since MACRO added in @b v4.0.0

    @ingroup rxfunctions

    @param[in] var_name desired variable name
    @param[in] data_size_in_words desired payload size (words)
*/
#define SKIQ_RX_BLOCK_INITIALIZER_BY_WORDS(var_name, data_size_in_words) \
    _SKIQ_RX_BLOCK_INITIALIZER(var_name, (data_size_in_words) * 4)


/*****************************************************************************/
/** Sidekiq Receive Block static initializer, allocates the maximum receive
    block size

    @hideinitializer
    @note Sidekiq Receive Blocks statically allocated should be typecast to a
    @ref skiq_rx_block_t reference when calling @ref skiq_receive to avoid
    compiler warnings

    @since MACRO added in @b v4.0.0

    @ingroup rxfunctions

    @param[in] var_name desired variable name
*/
#define SKIQ_RX_BLOCK_INITIALIZER(var_name) \
    SKIQ_RX_BLOCK_INITIALIZER_IN_WORDS(var_name, SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS)


/*****************************************************************************/
/* creates an anonymous structure that is the 'right size' and matches the
 * structure of @ref skiq_rx_block_t */
/* @cond IGNORE */
#define _SKIQ_RX_BLOCK_INITIALIZER(_var, _data_size_in_bytes)           \
    struct __attribute__((__packed__, aligned(8))) {                    \
        uint64_t rf_timestamp;                                          \
        uint64_t sys_timestamp;                                         \
        union {                                                         \
            uint64_t __raw_meta;                                        \
            struct {                                                    \
                uint64_t hdl:2;                                         \
                uint64_t overload:1;                                    \
                uint64_t rfic_control:8;                                \
                uint64_t system_meta:21;                                \
                uint64_t user_meta:32;                                  \
            };                                                          \
        };                                                              \
        int16_t data[(_data_size_in_bytes) / 2];                        \
    } _var;                                                             \
    static_assert(_data_size_in_bytes <= SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES, "data size cannot exceed SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES")
/* @endcond */

/**************************************************************************************************/
/** A 'float _Complex' type definition that provides a cross-platform complex variable type.  Under
    Linux and MinGW64, the ::float_complex_t definition resolves to 'float complex' while under
    Visual Studio, it resolves to '_Fcomplex'
 */
#if (defined _C_COMPLEX_T)
typedef _Fcomplex float_complex_t;
#else
typedef float _Complex float_complex_t;
#endif

/**************************************************************************************************/
/** Sidekiq logging levels.  The lower the numeric value, the higher the severity level.
*/
#if (!defined __MINGW32__ && !defined _MSC_VER)
#include <syslog.h>
#define SKIQ_LOG_DEBUG      ((int)LOG_DEBUG)
#define SKIQ_LOG_INFO       ((int)LOG_INFO)
#define SKIQ_LOG_WARNING    ((int)LOG_WARNING)
#define SKIQ_LOG_ERROR      ((int)LOG_ERR)
#else
typedef enum _logLevels {
    LOG_ALL,
    LOG_FINEST = LOG_ALL,
    LOG_FINE,
    LOG_NORMAL,
    LOG_INFO = LOG_NORMAL,
    LOG_WARNING,
    LOG_ERROR,
    LOG_OFF,
    LOG_NONE = LOG_OFF
} LOG_level_t;
#define SKIQ_LOG_DEBUG      ((LOG_level_t)LOG_FINE)
#define SKIQ_LOG_INFO       ((LOG_level_t)LOG_INFO)
#define SKIQ_LOG_WARNING    ((LOG_level_t)LOG_WARNING)
#define SKIQ_LOG_ERROR      ((LOG_level_t)LOG_ERROR)
#endif /* __MINGW32__ || _MSC_VER */

#ifdef __cplusplus
}
#endif


#endif
