#ifndef SIDEKIQ_API_H
#define SIDEKIQ_API_H

/* Copyright 2013-2021 Epiq Solutions, All Rights Reserved */

/*! \file sidekiq_api.h
 * \brief This file contains the public interface of the sidekiq_api
 * provided by libsidekiq.
 *
 * @mainpage libsidekiq - Sidekiq Library
 *
 * Sidekiq is a software defined radio card in a MiniPCIe, M.2 (3042 and 2280), or VITA 57.1 FPGA
 * Mezzanine Card (FMC) form factor (Sidekiq X2 and X4).  Each utilizes an RFIC, which provides the
 * complete RF front end & baseband analog & A/D and D/A converters.  An on-board FPGA then provides
 * timestamping/buffering, along with optional signal processing.
 *
 * For the MiniPCIe and M.2 form factors, a single lane (x1) PCIe interface in the FPGA provides a
 * transport path between the host system and Sidekiq, which is used for streaming data between the
 * host and Sidekiq, as well as for command/control of Sidekiq through a register interface.  A USB
 * 2.0 high speed interface is also included in Sidekiq mPCIe and M.2-3042, which is used to provide
 * a path for re-programming the FPGA bitstream.  This USB interface can also be used by the host
 * for streaming of data and command/control of the card for host systems that include a MiniPCIe or
 * M.2-3042 card slot but only wire up the USB 2.0 pins.  See the <a
 * href="https://epiqsolutions.com/rf-transceiver/sidekiq/">Epiq Solutions Website</a> for more
 * details.
 *
 * The Sidekiq Z2 is offered in a MiniPCIe form factor but uses a USB 2.0 high speed interface as a
 * transport between the host system and the Zynq 7010 FPGA.  See <a
 * href="https://epiqsolutions.com/rf-transceiver/sidekiq-z2/">Sidekiq Z2</a> for more details.
 *
 * The VITA 57.1 FMC form factor can be used in conjunction with compliant FPGA carrier boards to
 * provide a user with access to IQ samples and command / control.  See <a
 * href="https://epiqsolutions.com/rf-transceiver/sidekiq-x2/">Sidekiq X2</a> and <a
 * href="https://epiqsolutions.com/rf-transceiver/sidekiq-x4/">Sidekiq X4</a> for more details.
 *
 * The Sidekiq NV100 is offered in an M.2-2280 form factor and uses a Gen2 x2 PCIe as a transport
 * between the on-board Artix 7 FPGA and the host system.  See <a
 * href="https://epiqsolutions.com/rf-transceiver/sidekiq-nv100/">Sidekiq NV100</a> for more
 * details.
 *
 * The following list enumerates the features of Sidekiq (MiniPCIe card form factor):
 *  - Flexible RF front end supports two operating modes:
 *    - Two phase coherent RF receivers (common LO)
 *    - One RF receiver + one RF transmitter (separate LOs)
 *  - RF tuning range from 70 MHz to 6 GHz
 *  - Up to 50 MHz RF bandwidth per channel (min sample rate: 233 Ksps, max sample rate: 61.44 Msps)
 *  - Great dynamic range with 12-bit A/D and D/A converters
 *  - PCIe Gen 1 x1 (2.5 GT/s) interface to host + USB 2.0 Hi-Speed interface
 *  - Integrated FPGA for custom signal processing and PCIe data transport to host
 *  - Integrated temperature sensor + accelerometer
 *
 * The following list enumerates the features of Sidekiq M.2 (M.2-3042 card form factor):
 *  - Flexible RF front end supports two operating modes:
 *    - Two RF receiver + two RF transmitter (2x2 MIMO)
 *    - One RF receiver + one RF transmitter (separate LOs)
 *  - RF tuning range from 70 MHz to 6 GHz
 *  - Up to 50 MHz RF bandwidth per channel (min sample rate: 233 Ksps, max sample rate: 61.44 Msps)
 *  - Great dynamic range with 12-bit A/D and D/A converters
 *  - PCIe Gen 2 x1 (5.0GT/s) interface to host + USB 2.0 Hi-Speed interface
 *  - Integrated FPGA for custom signal processing and PCIe data transport to host
 *  - Integrated temperature sensor + accelerometer
 *
 * The following list enumerates the features of Sidekiq Stretch (M.2-2280 Key B+M card form factor):
 *  - One RF receiver + one RF transmitter (separate LOs)
 *  - RF tuning range from 70 MHz to 6 GHz
 *  - Up to 50 MHz RF bandwidth per channel (min sample rate: 233 Ksps, max sample rate: 61.44 Msps)
 *  - Great dynamic range with 12-bit A/D and D/A converters
 *  - PCIe x2 (5.0GT/s) interface to host
 *  - Integrated FPGA for custom signal processing and PCIe data transport to host
 *  - Integrated temperature sensor + accelerometer
 *  - Integrated GPSDO receiver with 1PPS
 *  - Sub-octave Rx pre-select filtering with adjustable band-pass from 150MHz to 6GHz
 *
 * The following list enumerates the features of Sidekiq Z2 (MiniPCIe card form factor):
 *  - Wideband RF Transceiver (Analog Devices' AD9364)
 *    - 1Rx + 1Tx RF Transceiver
 *    - RF tuning range from 70 MHz to 6 GHz
 *    - Four band Rx pre-select filter bank
 *    - Up to 61.44 Msps sample rate
 *    - Great dynamic range with 12-bit A/D and D/A converters
 *    - 40 MHz TCVCXO ref clock with +/- 1 PPM stability
 *  - Linux Computer (Xilinx Zynq XC7Z010-2I)
 *    - Dual-core ARM Cortex A9 CPU running Linux
 *    - 512 MB of DDR3L RAM
 *    - 32 MB of QSPI flash memory
 *    - Linux boot time <2 seconds
 *
 * The following list enumerates the features of Sidekiq X2 (VITA 57.1 FMC HPC form factor):
 *  - Two phase coherent RF receivers (common LO) + third independently tunable RF receiver
 *  - Seven band RF pre-select filters on all three Rx antenna ports
 *  - Two phase coherent RF transmitters (common LO)
 *  - RF tuning range from 1 MHz to 6 GHz
 *  - Up to 100 MHz RF bandwidth per channel (max sample rate: 122.88 Msps)
 *  - Exceptional dynamic range with 16-bit A/D converters, 14-bit D/A converters
 *  - Integrated temperature sensor
 *  - 10MHz + PPS sync inputs
 *
 * The following list enumerates the features of Sidekiq X4 (VITA 57.1 FMC HPC form factor):
 *  - Four RF receivers (phase coherent or <b>independently tunable</b>)
 *  - Seven band-pass RF filters on each RF receiver
 *  - Four RF transmitters (<b>phase coherent</b> or two phase coherent pairs)
 *  - RF tuning range from 1 MHz to 6 GHz
 *  - Up to 200 MHz RF bandwidth per channel (max sample rate: 245.76 Msps)
 *  - Exceptional dynamic range with 16-bit A/D converters, 14-bit D/A converters
 *  - Integrated temperature sensor
 *  - 10MHz + PPS sync inputs
 *
 * The following list enumerates the features of Matchstiq Z3u:
 *  - Wideband RF Transceiver (Analog Devices' AD9364)
 *    - 2-channel phase coherent Rx, or 1 Tx + 1 Rx
 *    - RF tuning range from 70 MHz to 6 GHz
 *    - Up to 61.44 Msps sample rate
 *    - Great dynamic range with 12-bit A/D and D/A converters
 *    - 40 MHz TCVCXO ref clock with +/- 1 PPM stability
 *    - Integrated temperature sensor + 3-axis gyroscope + 3-axis accelerometer
 *    - Integrated GPSDO receiver with 1PPS
 *    - Sub-octave Rx pre-select filtering with adjustable band-pass from 150MHz to 6GHz
 *  - Linux Computer (Xilinx Zynq Ultrascale+ XCZU3EG)
 *    - Quad-core ARM Cortex A53 CPU running Linux
 *    - 2 GB of LPDDR4 RAM
 *    - 128 MB of QSPI flash memory
 *    - 128 GB eMMC + microSD card slot
 *    - USB 3.0 OTG via USB-C
 *
 * The following list enumerates the features of Sidekiq NV100:
 *  - Wideband RF Transceiver (Analog Devices' ADRV9004)
 *    - Antenna Port 1: U.FL coaxial connector supporting Tx or Rx
 *    - Antenna Port 2: U.FL coaxial connector supporting either Tx or Rx
 *    - RF tuning range from 30 MHz to 6 GHz (RF access to 10 MHz)
 *    - Up to 40 MHz RF channel bandwidth
 *    - Up to 61.44 Msps sample rate
 *    - Exceptional RF fidelity and instantaneous dynamic range with 16-bit A/D and D/A converters
 *    - 40 MHz TCVCXO ref clock with +/- 1 PPM stability
 *    - Integrated temperature sensor + 3-axis gyroscope + 3-axis accelerometer
 *    - Integrated GPSDO receiver with 1PPS
 *    - Sub-octave Rx pre-select filtering from 400 MHz to 6 GHz
 *
 * Documentation for the primary Sidekiq API exists in these files:
 *  - sidekiq_api.h
 *  - sidekiq_types.h
 *  - sidekiq_params.h
 *
 * Documentation for the custom transport developers, the Sidekiq Transport API, exists in these files:
 *  - sidekiq_xport_api.h
 *  - sidekiq_xport_types.h
 *
 * @page AD9361TimestampSlip Timestamp Slips within AD9361 Products
 * 
 * @section TimestampSlipOverview Overview
 * 
 * Products that use the AD9361 RFIC will have timestamp slips when using API functions that need to
 * deactivate the sample clock in order to make updates to the radio configuration.
 * 
 * This occurs when:
 *  - updating the LO frequency
 *  - updating the sample rate
 *  - running the transmit quadrature calibration
 * 
 * Functions that will affect the timestamp:
 *  - ::skiq_write_rx_LO_freq()
 *  - ::skiq_write_rx_sample_rate_and_bandwidth()
 *  - ::skiq_write_tx_LO_freq()
 *  - ::skiq_run_tx_quadcal()
 *  - ::skiq_write_rx_freq_tune_mode()
 *  - ::skiq_write_tx_freq_tune_mode()
 * 
 * Functions that will be affected by the timestamp slip:
 *  - ::skiq_read_last_1pps_timestamp()
 *  - ::skiq_receive()
 *  - ::skiq_transmit()
 *  - ::skiq_read_curr_rx_timestamp()
 *  - ::skiq_read_curr_tx_timestamp()
 * 
 * It is recommended to use the system clock - which is not subject to interruptions - if a consistent
 * time source is needed.
 */

/***** INCLUDES *****/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <endian.h>
#endif /* _WIN32 */
#include <sys/types.h>

#include "sidekiq_types.h"
#include "sidekiq_params.h"
#include "sidekiq_xport_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libfunctions Library Functions and Definitions
 *
 * @brief These functions and definitions are related to interacting with the library configuration
 * unrelated to the Sidekiq SDR.
 */

/**
 * @defgroup cardfunctions Card Functions and Definitions
 *
 * @brief These functions and definitions are related to initializing and
 * configuring the digital (non-RF) related functionality of the Sidekiq SDR.
 */

/**
 * @defgroup timestampfunctions Timestamp Functions
 *
 * @brief These functions are related to configuring and querying the System and
 * RF timestamps of the Sidekiq SDR.
 */

/**
 * @defgroup rficfunctions RFIC Functions and Definitions
 *
 * @brief These functions and definitions are related to configuring and
 * exercising functionality of the RFIC on the Sidekiq SDR.  These functions may
 * also be related to receive and transmit capabilities, but are grouped here
 * because they deal directly with the RFIC configuration.
 */

/**
 * @defgroup rfportfunctions RF Port Functions and Definitions
 *
 * @brief These functions and definitions are related to configuring and exercising the capabilities
 * of the physical RF connections and pathways of the Sidekiq SDR.
 */

/**
 * @defgroup rxfunctions Receive Functions and Definitions
 *
 * @brief These functions and definitions are related to configuring and
 * exercising the receive capabilities of the Sidekiq SDR.
 */

/**
 * @defgroup txfunctions Transmit Functions and Definitions
 *
 * @brief These functions and definitions are related to configuring and
 * exercising the transmit capabilities of the Sidekiq SDR.
 */

/**
 * @defgroup hopfunctions Fast Frequency Hopping Functions and Definitions
 *
 * @brief These functions and definitions are related to configuring and
 * exercising the fast frequency hopping capabilities of the Sidekiq SDR.
 */

/**
 * @defgroup fpgafunctions FPGA Functions and Definitions
 *
 * @brief These functions and definitions are related to communicating and
 * exercising the FPGA capabilities of the Sidekiq SDR.
 */

/**
 * @defgroup ppsfunctions 1PPS Functions and Definitions
 *
 * @brief These functions and definitions are related to interaction with the
 * 1PPS pulse input of the Sidekiq SDR.
 */

/**
 * @defgroup tcvcxofunctions Crystal Oscillator (TCVCXO) Functions
 *
 * @brief These functions are related to configuration and usage of the on-board
 * TCVCXO (Temperature Compensated / Voltage Controlled Crystal Oscillator) of
 * the Sidekiq SDR.
 */

/**
 * @defgroup accelfunctions Accelerometer Functions
 *
 * @brief These functions are related to using the Sidekiq's on-board accelerometer (<a
 * href="http://www.analog.com/en/products/mems/accelerometers/adxl346.html">Analog Device's
 * ADXL346</a>) for miniPCIe Sidekiq and M.2 Sidekiq products.  The Sidekiq Z2, Sidekiq Stretch,
 * and Matchstiq Z3u products use <a
 * href="https://www.invensense.com/products/motion-tracking/6-axis/icm-20602/">TDK's InvenSense
 * ICM-20602</a> motion tracking device.  The accelerometer functions in this section are designed
 * to function equivalently with their default configurations.  Users may modify the behavior of the
 * underlying device by using skiq_read_accel_reg() and skiq_write_accel_reg() as it suits their
 * needs.
 */

/**
 * @defgroup calfunctions Receiver Calibration Functions
 *
 * @brief These functions and definitions are related to reading receiver
 * calibration offsets (in dB) of the Sidekiq SDR.
 */

/**
 * @defgroup flashfunctions Flash Functions and Definitions
 *
 * @brief These functions and definitions are related to utilizing a Sidekiq's on-board flash
 * storage for FPGA bitstream(s).
 */

/**
 * @defgroup gpsdofunctions GPS Disciplined Oscillator (GPSDO) Functions
 *
 * @brief These functions are related to status and availability of GPSDO for a product.
 */


/***** DEFINES *****/
/** @brief Major version number for libsidekiq */
#define LIBSIDEKIQ_VERSION_MAJOR 4

/** @brief Minor version number for libsidekiq */
#define LIBSIDEKIQ_VERSION_MINOR 18

/** @brief Patch version number for libsidekiq */
#define LIBSIDEKIQ_VERSION_PATCH 0

/** @brief Label version for libsidekiq */
#ifdef _WIN32
#define LIBSIDEKIQ_VERSION_LABEL "-dev-win64"
#else
#define LIBSIDEKIQ_VERSION_LABEL ""

#endif /* _WIN32 */

/** @brief Version of libsidekiq.  e.g., To test for LIBSIDEKIQ_VERSION > 3.6.1
 *
 * @code
 * #if LIBSIDEKIQ_VERSION > 30601
 * @endcode
 */
#define LIBSIDEKIQ_VERSION (LIBSIDEKIQ_VERSION_MAJOR * 10000 \
                            + LIBSIDEKIQ_VERSION_MINOR * 100 \
                            + LIBSIDEKIQ_VERSION_PATCH)

/** @addtogroup rxfunctions
 * @{
 */

/** @brief ::SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS is the largest block size that can
    be transferred between the FPGA and the CPU in a single transaction when
    receiving. */
#define SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS 1024

/** @brief The same parameter as ::SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS except
    calculated in bytes @hideinitializer */
#define SKIQ_MAX_RX_BLOCK_SIZE_IN_BYTES (SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS*sizeof(uint32_t))

/** @brief The current Rx header size is 6 words but may change in the future.
    The metadata placed at the beginning of each IQ block.  Refer to
    skiq_receive() for details on the formatting of the metadata. */
#define SKIQ_RX_HEADER_SIZE_IN_WORDS 6

/** @brief The current Rx header size, only in bytes. @hideinitializer */
#define SKIQ_RX_HEADER_SIZE_IN_BYTES (SKIQ_RX_HEADER_SIZE_IN_WORDS*sizeof(uint32_t))

/** @brief When running in packed mode, every 4 samples are 3 words of data.
    ::SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK converts from number of words to number
    of samples when running in packed mode @hideinitializer */
#define SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK(block_size_in_words) ( ( (block_size_in_words) / 3) * 4 )

/** @brief When running in packed mode, every 3 words of data contain 4 samples.
    ::SKIQ_NUM_WORDS_IN_PACKED_BLOCK converts from the number of samples to the number of words
    needed to hold the number of unpacked samples.  The ::SKIQ_NUM_WORDS_IN_PACKED_BLOCK macro
    rounds up by adding one less than the denominator (the number of bytes in a word: 4) prior to
    performing the integer division.

    For example, if a user wants 5 packed samples, then 4 words of data must be considered when
    unpacking.  Packed samples occupy 24 bits and words are 32 bits

    5 x 24 bits < 4 x 32 bits == 120 bits < 128 bits

    SKIQ_NUM_WORDS_IN_PACKED_BLOCK(5) = ((5 * 3) + 3) / 4
                                      = (15      + 3) / 4
                                      = 18            / 4
                                      = 4


    Another example is if a user wants 1906250 packed samples, then 1429688 words of data must be
    considered when unpacking.

    1906250 x 24 bits < 1429688 x 32 bits == 45750000 bits < 45750016 bits

    SKIQ_NUM_WORDS_IN_PACKED_BLOCK(1906250) = ((1906250 * 3) + 3) / 4
                                            = (5718750       + 3) / 4
                                            = 5718753             / 4
                                            = 1429688
    @hideinitializer */
#define SKIQ_NUM_WORDS_IN_PACKED_BLOCK(num_packed_samples) ((((num_packed_samples) * 3) + 3) / 4)

/** @brief The number of packets in the ring buffer is the number of packets that can be buffered
    and not yet received prior to the packets getting overwritten.

    @deprecated As of libsidekiq v4.13, this value is no longer guaranteed to be accurate as
                the value can change based upon the configuration of the PCI DMA Driver kernel
                module.
*/
#ifdef SMALL_NUM_DESCRIPTORS
#define SKIQ_RX_NUM_PACKETS_IN_RING_BUFFER (1024)
#else
#define SKIQ_RX_NUM_PACKETS_IN_RING_BUFFER (2048)
#endif

/** @}*/

/** @addtogroup txfunctions
 * @{
 */

/** @brief The largest number of words that can be transferred between the FPGA and
    the CPU. This includes both the data block as well as the header size. */
#define SKIQ_MAX_TX_PACKET_SIZE_IN_WORDS 65536

/** @brief The current Tx header size is fixed at 4 words of metadata for now at
    the start of each I/Q block, which may well increase at some point. For
    details on the exact format and contents of the transmit packet, refer to
    skiq_transmit() */
#define SKIQ_TX_HEADER_SIZE_IN_WORDS 4

/** @brief The offset (in 32-bit words) to the header where the Tx timestamp is
    stored */
#define SKIQ_TX_TIMESTAMP_OFFSET_IN_WORDS 2

/** @brief ::SKIQ_MAX_TX_BLOCK_SIZE_IN_WORDS is the largest block size of sample
    data that can be transferred from the CPU to the FPGA while transmitting.
    Note that a "block" of data includes the sample data minus the header
    data @hideinitializer */
#define SKIQ_MAX_TX_BLOCK_SIZE_IN_WORDS (SKIQ_MAX_TX_PACKET_SIZE_IN_WORDS-SKIQ_TX_HEADER_SIZE_IN_WORDS)

/** @brief The current Tx header size, only in bytes. @hideinitializer */
#define SKIQ_TX_HEADER_SIZE_IN_BYTES (SKIQ_TX_HEADER_SIZE_IN_WORDS*sizeof(uint32_t))

/** @brief The Tx packet must be in increments of 256 words.  Note: the packet size accounts
    for both the header size as well as the block (sample) size */
#define SKIQ_TX_PACKET_SIZE_INCREMENT_IN_WORDS (256)

/** @}*/

/** @addtogroup fpgafunctions
 * @{
 */

/** @brief ::SKIQ_START_USER_FPGA_REG_ADDR is first address available in the
    FPGA memory map that can be user defined.  These 32-bit register addresses
    increment by 4 bytes */
#define SKIQ_START_USER_FPGA_REG_ADDR 0x00008700

/** @brief ::SKIQ_END_USER_FPGA_REG_ADDR is last address of the last FPGA
    register available in the FPGA memory map that can be user defined. */
#define SKIQ_END_USER_FPGA_REG_ADDR 0x00008FFF

/** @}*/

/** @brief ::SKIQ_MIN_LO_FREQ defines the minimum acceptable RF frequency for the
    Rx/Tx LO for a standard Sidekiq.

    @deprecated To determine the min LO frequency use
    skiq_read_rx_LO_freq_range() or skiq_read_min_rx_LO_freq(). */
#define SKIQ_MIN_LO_FREQ 47000000LLU

/** @brief ::SKIQ_MAX_LO_FREQ defines the maximum acceptable RF frequency for
    the Rx/Tx LO for a standard Sidekiq.

    @deprecated To determine the max LO frequency use
    skiq_read_rx_LO_freq_range() or skiq_read_max_rx_LO_freq(). */
#define SKIQ_MAX_LO_FREQ 6000000000LLU

/** @brief ::SKIQ_MIN_SAMPLE_RATE is the minimum Rx/Tx sample rate that can
    be generated for a single Rx/Tx channel.

    @deprecated To determine the minimum sample rate for the specific hardware /
    radio configuration, refer to @ref skiq_read_parameters. */
#define SKIQ_MIN_SAMPLE_RATE 233000

/** @brief ::SKIQ_MAX_SAMPLE_RATE is the maximum Rx/Tx sample rate that can
    be generated for a single Rx/Tx channel.

    @note this rate can be extended higher, but only with certain caveats, so
    this is kept at a reasonably safe value for all use cases by default.

    @deprecated To determine the maximum sample rate for the specific hardware /
    radio configuration, refer to @ref skiq_read_parameters. */
#define SKIQ_MAX_SAMPLE_RATE 122880000

/** @brief ::SKIQ_MAX_DUAL_CHAN_MPCIE_SAMPLE_RATE is the maximum sample rate
    that can be generated when running in dual channel mode on a Sidekiq mPCIe
    (::skiq_mpcie) product.  Note: this rate can be extended higher, but only
    with certain caveats, so this is kept at a reasonably safe value for all use
    cases by default. */
#define SKIQ_MAX_DUAL_CHAN_MPCIE_SAMPLE_RATE 30720000

/** @brief ::SKIQ_MAX_DUAL_CHAN_Z3U_SAMPLE_RATE is the maximum sample rate
    that can be generated when running in dual channel mode on a Matchstiq Z3u
    (::skiq_z3u) product.  */
#define SKIQ_MAX_DUAL_CHAN_Z3U_SAMPLE_RATE 30720000

/** @brief ::SKIQ_MAX_TX_ATTENUATION is the maximum value of the Tx attenuation.

    @deprecated Use skiq_read_parameters() and the corresponding skiq_param_t
    struct to determine the attenuation range. */
#define SKIQ_MAX_TX_ATTENUATION (359)

/** @brief ::SKIQ_MIN_RX_GAIN is the minimum value of the Rx gain.

    @deprecated To determine the minimum Rx gain, use
    skiq_read_rx_gain_index_range(). */
#define SKIQ_MIN_RX_GAIN (0)

/** @brief ::SKIQ_MAX_RX_GAIN is the maximum value of the Rx gain.

    @deprecated To determine the maximum Rx gain, use
    skiq_read_rx_gain_index_range(). */
#define SKIQ_MAX_RX_GAIN (76)

/** @brief ::SKIQ_MAX_NUM_CARDS is the maximum number of Sidekiq cards that is
    supported in a system */
#define SKIQ_MAX_NUM_CARDS (32)

/** @brief ::SKIQ_SYS_TIMESTAMP_FREQ is the frequency at which the system timestamp increments

    @attention This value is valid only for @ref ::skiq_mpcie "mPCIe" and @ref
    ::skiq_m2 "M.2"

    @deprecated All platforms should use the skiq_read_sys_timestamp_freq() API
    instead */
#define SKIQ_SYS_TIMESTAMP_FREQ (40000000)

/** @brief ::SKIQ_RX_SYS_META_WORD_OFFSET is the offset at which the system
    metadata is located within a receive packet.  Included in this is the Rx
    handle as well as the overload bit

    @deprecated Use skiq_rx_block_t::hdl, skiq_rx_block_t::overload, and
    skiq_rx_block_t::rfic_control instead of this definition */
#define SKIQ_RX_SYS_META_WORD_OFFSET (4)

/** @brief ::SKIQ_RX_USER_META_WORD_OFFSET is the offset at which the user-defined metadata is
    located with a receive packet

    @deprecated Use skiq_rx_block_t::user_meta instead of this definition */
#define SKIQ_RX_USER_META_WORD_OFFSET (5)

/** @brief ::SKIQ_RX_META_HDL_BITS is the bitmask which represent the Rx handle

    @deprecated Use skiq_rx_block_t::hdl instead of this definition */
#define SKIQ_RX_META_HDL_BITS           (0x3F)

/** @brief ::SKIQ_RX_META_OVERLOAD_BIT is the location of the bit representing
    the Rx overload detection in the miscellaneous metadata of a receive packet

    @deprecated Use skiq_rx_block_t::overload instead of this definition */
#define SKIQ_RX_META_OVERLOAD_BIT       (1 << 6)

/** @brief ::SKIQ_RX_META_RFIC_CTRL_BITS are the bits which contain the RFIC
    control bits embedded within the system metadata

    @deprecated Use skiq_rx_block_t::rfic_control instead of this definition */
#define SKIQ_RX_META_RFIC_CTRL_BITS     (0xFF)

/** @brief ::SKIQ_RX_META_RFIC_CTRL_OFFSET is the bit offset where the RFIC
    control bits are located within the system metadata

    @deprecated Use skiq_rx_block_t::rfic_control instead of this definition */
#define SKIQ_RX_META_RFIC_CTRL_OFFSET   (7)

/** @addtogroup txfunctions
 * @{
 */

/** @brief ::SKIQ_TX_ASYNC_SEND_QUEUE_FULL is the return code of the
    skiq_transmit() call when using ::skiq_tx_transfer_mode_async and there is
    no space available to store the data to send */
#define SKIQ_TX_ASYNC_SEND_QUEUE_FULL (100)

/** @brief ::SKIQ_TX_MAX_NUM_THREADS is the maximum number of threads used in
    transmitting when using ::skiq_tx_transfer_mode_async */
#define SKIQ_TX_MAX_NUM_THREADS (10)

/** @brief ::SKIQ_TX_MIN_NUM_THREADS is the minimum number of threads used in
    transmitting when skiq_tx_transfer_mode_async */
#define SKIQ_TX_MIN_NUM_THREADS (2)

/** @}*/

/** @brief ::RFIC_CONTROL_OUTPUT_MODE_GAIN_CONTROL_RXA1 is the value that should
    be used to enable the gain values for RxA1 to be presented in the system
    metadata of each receive packet.  Use this definition in conjunction with
    skiq_write_rfic_control_output_config()

    @deprecated since v4.9.0 Not all radio types use this control output mode value to present
    the gain in the control output field.  Use skiq_read_rfic_control_output_rx_gain_config()
    to determine appropriate enable and mode configuration to present A1 gain in the metadata.
*/
#define RFIC_CONTROL_OUTPUT_MODE_GAIN_CONTROL_RXA1 (0x16)

/** @brief ::RFIC_CONTROL_OUTPUT_MODE_GAIN_CONTROL_RXA2 is the value that should
    be used to enable the gain values for RxA2 to be presented in the system
    metadata of each receive packet.  Use this definition in conjunction with
    skiq_write_rfic_control_output_config()

    @deprecated since v4.9.0 Not all radio types use this control output mode value to present
    the gain in the control output field.  Use skiq_read_rfic_control_output_rx_gain_config()
    to determine appropriate enable and mode configuration to present A2 gain in the metadata.
*/
#define RFIC_CONTROL_OUTPUT_MODE_GAIN_CONTROL_RXA2 (0x17)

/** @brief ::RFIC_CONTROL_OUTPUT_MODE_GAIN_BITS are the bits used in conveying
    the current gain setting (read from the RFIC control output).  Use this
    definition in conjunction with skiq_write_rfic_control_output_config()

    @deprecated since v4.9.0 Not all radio types use this control output mode value to present
    the gain in the control output field.  Use skiq_read_rfic_control_output_rx_gain_config()
    to determine appropriate enable and mode configuration.
*/
#define RFIC_CONTROL_OUTPUT_MODE_GAIN_BITS (0x7F)

/** @addtogroup rxfunctions
    @{
 */

/** @brief Option for timeout_us argument of skiq_set_rx_transfer_timeout() to
    return immediately, regardless as to whether or not samples are available.
    Effectively results in a non-blocking skiq_receive() call and the return
    code is set accordingly
 */
#define RX_TRANSFER_NO_WAIT (0)

/** @brief Option for timeout_us argument of skiq_set_rx_transfer_timeout() to
    block forever until samples are available. Effectively results in a blocking
    skiq_receive() call with no timeout.  Use with caution (or don't use at all)
    - a failure to transfer samples will result in the calling thread being
    blocked indefinitely.
 */
#define RX_TRANSFER_WAIT_FOREVER (-1)

/** @brief Possible value for @b p_timeout_us argument of
    skiq_get_rx_transfer_timeout() to indicate that blocking skiq_receive() is
    not supported by the card and/or its currently configured transport layer
    (::skiq_xport_type_t).
 */
#define RX_TRANSFER_WAIT_NOT_SUPPORTED (-2)

/** @}*/

/***** inline Functions  *****/


/*****************************************************************************/
/** The skiq_tx_set_block_timestamp() function sets the timestamp field
    (skiq_tx_block_t::timestamp) of a transmit block.

    @since Function added in v4.0.0

    @ingroup txfunctions

    @param[in] p_block [::skiq_tx_block_t *] reference to a @ref skiq_tx_block_t
    @param[in] timestamp desired timestamp for the transmit block

    @return void
 */
static inline void skiq_tx_set_block_timestamp( skiq_tx_block_t *p_block, uint64_t timestamp )
{
#ifndef _WIN32
    p_block->timestamp = htole64(timestamp);
#else
    /* TODO our WIN32 builds are presently only little-endian, and endian.h does not exist */
    p_block->timestamp = timestamp;
#endif /* _WIN32 */
}


/*****************************************************************************/
/** The skiq_tx_get_block_timestamp() function return the timestamp field
    (skiq_tx_block_t::timestamp) of a referenced transmit block.

    @since Function added in v4.0.0

    @ingroup txfunctions

    @param[in] p_block [::skiq_tx_block_t *] reference to a @ref skiq_tx_block_t

    @return uint64_t the timestamp associated with the transmit block
 */
static inline uint64_t skiq_tx_get_block_timestamp( skiq_tx_block_t *p_block )
{
#ifndef _WIN32
    return le64toh( p_block->timestamp );
#else
    /* TODO our WIN32 builds are presently only little-endian, and endian.h does not exist */
    return (p_block->timestamp);
#endif /* _WIN32 */
}


/*****************************************************************************/
/** The skiq_tx_block_allocate_by_bytes() function allocates a Sidekiq Transmit
    Block (::skiq_tx_block_t) with the desired number of bytes.

    @note The returned reference @b MUST be freed by calling @ref
    skiq_tx_block_free.

    @since Function added in v4.0.0

    @ingroup txfunctions

    @param[in] data_size_in_bytes desired number of bytes in the transmit block

    @return skiq_tx_block_t* a reference to the Sidekiq Transmit Block
 */
static inline skiq_tx_block_t *skiq_tx_block_allocate_by_bytes(uint32_t data_size_in_bytes)
{
    skiq_tx_block_t *p_tx_block = NULL;
    size_t alloc_size = SKIQ_TX_HEADER_SIZE_IN_BYTES + data_size_in_bytes;

#if (defined __MINGW32__ || defined _MSC_VER)
    p_tx_block = (skiq_tx_block_t *)_aligned_malloc(alloc_size,
                                                    (size_t)SKIQ_TX_BLOCK_MEMORY_ALIGN);
#else
    if ( posix_memalign((void **)&p_tx_block, SKIQ_TX_BLOCK_MEMORY_ALIGN, alloc_size) != 0 )
    {
        p_tx_block = NULL;
    }
#endif

    if ( p_tx_block != NULL )
    {
        memset( p_tx_block, 0, alloc_size );
    }

    return p_tx_block;
}


/*****************************************************************************/
/** The skiq_tx_block_allocate() function allocates a Sidekiq Transmit Block
    (::skiq_tx_block_t) with the desired number of unpacked samples (words).

    @note The returned reference @b MUST be freed by calling @ref
    skiq_tx_block_free.

    @since Function added in v4.0.0

    @ingroup txfunctions

    @param[in] data_size_in_samples desired number of samples in the transmit block

    @return skiq_tx_block_t* a reference to the Sidekiq Transmit Block
 */
static inline skiq_tx_block_t *skiq_tx_block_allocate(uint32_t data_size_in_samples)
{
    return skiq_tx_block_allocate_by_bytes( data_size_in_samples * 4 );
}


/*****************************************************************************/
/** The skiq_tx_block_free() function frees a Sidekiq Transmit Block
    (::skiq_tx_block_t) that was allocated using skiq_tx_block_allocate().

    @note The passed reference @b MUST have been allocated by calling @ref
    skiq_tx_block_allocate.

    @since Function added in v4.0.0

    @ingroup txfunctions

    @param[in] p_block [::skiq_tx_block_t *] reference to the Sidekiq Transmit Block to free

    @return void
 */
static inline void skiq_tx_block_free(skiq_tx_block_t *p_block)
{
    free( p_block );
}


/***** External Functions  *****/

/*****************************************************************************/
/** The skiq_get_cards() function is responsible for generating a list
    of valid Sidekiq card indices for the transport specified.  Return of the
    card does not mean that it is available for use by the application.  To
    check card availability, refer to skiq_is_card_avail().

    @since Function added in API @b v4.0.0

    @note Can be called before skiq_init(), skiq_init_without_cards(), or skiq_init_by_serial_str()

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_init_by_serial_str
    @see skiq_exit

    @param[in] xport_type [::skiq_xport_type_t]  transport type to detect card
    @param[out] p_num_cards pointer to where to store the number of cards
    @param[out] p_cards pointer to where to store the card indices of the
    Sidekiqs available.  There should be room to store at least
    ::SKIQ_MAX_NUM_CARDS at this location.

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_get_cards(skiq_xport_type_t xport_type,
                                uint8_t *p_num_cards,
                                uint8_t *p_cards);

/*****************************************************************************/
/** The skiq_read_serial_string() function is responsible for returning the serial
    number of the Sidekiq.

    @note Memory used for holding the string representation of the serial number
    is managed internally by libsidekiq and does not need to be managed in any
    manner by the end user (i.e. no need to free memory).

    @ingroup cardfunctions
    @see skiq_get_card_from_serial_string

    @param[in] card card index of the Sidekiq of interest
    @param[out] pp_serial_num: a pointer to hold the serial number

    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_serial_string(uint8_t card,
                                         char** pp_serial_num);

/*****************************************************************************/
/** The skiq_get_card_from_serial_string() function is responsible for obtaining
    the Sidekiq card index for the specified serial number.

    @ingroup cardfunctions
    @see skiq_read_serial_string

    @param[in] p_serial_num serial number of Sidekiq card
    @param[out] p_card pointer to where to store the corresponding card index
    of the specified Sidekiq

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_get_card_from_serial_string( char* p_serial_num,
                                                   uint8_t *p_card );

/*****************************************************************************/
/** The skiq_init() function is responsible for performing all initialization
    tasks for the sidekiq platform.

    @since Function signature modified in API @b v4.0.0

    @ingroup cardfunctions
    @see skiq_init_without_cards
    @see skiq_init_by_serial_str
    @see skiq_exit
    @see skiq_enable_cards
    @see skiq_enable_cards_by_serial_str
    @see skiq_disable_cards

    @param[in] type [::skiq_xport_type_t]  the transport type that is required:
      - ::skiq_xport_type_auto - automatically detect and use available transport
      - ::skiq_xport_type_pcie - communicate with Sidekiq over PCIe.  If USB is
          available it will also be used for certain functionality.
      - ::skiq_xport_type_usb - communicate with Sidekiq entirely over USB.  A
          USB FPGA bitstream must be utilized if initializing at
          ::skiq_xport_init_level_full.
      - ::skiq_xport_type_custom - communicate with Sidekiq using the registered
          transport implementation provided by a call to
          skiq_register_custom_transport().  If USB is available, it will also
          be used for certain functionality.

    @param[in] level [::skiq_xport_init_level_t] the transport functionality
    level of initialization that is required:
      - ::skiq_xport_init_level_basic - minimal initialization necessary to bring
          up the requested transport interface for FPGA / RFIC register
          reads/writes, and initialize the mutexes that serializes access to
          libsidekiq
      - ::skiq_xport_init_level_full - Same as ::skiq_xport_init_level_basic and
          perform the complete bring up of all hardware (most applications
          concerned with sending/receiving RF will use this)

    @param[in] p_card_nums pointer to the list of Sidekiq card indices to be initialized
    @param[in] num_cards number of Sidekiq cards to initialize

    @attention  As of libsidekiq v4.8.0, the @a type parameter is ignored as the
                transport type is automatically set to ::skiq_xport_type_auto, which
                will select the correct transport for the specified card(s).

    @attention  ::skiq_init() and ::skiq_init_by_serial_str() should only be called when starting an
                application or after ::skiq_exit() has been called; these functions are not
                designed to be called multiple times to initialize individual cards.

    @return int32_t status where 0=success, anything else is an error
    @retval -EEXIST libsidekiq has already been initialized in this application without
                    skiq_exit() being called
    @retval -E2BIG  if the number of cards requested exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
    @retval -EINVAL if one of the specified card indices is out of range or refers to a
                    non-existent card
*/
EPIQ_API int32_t skiq_init(skiq_xport_type_t type,
                           skiq_xport_init_level_t level,
                           uint8_t *p_card_nums,
                           uint8_t num_cards);

/*****************************************************************************/
/** The skiq_enable_cards() function is responsible for performing all initialization
    tasks for the specified Sidekiq cards.

    @attention  The Sidekiq library must have been previously initialized with skiq_init(),
                skiq_init_without_cards(), or skiq_init_by_serial_str(). The transport type is
                automatically selected based on availability.

    @since Function added in API @b v4.8.0

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_init_without_cards
    @see skiq_init_by_serial_str
    @see skiq_exit
    @see skiq_enable_cards_by_serial_str
    @see skiq_disable_cards

    @param[in] cards array of Sidekiq card indices to be initialized
    @param[in] num_cards number of Sidekiq cards to initialize
    @param[in] level [::skiq_xport_init_level_t] the transport functionality
    level of initialization that is required:
      - ::skiq_xport_init_level_basic - minimal initialization necessary to bring
          up the requested transport interface for FPGA / RFIC register
          reads/writes, and initialize the mutexes that serializes access to
          libsidekiq
      - ::skiq_xport_init_level_full - Same as ::skiq_xport_init_level_basic and
          perform the complete bring up of all hardware (most applications
          concerned with sending/receiving RF will use this)

    @return int32_t  status where 0=success, anything else is an error
    @retval -EPERM  if libsidekiq has not been initialized yet (through skiq_init(),
                    skiq_init_without_cards(), or skiq_init_by_serial_str())
    @retval -EINVAL if one of the specified card indices is out of range or refers to a
                    non-existent card
    @retval -E2BIG  if the number of cards specified exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
    @retval -EBUSY  if one or more of the specified cards is already in use (either by the
                    current process or another)
*/
EPIQ_API int32_t skiq_enable_cards(const uint8_t cards[],
                                   uint8_t num_cards,
                                   skiq_xport_init_level_t level);

/*****************************************************************************/
/** The skiq_enable_cards_by_serial_str() function is responsible for performing all initialization
    tasks for the specified Sidekiq cards.

    @attention  The Sidekiq library must have been previously initialized with skiq_init(),
                skiq_init_without_cards(), or skiq_init_by_serial_str(). The transport type is
                automatically selected based on availability.

    @since Function added in API @b v4.9.0

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_init_without_cards
    @see skiq_init_by_serial_str
    @see skiq_exit
    @see skiq_enable_cards
    @see skiq_disable_cards

    @param[in]  pp_serial_nums  pointer to the list of Sidekiq serial number strings to initialize
    @param[in]  num_cards       number of Sidekiq cards to initialize
    @param[in]  level           the transport functionality level of initialization that is
                                required:
      - ::skiq_xport_init_level_basic - minimal initialization necessary to bring
          up the requested transport interface for FPGA / RFIC register
          reads/writes, and initialize the mutexes that serializes access to
          libsidekiq
      - ::skiq_xport_init_level_full - Same as ::skiq_xport_init_level_basic and
          perform the complete bring up of all hardware (most applications
          concerned with sending/receiving RF will use this)
    @param[out] p_card_nums     pointer to the list of Sidekiq card indices corresponding with
                                serial strings provided; this list should be able to hold at
                                least ::SKIQ_MAX_NUM_CARDS entries

    @return int32_t  status where 0=success, anything else is an error
    @retval -EPERM  if libsidekiq has not been initialized yet (through skiq_init(),
                    skiq_init_without_cards(), or skiq_init_by_serial_str())
    @retval -E2BIG  if the number of cards specified exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
    @retval -ENXIO  if one of the specified serial numbers cannot be obtained
*/
EPIQ_API int32_t skiq_enable_cards_by_serial_str(const char **pp_serial_nums,
                                                 uint8_t num_cards,
                                                 skiq_xport_init_level_t level,
                                                 uint8_t *p_card_nums);

/*****************************************************************************/
/** The skiq_init_by_serial_str() function is identical to skiq_init() except a
    list of serial numbers can be requested instead of card indices.

    @since Function added in API @b v4.0.0

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_exit

    @param[in] type [::skiq_xport_type_t] the transport type that is required:
      - ::skiq_xport_type_auto - automatically detect and use available transport
      - ::skiq_xport_type_pcie - communicate with Sidekiq over PCIe.  If USB is
          available it will also be used for certain functionality.
      - ::skiq_xport_type_usb - communicate with Sidekiq entirely over USB.  A
          USB FPGA bitstream must be utilized if initializing at
          ::skiq_xport_init_level_full.
      - ::skiq_xport_type_custom - communicate with Sidekiq using the registered
          transport implementation provided by a call to
          skiq_register_custom_transport().  If USB is available, it will also
          be used for certain functionality.

    @param[in] level the transport functionality level of initialization that is required:
      - ::skiq_xport_init_level_basic - minimal initialization necessary to bring
          up the requested transport interface for FPGA / RFIC register
          reads/writes, and initialize the mutexes that serializes access to
          libsidekiq
      - ::skiq_xport_init_level_full - Same as ::skiq_xport_init_level_basic and
          perform the complete bring up of all hardware (most applications
          concerned with sending/receiving RF will use this)

    @param[in] pp_serial_nums pointer to the list of Sidekiq serial number strings to initialize
    @param[in] num_cards number of Sidekiq cards to initialize
    @param[out] p_card_nums pointer to the list of Sidekiq card indices corresponding with
    serial strings provided; this list should be able to hold at least ::SKIQ_MAX_NUM_CARDS
    entries

    @attention  As of libsidekiq v4.8.0, the @a type parameter is ignored as the
                transport type is automatically set to ::skiq_xport_type_auto, which
                will select the correct transport for the specified card(s).

    @attention  ::skiq_init(), ::skiq_init_without_cards(), and ::skiq_init_by_serial_str() should
                only be called when starting an application or after ::skiq_exit() has been
                called; these functions are not designed to be called multiple times to
                initialize individual cards.

    @return int32_t  status where 0=success, anything else is an error
    @retval -EEXIST libsidekiq has already been initialized in this application without
                    skiq_exit() being called
    @retval -E2BIG  if the number of cards requested exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
    @retval -ENXIO  if one of the specified serial numbers cannot be found
*/
EPIQ_API int32_t skiq_init_by_serial_str(skiq_xport_type_t type,
                                         skiq_xport_init_level_t level,
                                         char **pp_serial_nums,
                                         uint8_t num_cards,
                                         uint8_t *p_card_nums);

/*****************************************************************************/
/** The skiq_init_without_cards() function initializes the library (like ::skiq_init()) without
    having to specify any cards. This is useful when using cards dynamically via the
    ::skiq_enable_cards() / ::skiq_disable_cards() functions.

    @attention  ::skiq_init(), ::skiq_init_without_cards(), and ::skiq_init_by_serial_str() should
                only be called when starting an application or after ::skiq_exit() has been
                called; these functions are not designed to be called multiple times.

    @since  Function added in API @b v4.13.0

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_init_by_serial_str
    @see skiq_enable_cards
    @see skiq_disable_cards
    @see skiq_exit

    @return int32_t     status where 0 = success, anything else is an error
    @retval -EEXIST     libsidekiq has already been initialized in this application without
                        ::skiq_exit() being called
*/
EPIQ_API int32_t skiq_init_without_cards(void);

/*****************************************************************************/
/** The skiq_read_parameters() function is used for populating the ::skiq_param_t
    struct for a given card. This structure can be queried for various values
    relating to the card. For further information regarding that structure,
    reference the documentation provided in sidekiq_params.h.

    @note The initialization level influences what can be populated in the
    structure. This is fully documented in skiq_params.h.

    @since Function added in API @b v4.4.0

    @ingroup cardfunctions

    @param[in]  card     card index of the Sidekiq of interest
    @param[out] p_param  [::skiq_param_t] pointer to structure to be populated.

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EFAULT     if @p p_param is NULL
    @retval -EPROTO     if an internal error is detected
*/
EPIQ_API int32_t skiq_read_parameters( uint8_t card,
                                       skiq_param_t* p_param );

/*****************************************************************************/
/** The skiq_is_xport_avail() function is responsible for determining if the
    requested transport type is available for the card index specified.

    @since Function added in API @b v4.0.0

    @ingroup cardfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] type transport type to check for card specified

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_is_xport_avail( uint8_t card,
                                      skiq_xport_type_t type );

/*****************************************************************************/
/** The skiq_is_card_avail() function is responsible for determining if the
    requested card is currently available and free for use.  If the card is
    already locked, the process ID of the current card owner is provided.

    @note This only reflects the instantaneous availability of the Sidekiq card
    and does not reserve any resources for future use.

    @note If a card is locked by another thread within the current process,
    the process ID (PID) returned in p_card_owner can be the PID of the current process.

    @since Function added in API @b v4.0.0

    @ingroup cardfunctions

    @param[in]  card            card index of the Sidekiq of interest
    @param[out] p_card_owner    is a pointer where the process ID of the current
    card owner is provided (only if the card is already locked). May be NULL
    if the caller does not require the information; if not NULL, this value is
    set if the function returns 0 or EBUSY.

    @return int32_t status code
    @retval -ERANGE if the specified card index exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
    @retval -ENODEV if a card was not detected at the specified card index
    @retval 0       if the card is available
    @retval EBUSY   if the specified card is not available (already in use)
    @retval non-zero Unspecified error occurred
*/
#if defined(WIN32) && !defined(__MINGW32__)
#ifndef pid_t
typedef intmax_t pid_t;
#endif
#endif /* WIN32 */
EPIQ_API int32_t skiq_is_card_avail( uint8_t card,
                                     pid_t *p_card_owner );

/*****************************************************************************/
/** The skiq_exit() function is responsible for performing all shutdown tasks
    for libsidekiq.  It should be called once when the associated application
    is closing.

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_init_by_serial_str

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_exit(void);

/*****************************************************************************/
/** The skiq_disable_cards() function is responsible for performing all shutdown tasks
    for the specified Sidekiq card(s).  This does not perform the various shutdown
    tasks for all of libsidekiq, only for the card(s) specified.

    @since Function added in API @b v4.8.0

    @attention  The Sidekiq library must have been previously initialized with:
                    - skiq_init(),
                    - skiq_init_without_cards(),
                    - or skiq_init_by_serial_str()
                and the specified card(s) must have been initialized with either:
                    - skiq_init(),
                    - skiq_init_by_serial_str(),
                    - skiq_enable_cards(), or
                    - skiq_enable_cards_by_serial_str().

    @attention  This function does not automatically release all libsidekiq resources
                if all cards are disabled; if libsidekiq is no longer needed,
                ::skiq_exit() must be called to perform a clean shutdown of the library.

    @ingroup cardfunctions
    @see skiq_init
    @see skiq_init_by_serial_str
    @see skiq_enable_cards
    @see skiq_enable_cards_by_serial_str

    @param[in] cards array of Sidekiq cards to be disabled
    @param[in] num_cards number of Sidekiq cards to disabled

    @return int32_t status where 0=success, anything else is an error
    @retval -EPERM  if libsidekiq has not been initialized yet (through skiq_init(),
                    skiq_init_without_cards(), or skiq_init_by_serial_str())
    @retval -E2BIG  if the number of cards requested exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
    @retval -EINVAL if one of the specified card indices is out of range or refers to a
                    non-existent card
*/
EPIQ_API int32_t skiq_disable_cards(const uint8_t cards[],
                                    uint8_t num_cards);

/*****************************************************************************/
/** The skiq_read_rx_streaming_handles() function is responsible for providing
    a list of RX handles currently streaming.

    @ingroup rxfunctions

    @since Function added in @b v4.9.0

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_hdls_streaming [::skiq_rx_hdl_t] array of handles currently streaming
    @param[out] p_num_hdls pointer of where to store number of handles in streaming list

    @return int32_t
    @retval 0 p_hdls_streaming populated with RX handles currently streaming
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_read_rx_streaming_handles(uint8_t card,
                                                skiq_rx_hdl_t *p_hdls_streaming,
                                                uint8_t *p_num_hdls );

/*****************************************************************************/
/** The skiq_read_rx_stream_handle_conflict() function is responsible for providing
    a list of RX handles that cannot be streaming simultaneous to the handle
    specified.  If streaming is requested with a conflicting handle, the stream
    cannot be started.

    @ingroup rxfunctions

    @since Function added in @b v4.9.0

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl_to_stream [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_conflicting_hdls [::skiq_rx_hdl_t] array of handles that conflict.
                Must be large enough to contain ::skiq_rx_hdl_end elements.
    @param[out] p_num_hdls pointer of where to store number of handles in conflict list

    @return int32_t
    @retval 0 p_hdls_streaming populated with RX handles currently streaming
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EINVAL Error occurred reading conflicting handles
    @retval non-zero other error occurred
*/
EPIQ_API int32_t skiq_read_rx_stream_handle_conflict(uint8_t card,
                                                     skiq_rx_hdl_t hdl_to_stream,
                                                     skiq_rx_hdl_t *p_conflicting_hdls,
                                                     uint8_t *p_num_hdls);

/*****************************************************************************/
/** The skiq_start_rx_streaming() function is responsible for starting the
    flow of data between the FPGA and the CPU.  This function triggers the
    FPGA to start receiving data and transferring it to the CPU.  A continuous
    flow of packets will be transferred from the FPGA to the CPU until the user
    app calls skiq_stop_rx_streaming().  These packets will be received by the
    user app by calling skiq_receive(), which returns one packet at a time.

    This function call is functionally equivalent to:
    <pre>
    skiq_start_rx_streaming_multi_on_trigger( card, &hdl, 1,
                                              skiq_trigger_src_immediate,
                                              0 )
    </pre>

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface

    @return int32_t
    @retval 0 successful start streaming for handle specified
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -EBUSY One of the specified handles is already streaming
    @retval -EBUSY A conflicting handle is already streaming
    @retval -ENOTSUP Configured RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL Configured RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
    @retval -EPERM I/Q packed mode is already enabled and conflicts with the requested RX stream mode
    @retval -EIO Failed to start streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOSYS Transport does not support FPGA register access
    @retval non-zero An unspecified error occurred
*/
EPIQ_API int32_t skiq_start_rx_streaming(uint8_t card,
                                         skiq_rx_hdl_t hdl);

/*****************************************************************************/
/** The skiq_start_rx_streaming_multi_immediate() function allows a user to
    start multiple receive streams immediately (not necessarily
    timestamp-synchronized depending on FPGA support and library support).

    @warning If one of the receive handles is already streaming then this
    function returns an error.

    This function call is functionally equivalent to:
    <pre>
    skiq_start_rx_streaming_multi_on_trigger( card, handles, nr_handles,
                                              skiq_trigger_src_immediate,
                                              0 )
    </pre>

    @since Function added in @b v4.5.0

    @ingroup rxfunctions

    @param[in] card  card index of the Sidekiq of interest
    @param[in] handles  [array of ::skiq_rx_hdl_t] the receive handles to start streaming
    @param[in] nr_handles  the number of entries in handles[]

    @return int32_t
    @retval 0 successful start streaming for handle specified
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -EBUSY One of the specified handles is already streaming
    @retval -EBUSY A conflicting handle is already streaming
    @retval -ENOTSUP Configured RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL Configured RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
    @retval -EPERM I/Q packed mode is already enabled and conflicts with the requested RX stream mode
    @retval -EIO Failed to start streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOSYS Transport does not support FPGA register access
    @retval non-zero An unspecified error occurred
 */
EPIQ_API int32_t skiq_start_rx_streaming_multi_immediate( uint8_t card,
                                                          skiq_rx_hdl_t handles[],
                                                          uint8_t nr_handles );



/*************************************************************************************************/
/** The skiq_start_rx_streaming_multi_synced() function allows a user to start multiple receive
    streams immediately and with timestamp synchronization (not necessarily phase coherent however).

    @warning If one of the receive handles is already streaming then this function returns an error.

    @attention Not all Sidekiq products support the use of this function.

    @since Function added in @b v4.9.0, requires FPGA bitstream @b v3.11.0 or greater

    @ingroup rxfunctions

    @param[in] card  card index of the Sidekiq of interest
    @param[in] handles  [array of ::skiq_rx_hdl_t] the receive handles to start streaming
    @param[in] nr_handles  the number of entries in handles[]

    @return int32_t
    @retval 0 successful start streaming for handle specified
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -EBUSY One of the specified handles is already streaming
    @retval -EBUSY A conflicting handle is already streaming
    @retval -ENOTSUP Configured RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL Configured RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
    @retval -EPERM I/Q packed mode is already enabled and conflicts with the requested RX stream mode
    @retval -EIO Failed to start streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOSYS Transport does not support FPGA register access
    @retval -ENOTSUP the ::skiq_trigger_src_synced trigger source is not supported for the given Sidekiq product or FPGA bitstream
    @retval non-zero An unspecified error occurred
 */
EPIQ_API int32_t skiq_start_rx_streaming_multi_synced( uint8_t card,
                                                       skiq_rx_hdl_t handles[],
                                                       uint8_t nr_handles );


/*****************************************************************************/
/** The skiq_start_rx_streaming_on_1pps() function is identical to the
    skiq_start_rx_streaming() with exception of when the data stream starts to
    flow.  When calling this function, the data does not begin to flow until the
    rising @ref ppsfunctions "1PPS" edge after the system timestamp specified
    has occurred.  If a timestamp of 0 is provided, then the next @ref
    ppsfunctions "1PPS" edge will begin the data flow. This function blocks
    until the data starts flowing.

    This function call is functionally equivalent to:
    <pre>
    skiq_start_rx_streaming_multi_on_trigger( card, &hdl, 1,
                                              skiq_trigger_src_1pps,
                                              sys_timestamp )
    </pre>

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] sys_timestamp system timestamp after the next @ref ppsfunctions "1PPS" will begin the data flow

    @return int32_t
    @retval 0 successful start streaming for handle specified
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -EBUSY One of the specified handles is already streaming
    @retval -EBUSY A conflicting handle is already streaming
    @retval -ENOTSUP Configured RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL Configured RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
    @retval -EPERM I/Q packed mode is already enabled and conflicts with the requested RX stream mode
    @retval -EIO Failed to start streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOSYS Transport does not support FPGA register access
    @retval non-zero An unspecified error occurred
*/
EPIQ_API int32_t skiq_start_rx_streaming_on_1pps(uint8_t card,
                                                 skiq_rx_hdl_t hdl,
                                                 uint64_t sys_timestamp);

/*****************************************************************************/
/** The skiq_start_rx_streaming_multi_on_trigger() function allows a user to
    start multiple receive streams after the specified trigger occurs.

    @warning If one of the receive handles is already streaming then this
    function returns an error.

    @attention If ::skiq_trigger_src_1pps is used as a trigger then this
    function will @b block until the 1PPS edge occurs.

    @since Function added in @b v4.5.0

    @ingroup rxfunctions

    @param[in] card  card index of the Sidekiq of interest
    @param[in] handles  [array of ::skiq_rx_hdl_t] the receive handles to start streaming
    @param[in] nr_handles  the number of entries in handles[]
    @param[in] trigger [::skiq_trigger_src_t] type of trigger to use
    @param[in] sys_timestamp System Timestamp after the next positive trigger will begin the data flow

    @return int32_t
    @retval 0 successful start streaming for handle specified
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -EBUSY One of the specified handles is already streaming
    @retval -EBUSY A conflicting handle is already streaming
    @retval -ENOTSUP Configured RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL Configured RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
    @retval -EPERM I/Q packed mode is already enabled and conflicts with the requested RX stream mode
    @retval -EIO Failed to start streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOSYS Transport does not support FPGA register access
    @retval non-zero An unspecified error occurred
 */
EPIQ_API int32_t skiq_start_rx_streaming_multi_on_trigger( uint8_t card,
                                                           skiq_rx_hdl_t handles[],
                                                           uint8_t nr_handles,
                                                           skiq_trigger_src_t trigger,
                                                           uint64_t sys_timestamp );

/*****************************************************************************/
/** The skiq_start_tx_streaming() function is responsible for preparing for the
    flow of data between the CPU and the FPGA.  Once started, the data flow can
    be stopped with a call to skiq_stop_tx_streaming().

    The total size of the transmit packet must be in an increment of
    ::SKIQ_TX_PACKET_SIZE_INCREMENT_IN_WORDS.  The packet size is calculated by:
    block_size + header_size.  If this condition is not met, an error will be
    returned and the transmit stream will not begin.

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl the handle of the tx interface to start streaming
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_start_tx_streaming(uint8_t card,
                                         skiq_tx_hdl_t hdl);

/*****************************************************************************/
/** The skiq_start_tx_streaming_on_1pps() function is identical to the
    skiq_start_tx_streaming() with exception of when the data stream starts to
    flow.  When calling this function, the data does not begin to flow until the
    rising @ref ppsfunctions "1PPS" edge after the system timestamp specified
    has occurred.  If a timestamp of 0 is provided, then the next @ref
    ppsfunctions "1PPS" edge will begin the data flow. This function blocks
    until the data starts flowing.

    @ingroup txfunctions

    The total size of the transmit packet must be in an increment of
    ::SKIQ_TX_PACKET_SIZE_INCREMENT_IN_WORDS.  The packet size is calculated by:
    block_size + header_size.  If this condition is not met, an error will be
    returned and the transmit stream will not begin.

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] sys_timestamp system timestamp after the next @ref ppsfunctions "1PPS" will begin the data flow
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_start_tx_streaming_on_1pps(uint8_t card,
                                                 skiq_tx_hdl_t hdl,
                                                 uint64_t sys_timestamp);

/*****************************************************************************/
/** The skiq_stop_rx_streaming() function is responsible for stopping the
    streaming of data between the FPGA and the CPU.  This function can only
    be called after an interface has previously started streaming.

    This function call is functionally equivalent to:
    <pre>
    skiq_stop_rx_streaming_multi_on_trigger( card, &hdl, 1,
                                             skiq_trigger_src_immediate,
                                             0 )
    </pre>

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface

    @return int32_t
    @retval 0 Success
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -ENODEV One of the specified handles is not currently streaming
    @retval -EIO Failed to stop streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_stop_rx_streaming(uint8_t card, skiq_rx_hdl_t hdl);

/*****************************************************************************/
/** The skiq_stop_rx_streaming_multi_immediate() function allows a user to stop
    multiple receive streams immediately (not necessarily timestamp-synchronized
    depending on FPGA support and library support).

    @warning If one of the receive handles is not streaming then this function
    returns an error.

    This function call is functionally equivalent to:
    <pre>
    skiq_stop_rx_streaming_multi_on_trigger( card, handles, nr_handles,
                                             skiq_trigger_src_immediate,
                                             0 )
    </pre>

    @since Function added in @b v4.5.0

    @ingroup rxfunctions

    @param[in] card  card index of the Sidekiq of interest
    @param[in] handles  [array of ::skiq_rx_hdl_t] the receive handles to start streaming
    @param[in] nr_handles  the number of entries in handles[]

    @return int32_t
    @retval 0 Success
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -ENODEV One of the specified handles is not currently streaming
    @retval -EIO Failed to stop streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval non-zero Unspecified error occurred
 */
EPIQ_API int32_t skiq_stop_rx_streaming_multi_immediate( uint8_t card,
                                                         skiq_rx_hdl_t handles[],
                                                         uint8_t nr_handles );


/*************************************************************************************************/
/** The skiq_stop_rx_streaming_multi_synced() function allows a user to stop multiple receive
    streams immediately and with timestamp synchronization (not necessarily phase coherent however).

    @warning If one of the receive handles is not streaming then this function returns an error.

    @attention Not all Sidekiq products support this function.

    @since Function added in @b v4.9.0, requires FPGA bitstream @b v3.11.0 or greater

    @ingroup rxfunctions

    @param[in] card  card index of the Sidekiq of interest
    @param[in] handles  [array of ::skiq_rx_hdl_t] the receive handles to start streaming
    @param[in] nr_handles  the number of entries in handles[]

    @return int32_t
    @retval 0 Success
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -ENODEV One of the specified handles is not currently streaming
    @retval -EIO Failed to stop streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOTSUP the ::skiq_trigger_src_synced trigger source is not supported for the given Sidekiq product or FPGA bitstream
    @retval non-zero Unspecified error occurred
 */
EPIQ_API int32_t skiq_stop_rx_streaming_multi_synced( uint8_t card,
                                                      skiq_rx_hdl_t handles[],
                                                      uint8_t nr_handles );


/*****************************************************************************/
/** The skiq_stop_rx_streaming_on_1pps() function stops the data from flowing on
    the rising edge of the @ref ppsfunctions "1PPS" after the timestamp
    specified.  If a timestamp of 0 is provided, then the next @ref ppsfunctions
    "1PPS" edge will stop the data flow.  This function blocks until the data
    stream has been stopped.

    @note this stops the data at the FPGA.  However, there will be data
    remaining in the internal FIFOs, so skiq_receive() should continue to be
    called until no data remains.  Once that is complete, the
    skiq_stop_rx_streaming() function should be called to finalize the disabling
    of the data flow.

    This function call is functionally equivalent to:
    <pre>
    skiq_stop_rx_streaming_multi_on_trigger( card, &hdl, 1,
                                             skiq_trigger_src_1pps,
                                             sys_timestamp )
    </pre>

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] sys_timestamp system timestamp after the next @ref ppsfunctions "1PPS" will stop the data flow

    @return int32_t
    @retval 0 Success
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -ENODEV One of the specified handles is not currently streaming
    @retval -EIO Failed to stop streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_stop_rx_streaming_on_1pps(uint8_t card,
                                                skiq_rx_hdl_t hdl,
                                                uint64_t sys_timestamp);


/*****************************************************************************/
/** The skiq_stop_rx_streaming_multi_on_trigger() function allows a user to
    stop multiple receive streams after the specified trigger occurs.

    @warning If one of the receive handles is not streaming then this
    function returns an error.

    @attention If ::skiq_trigger_src_1pps is used as a trigger then this
    function will @b block until the 1PPS edge occurs.

    @since Function added in @b v4.5.0

    @ingroup rxfunctions

    @param[in] card  card index of the Sidekiq of interest
    @param[in] handles  [array of ::skiq_rx_hdl_t] the receive handles to stop streaming
    @param[in] nr_handles  the number of entries in handles[]
    @param[in] trigger [::skiq_trigger_src_t] type of trigger to use
    @param[in] sys_timestamp System Timestamp after the next positive trigger will stop the data flow

    @return int32_t
    @retval 0 Success
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -EINVAL Invalid parameter passed (nr_handles < 1, etc)
    @retval -ENODEV One of the specified handles is not currently streaming
    @retval -EIO Failed to stop streaming for given transport
    @retval -ECOMM Communication error occurred transacting with FPGA registers
    @retval -ENOTSUP the ::skiq_trigger_src_synced trigger source is not supported for the given Sidekiq product or FPGA bitstream
    @retval non-zero Unspecified error occurred
 */
EPIQ_API int32_t skiq_stop_rx_streaming_multi_on_trigger( uint8_t card,
                                                          skiq_rx_hdl_t handles[],
                                                          uint8_t nr_handles,
                                                          skiq_trigger_src_t trigger,
                                                          uint64_t sys_timestamp );

/*****************************************************************************/
/** The skiq_stop_tx_streaming() function is responsible for stopping the
    streaming of data between the CPU and the FPGA.  This function can only
    be called after an interface has previously started streaming.

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl  the handle of the requested tx interface
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_stop_tx_streaming(uint8_t card,
                                        skiq_tx_hdl_t hdl);

/*****************************************************************************/
/** The skiq_stop_tx_streaming_on_1pps() function is identical to the
    skiq_stop_tx_streaming() function with the exception of when the data stops
    streaming.  When calling this function, the data stream is disabled on the
    rising @ref ppsfunctions "1PPS" edge after the system timestamp specified
    has occurred.  If a timestamp of 0 is provided, then the next @ref
    ppsfunctions "1PPS" edge will stop the data flow.  This function blocks
    until the data flow is disabled.

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl  the handle of the requested tx interface
    @param[in] sys_timestamp specifies the timestamp on which to stop TX streaming
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_stop_tx_streaming_on_1pps(uint8_t card,
                                                skiq_tx_hdl_t hdl,
                                                uint64_t sys_timestamp);

/**************************************************************************************************/
/**
    @brief The skiq_read_last_1pps_timestamp() function is responsible for returning the RF and
    System timestamps of when the last @ref ppsfunctions "1PPS" timestamp occurred.

    @note A user may pass @a NULL to @p p_rf_timestamp or @p p_sys_timestamp if the user is not
    interested in the value.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup ppsfunctions

    @param[in] card              requested Sidekiq card ID
    @param[out] p_rf_timestamp   a uint64_t pointer where the value of the RF timestamp when the last @ref ppsfunctions "1PPS" occurred, may be NULL
    @param[out] p_sys_timestamp  a uint64_t pointer where the value of the System timestamp when the last @ref ppsfunctions "1PPS" occurred, may be NULL

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
    @retval -ERANGE     if timestamps could not be validated to be from the same 1PPS period
*/
EPIQ_API int32_t skiq_read_last_1pps_timestamp( uint8_t card,
                                                uint64_t* p_rf_timestamp,
                                                uint64_t* p_sys_timestamp );

/*****************************************************************************/
/** The skiq_write_timestamp_reset_on_1pps() function is responsible for
    configuring the FPGA to reset all the timestamps at a well defined
    point in the future.  This point in the future is the occurrence of a
    1PPS AFTER the specified system timestamp.

    @ingroup ppsfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] future_sys_timestamp the value of the system timestamp of a well
    defined point in the future, where the next 1PPS signal after this timestamp
    value will cause the timestamp to reset back to 0
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_timestamp_reset_on_1pps(uint8_t card,
                                                    uint64_t future_sys_timestamp);

/*****************************************************************************/
/** The skiq_write_timestamp_update_on_1pps() function is responsible for
    configuring the FPGA to set all timestamps to a specific value at a well
    defined point in the future.  This point in the future is the occurrence of
    a 1PPS AFTER the specified system timestamp.

    @ingroup ppsfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] future_sys_timestamp the value of the system timestamp of a well
    defined point in the future, where the next 1PPS signal after this timestamp
    value will cause the timestamp to update to the value specified
    @param[in] new_timestamp the value to set all timestamps to after the 1PPS
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_timestamp_update_on_1pps(uint8_t card,
                                                     uint64_t future_sys_timestamp,
                                                     uint64_t new_timestamp);

/*****************************************************************************/
/** The skiq_read_tx_timestamp_base() function is responsible for returning the
    current timestamp base for transmitting on timestamp.

    @see skiq_tx_timestamp_base_t
    @see skiq_write_tx_timestamp_base

    @since Function added in API @b v4.16.0

    @ingroup txfunctions

    @param[in]  card                card index of the Sidekiq of interest
    @param[out] p_timestamp_base    [::skiq_tx_timestamp_base_t] a pointer to
    the current timestamp base configuration

    @return 0 on success, else a negative errno value
    @retval -ENOSYS     if the FPGA version does not meet minimum requirements
                        to support this feature.
    @retval -EFAULT  NULL pointer detected for @p p_timestamp_base
*/
EPIQ_API int32_t skiq_read_tx_timestamp_base(uint8_t card, skiq_tx_timestamp_base_t* p_timestamp_base);

/*****************************************************************************/
/** The skiq_write_tx_timestamp_base() function is responsible for configuring
    the timestamp base for transmitting on timestamp.

    @see skiq_tx_timestamp_base_t
    @see skiq_read_tx_timestamp_base

    @since Function added in API @b v4.16.0

    @note This functionality is not supported on older Sidekiq mPCIe products,
    please contact the support forum if you have any questions about supported
    products.

    @ingroup txfunctions

    @param[in]  card            card index of the Sidekiq of interest
    @param[in]  timestamp_base  [::skiq_tx_timestamp_base_t] timestamp base
    configuration desired

    @return 0 on success, else a negative errno value
    @retval -ENOTSUP    if the Sidekiq card does not support changing the
                        base.
    @retval -ENOSYS     if the FPGA version does not meet minimum requirements
                        to support this feature.
    @retval -EFAULT  NULL pointer detected for @p p_timestamp_base
*/
EPIQ_API int32_t skiq_write_tx_timestamp_base(uint8_t card, skiq_tx_timestamp_base_t timestamp_base);

/*****************************************************************************/
/** The skiq_read_tx_data_flow_mode() function is responsible for returning the
    current data flow mode for the Tx interface; this can be one of the
    following:

    - ::skiq_tx_immediate_data_flow_mode, where timestamps are ignored, and data
    is transmitted as soon as possible.
    - ::skiq_tx_with_timestamps_data_flow_mode, where the FPGA will ensure that
    the data is sent at the appropriate timestamp.
    - ::skiq_tx_with_timestamps_allow_late_data_flow_mode, where the FPGA will
    ensure that the data is sent at the appropriate timestamp, but will also
    send data with timestamps that have already passed.

    @note   With ::skiq_tx_with_timestamps_data_flow_mode, if data arrives
    when the FPGA's timestamp is greater than the data's associated timestamp,
    the data is considered late and not transmitted. This is not the case with
    ::skiq_tx_with_timestamps_allow_late_data_flow_mode, which will allow late
    data to be transmitted.

    @ingroup txfunctions

    @param[in]  card    card index of the Sidekiq of interest
    @param[in]  hdl     the handle of the Tx interface of interest
    @param[out] p_mode  a pointer to where the current data flow mode will be written

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_data_flow_mode(uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             skiq_tx_flow_mode_t* p_mode);

/*****************************************************************************/
/** The skiq_write_tx_data_flow_mode() function is responsible for updating the
    current data flow mode for the interface; this can be one of the following:

    - ::skiq_tx_immediate_data_flow_mode, where timestamps are ignored, and data
    is transmitted as soon as possible.
    - ::skiq_tx_with_timestamps_data_flow_mode, where the FPGA will ensure that
    the data is sent at the appropriate timestamp.
    - ::skiq_tx_with_timestamps_allow_late_data_flow_mode, where the FPGA will
    ensure that the data is sent at the appropriate timestamp, but will also
    send data with timestamps that have already passed.

    @note   The data flow modes can be changed at any time, but updates are only
    honored whenever an interface is started through the
    skiq_start_tx_interface() call.

    @note   With ::skiq_tx_with_timestamps_data_flow_mode, if data arrives
    when the FPGA's timestamp is greater than the data's associated timestamp,
    the data is considered late and not transmitted. This is not the case with
    ::skiq_tx_with_timestamps_allow_late_data_flow_mode, which will allow late
    data to be transmitted.

    @attention  ::skiq_tx_with_timestamps_allow_late_data_flow_mode is only
    available on certain bitstreams; if this mode is set and the card's
    bitstream doesn't support it, -ENOTSUP is returned.

    @attention  The late timestamp counter is not updated when in
    ::skiq_tx_with_timestamps_allow_late_data_flow_mode, even if the data is
    transmitted later than its timestamp.

    @ingroup txfunctions

    @param[in]  card    card index of the Sidekiq of interest
    @param[in]  hdl     the handle of the requested Tx interface
    @param[in]  mode    the requested data flow mode

    @return int32_t  status where 0=success, anything else is an error

    @retval -ENOTSUP    if ::skiq_tx_with_timestamps_allow_late_data_flow_mode
                        TX data flow mode is selected and the currently loaded
                        bitfile on the selected card does not support that
                        feature.
    @retval -EPERM     if ::skiq_tx_with_timestamps_allow_late_data_flow_mode
                        TX data flow mode is not selected and the current config
                        for the timestamp base is set to use system timestamps
*/
EPIQ_API int32_t skiq_write_tx_data_flow_mode(uint8_t card,
                                              skiq_tx_hdl_t hdl,
                                              skiq_tx_flow_mode_t mode);

/*****************************************************************************/
/** The skiq_read_tx_transfer_mode() function is responsible for returning the
    current transfer mode (::skiq_tx_transfer_mode_t) for the Tx interface.
    This can be either tx synchronous or asynchronous.  With
    ::skiq_tx_transfer_mode_sync, the skiq_transmit() call blocks until the
    packet has been received by the FPGA.  With ::skiq_tx_transfer_mode_async,
    the skiq_transmit() will accept the packet immediately as long as there is
    adequate space within the buffer to store the block.  With
    ::skiq_tx_transfer_mode_async, a callback function (see
    skiq_register_tx_complete_callback() for details) can be registered to
    notify the application when the transfer to the FPGA has been completed.

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl  the handle of the Tx interface of interest
    @param[out] p_transfer_mode  a pointer to where the current transfer mode will be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_transfer_mode( uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             skiq_tx_transfer_mode_t *p_transfer_mode );

/*****************************************************************************/
/** The skiq_write_tx_transfer_mode() function is responsible for updating the
    current transfer mode (::skiq_tx_transfer_mode_t) for the Tx interface.
    Note that this can only be changed if the transmit interface is not
    currently streaming.  If a mode change is attempted while streaming, an
    error will be returned.  With ::skiq_tx_transfer_mode_sync, the
    skiq_transmit() call blocks until the packet has been received by the FPGA.
    With ::skiq_tx_transfer_mode_async, a call to skiq_transmit() will accept
    the packet immediately as long as there is adequate space within the buffer
    to store the block.  With ::skiq_tx_transfer_mode_async, a callback function
    (see skiq_register_tx_complete_callback() for details) can be registered to
    notify the application when the transfer to the FPGA has been completed.

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl  the handle of the requested Tx interface
    @param[in] transfer_mode  the requested transfer flow mode
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_transfer_mode( uint8_t card,
                                              skiq_tx_hdl_t hdl,
                                              skiq_tx_transfer_mode_t transfer_mode );

/*****************************************************************************/
/** The skiq_register_tx_complete_callback() function registers a callback
    function that should be called when the transfer of a packet at the
    address provided has been completed.  Once the callback function is called
    the memory location specified by p_data has completed processing.

    @note This callback function is used only when the transmit transfer mode is
    @ref skiq_tx_transfer_mode_async.

    @since Function signature modified since @b v4.0.0 to add private data
    pointer in callback, see @ref skiq_tx_callback_t for more details.

    @ingroup txfunctions
    @see skiq_register_tx_enabled_callback

    @param[in] card card index of the Sidekiq of interest
    @param[in] tx_complete pointer to function to call when a packet has finished transfer
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_register_tx_complete_callback( uint8_t card,
                                                     skiq_tx_callback_t tx_complete );

/*****************************************************************************/
/** The skiq_register_tx_enabled_callback() function registers a callback
    function that should be called when the transmit FIFO is enabled and
    available to queue packets.

    @since Function added in API @b v4.3.0

    @ingroup txfunctions
    @see skiq_register_tx_complete_callback

    @param[in] card card index of the Sidekiq of interest
    @param[in] tx_ena_cb [::skiq_tx_ena_callback_t] pointer to function to call when FIFO is enabled
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_register_tx_enabled_callback( uint8_t card,
                                                    skiq_tx_ena_callback_t tx_ena_cb );

/*****************************************************************************/
/** The skiq_read_chan_mode() function is responsible for returning the
    current Rx channel mode (::skiq_chan_mode_t) setting.

    @ingroup rxfunctions
    @see skiq_write_chan_mode

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_mode pointer to where to store the Rx channel mode setting
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_chan_mode(uint8_t card,
                                     skiq_chan_mode_t *p_mode);

/*****************************************************************************/
/** The skiq_write_chan_mode() function is responsible for configuring the
    channel mode.  If only A1 is needed for receiving or if transmit is being
    used it is recommended to configure the mode to ::skiq_chan_mode_single.  If
    A2 is being used as a receiver or if both A1 and A2 are being used as
    receivers, than the mode should be configured to ::skiq_chan_mode_dual.

    @ingroup rxfunctions
    @see skiq_read_chan_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] mode specifies the Rx channel mode setting
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_chan_mode(uint8_t card,
                                      skiq_chan_mode_t mode);

/*****************************************************************************/
/** The skiq_write_rx_preselect_filter_path() function is responsible for
    selecting from any ::skiq_filt_t value appropriate for the Sidekiq hardware
    on the specified Rx interface.

    @note Not all filter options are available for hardware variants.  Users may
    use skiq_read_rx_filters_avail() to determine RF filter path available for a
    given Sidekiq card.

    @ingroup rxfunctions
    @see skiq_read_rx_filters_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] path  an enum indicating which path is being requested
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_preselect_filter_path(uint8_t card,
                                                     skiq_rx_hdl_t hdl,
                                                     skiq_filt_t path);

/*****************************************************************************/
/** The skiq_read_rx_preselect_filter_path() function is responsible for
    returning the currently selected RF filter path (of type ::skiq_filt_t) on
    the specified Rx interface.

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_path  a pointer to where the current value of the filter path
    should be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_preselect_filter_path(uint8_t card,
                                                    skiq_rx_hdl_t hdl,
                                                    skiq_filt_t* p_path);

/*****************************************************************************/
/** The skiq_write_tx_filter_path() function is responsible for selecting from
    any ::skiq_filt_t value appropriate for the Sidekiq hardware on the
    specified Tx interface.

    @note Not all filter options are available for hardware variants.  Users may
    use skiq_read_tx_filters_avail() to determine RF filter path available for a
    given Sidekiq card.

    @ingroup txfunctions
    @see skiq_read_tx_filters_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl  the handle of the requested tx interface
    @param[in] path  an enum indicating which path is being requested
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_filter_path(uint8_t card,
                                           skiq_tx_hdl_t hdl,
                                           skiq_filt_t path);

/*****************************************************************************/
/** The skiq_read_tx_filter_path() function is responsible for returning the
    currently selected RF path on the specified Tx interface.

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl  the handle of the requested tx interface
    @param[out] p_path  a pointer to where the current value of the filter path
    should be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_filter_path(uint8_t card,
                                          skiq_tx_hdl_t hdl,
                                          skiq_filt_t* p_path);

/*****************************************************************************/
/** The skiq_read_rx_overload_state() function is responsible for reporting the
    overload state of the specified Rx interface.  An overload condition is
    detected when an RF input in excess of 0dBm is detected.  If an overload
    condition is detected, the state is 1, otherwise it is 0.

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_overload a pointer to where to store the overload state.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_overload_state( uint8_t card,
                                              skiq_rx_hdl_t hdl,
                                              uint8_t *p_overload );

/*****************************************************************************/
/** The skiq_read_rx_LO_freq() function reads the current setting for the LO
    frequency of the specified Rx interface.

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] the handle of the requested rx interface
    @param[out] p_freq  a pointer to the variable that should be updated with
    the programmed frequency (in Hertz)
    @param[out] p_actual_freq  a pointer to the variable that should be updated with
    the actual tuned frequency (in Hertz)

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid RX handle specified
    @retval -ENODATA RX LO frequency has not yet been configured
*/
EPIQ_API int32_t skiq_read_rx_LO_freq(uint8_t card,
                                      skiq_rx_hdl_t hdl,
                                      uint64_t* p_freq,
                                      double* p_actual_freq );

/*****************************************************************************/
/** The skiq_write_rx_LO_freq() function writes the current setting for the LO
    frequency of the specified Rx interface.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] freq  the new value for the LO freq (in Hertz)
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_LO_freq(uint8_t card,
                                       skiq_rx_hdl_t hdl,
                                       uint64_t freq);

/*****************************************************************************/
/** The skiq_read_rx_sample_rate() function reads the current setting for the
    rate of received samples being transferred into the FPGA from the RFIC.

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_rate  a pointer to the variable that should be updated with
    the current sample rate setting (in Hertz) currently set for the specified
    interface
    @param[out] p_actual_rate  a pointer to the variable that should be updated with
    the actual rate of received samples being transferred into the FPGA
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_sample_rate(uint8_t card,
                                          skiq_rx_hdl_t hdl,
                                          uint32_t *p_rate,
                                          double* p_actual_rate);

/*****************************************************************************/
/** The skiq_write_rx_sample_rate_and_bandwidth() function writes the current
    setting for the rate of received samples being transferred into the FPGA
    from the RFIC.  Additionally, the channel bandwidth is also configured.

    @note  When configuring multiple handles,
    ::skiq_write_rx_sample_rate_and_bandwidth_multi() is preferred since it offers 
    better performance compared to multiple calls to 
    ::skiq_write_rx_sample_rate_and_bandwidth().

    @warning Rx/Tx sample rates are derived from the same clock so modifications
    to the Rx sample rate will also update the Tx sample rate to the same value.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup rxfunctions
    @see skiq_write_rx_sample_rate_and_bandwidth_multi
    
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] rate  the new value of the sample rate (in Hertz)
    @param[in] bandwidth specifies the channel bandwidth in Hertz
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_sample_rate_and_bandwidth(uint8_t card,
                                                         skiq_rx_hdl_t hdl,
                                                         uint32_t rate,
                                                         uint32_t bandwidth);

/*****************************************************************************/
/** The skiq_write_rx_sample_rate_and_bandwidth_multi() function allows users
    to configure the sample rate and bandwith for multiple receive handles.

    @note  This function is preferred when configuring multiple handles, as
    it offers better performance compared to multiple calls to 
    ::skiq_write_rx_sample_rate_and_bandwidth().

    @warning Rx/Tx sample rates are derived from the same clock so modifications
    to the Rx sample rate will also update the Tx sample rate to the same value.

    @since      4.15.0
    @ingroup rxfunctions
    @see skiq_write_rx_sample_rate_and_bandwidth

    @param[in] card card index of the Sidekiq of interest
    @param[in] handles [::skiq_rx_hdl_t] array of rx handles to be initialized
    @param[in] nr_handles number of rx handles defined in @p handles
    @param[in] rate array of sample rates corresponding to handles[]
    @param[in] bandwidth array of bandwidth values corresponding to handles[]
    @return int32_t  status where 0=success, anything else is an error
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -ENOSYS if the FPGA version does not support IQ ordering mode
    @retval -ENOTSUP if IQ order mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL if an invalid rate or bandwidth is specified

    @note The indices of @p handles[] and @p rate[] should line up such
    that index N descibes the libsidekiq rx_handle of interest, the sample rate
    for index N (in @p rate[] ), and the bandwidth for index N (in @p bandwidth[])
    For example:

       <pre>
       card = 1
       handles[0] = skiq_rx_hdl_A1
       handles[1] = skiq_rx_hdl_B1
       rate[0] =  61440000
       rate[1] = 122880000
       bandwidth[0] =  49152000
       bandwidth[1] = 100000000
       nr_handles = 2;
       </pre>

        means libsidekiq card 1 will be configured to receive on handle
        skiq_rx_hdl_A1 @ 61440000 Msps with a bandwidth of 49152000 Hz and
        skiq_rx_hdl_B1 @ 122880000 Msps with a bandwidth of 100000000 Hz.

*/
EPIQ_API int32_t skiq_write_rx_sample_rate_and_bandwidth_multi(uint8_t card,
                                                               skiq_rx_hdl_t handles[],
                                                               uint8_t nr_handles,
                                                               uint32_t rate[],
                                                               uint32_t bandwidth[]);

/*****************************************************************************/
/** The skiq_read_rx_sample_rate_and_bandwidth() function reads the current
    setting for the rate of received samples being transferred into the FPGA
    from the RFIC and the configured channel bandwidth.

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] the handle of the requested rx interface
    @param[out] p_rate  a pointer to the variable that should be updated with
    the current sample rate setting (in Hertz) currently set for the specified
    interface
    @param[out] p_actual_rate  a pointer to the variable that should be updated with
    the actual rate of received samples being transferred into the FPGA
    @param[out] p_bandwidth a pointer to the variable that is updated with the current
    channel bandwidth setting (in Hertz)
    @param[out] p_actual_bandwidth a pointer to the variable that is updated with the
    actual channel bandwidth configured (in Hertz)
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_sample_rate_and_bandwidth(uint8_t card,
                                                        skiq_rx_hdl_t hdl,
                                                        uint32_t *p_rate,
                                                        double *p_actual_rate,
                                                        uint32_t *p_bandwidth,
                                                        uint32_t *p_actual_bandwidth);


/*****************************************************************************/
/** The skiq_write_tx_sample_rate_and_bandwidth() function writes the current
    setting for the rate of transmit samples being transferred from the FPGA to
    the RFIC.  Additionally, the channel bandwidth is also configured.

    @note Rx/Tx sample rates are derived from the same clock so modifications to
    the Tx sample rate will also update the Rx sample rate to the same value.

    @ingroup txfunctions
    @see skiq_write_rx_sample_rate_and_bandwidth
    @see skiq_read_tx_sample_rate_and_bandwidth

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] rate  the new value of the sample rate (in Hertz)
    @param[in] bandwidth specifies the channel bandwidth in Hertz
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_sample_rate_and_bandwidth(uint8_t card,
                                                         skiq_tx_hdl_t hdl,
                                                         uint32_t rate,
                                                         uint32_t bandwidth);

/*****************************************************************************/
/** The skiq_read_tx_sample_rate_and_bandwidth() function reads the current
    setting for the rate of transmit samples being transferred from the FPGA to
    the RFIC and the configured channel bandwidth.

    @ingroup txfunctions
    @see skiq_write_tx_sample_rate_and_bandwidth

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_rate  a pointer to the variable that should be updated with
    the current sample rate setting (in Hertz) currently set for the specified
    interface
    @param[out] p_actual_rate  a pointer to the variable that should be updated with
    the actual rate of received samples being transferred into the FPGA
    @param[out] p_bandwidth a pointer to the variable that is updated with the current
    channel bandwidth setting (in Hertz)
    @param[out] p_actual_bandwidth a pointer to the variable that is updated with the
    actual channel bandwidth configured (in Hertz)
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_sample_rate_and_bandwidth(uint8_t card,
                                                        skiq_tx_hdl_t hdl,
                                                        uint32_t *p_rate,
                                                        double *p_actual_rate,
                                                        uint32_t *p_bandwidth,
                                                        uint32_t *p_actual_bandwidth);

/*****************************************************************************/
/** The skiq_set_rx_transfer_timeout() function is responsible for updating the
    current receive transfer timeout for the provided card.  The currently
    permitted range of timeout is ::RX_TRANSFER_WAIT_FOREVER,
    ::RX_TRANSFER_NO_WAIT, or a value between 20 and 1000000.

    @note Changing the receive transfer timeout may affect calls that are
    in progress.

    @note A skiq_receive() call that times out is only guaranteed to be at least
    the receive transfer timeout value, and makes no guarantee of an upper
    bound.  Once the timeout has been exceeded without a packet from the FPGA,
    the call returns at the next opportunity the kernel provides to the
    associated process.

    @warning When using a non-zero timeout, calling skiq_stop_rx_streaming() or
    skiq_exit() can cause skiq_receive() to return without a packet.  Be sure to
    handle that case.

    @note For improved CPU usage efficiency in receiving, a non-zero timeout is
    recommended.  Additionally, a timeout that is greater than the inter-block
    timing at the configured Rx sample rate is also recommended.

    @ingroup rxfunctions
    @see skiq_receive
    @see skiq_get_rx_transfer_timeout

    @param[in] card card index of the Sidekiq of interest
    @param[in] timeout_us minimum timeout in microseconds for a blocking skiq_receive().
    can be ::RX_TRANSFER_WAIT_FOREVER, ::RX_TRANSFER_NO_WAIT, or 20-1000000.
    @return int32_t  status where 0=success, anything else is an error.
*/
EPIQ_API int32_t skiq_set_rx_transfer_timeout( const uint8_t card,
                                               const int32_t timeout_us );

/*****************************************************************************/
/** The skiq_get_rx_transfer_timeout() function returns the currently configured
    receive transfer timeout.  If the return code indicates success, then
    p_timeout_us is guaranteed to be ::RX_TRANSFER_NO_WAIT,
    ::RX_TRANSFER_WAIT_FOREVER, ::RX_TRANSFER_WAIT_NOT_SUPPORTED or 20-1000000.

    @ingroup rxfunctions
    @see skiq_receive
    @see skiq_set_rx_transfer_timeout

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_timeout_us reference to an int32_t to populate
    @return int32_t  status where 0=success, anything else is an error.
*/
EPIQ_API int32_t skiq_get_rx_transfer_timeout( const uint8_t card,
                                             int32_t *p_timeout_us );

/*************************************************************************************************/
/** The skiq_receive() function is responsible for receiving a contiguous block of data from the
    FPGA.  The type of data being returned is specified in the metadata, but is typically
    timestamped I/Q samples.  One contiguous block of data will be returned each time this function
    is called.

    @warning The Rx interface from which the data was received is specified in the p_hdl parameter.
    This is needed because the underlying driver may have multiple Rx interfaces streaming
    simultaneously, and these data streams will be interleaved by the hardware.

    @attention The format of the data returned by the receive call is specified by the
    ::skiq_rx_block_t structure.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup rxfunctions
    @see skiq_start_rx_streaming
    @see skiq_start_rx_streaming_on_1pps
    @see skiq_stop_rx_streaming
    @see skiq_stop_rx_streaming_on_1pps
    @see skiq_rx_block_t

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_hdl [::skiq_rx_hdl_t]  a pointer to the Rx handle that will be updated by
    libsidekiq to specify the handle associated with the received data
    @param[out] pp_block [::skiq_rx_block_t]  a reference to a receive block reference
    @param[out] p_data_len  a pointer to be filled in with the # of bytes
    are returned as part of the transfer
    @return ::skiq_rx_status_t  status of the receive call
*/
EPIQ_API skiq_rx_status_t skiq_receive( uint8_t card,
                                        skiq_rx_hdl_t* p_hdl,
                                        skiq_rx_block_t** pp_block,
                                        uint32_t* p_data_len );

/*****************************************************************************/
/** The skiq_transmit() function is responsible for writing a block of I/Q
    samples to transmit.  When running in @ref skiq_tx_transfer_mode_sync
    "synchronous mode", this function will block until the FPGA has queued the
    samples to send.  If running in @ref skiq_tx_transfer_mode_async
    "asynchronous mode", the function will return immediately.  If the packet
    has successfully been buffered for transfer, a 0 will be returned.  If there
    is not enough room left in the buffer, @ref SKIQ_TX_ASYNC_SEND_QUEUE_FULL is
    returned.

    The first @ref SKIQ_TX_HEADER_SIZE_IN_WORDS contain metadata associated with
    transmit packet.  Included in the metadata is the desired timestamp to send
    the samples.  If running in @ref skiq_tx_immediate_data_flow_mode the
    timestamp is ignored and the data is sent immediately. Following the
    metadata is the block_size (in words) of sample data.  The number of words
    contained in p_samples should match the previously configured Tx block size
    plus the header size.

    The format of the data provided to the transmit call
    <pre>
                   -31-------------------------------------------------------0-
          word 0   |                                                          |
                   |                      META0 (misc)                        |
          word 1   |                                                          |
                   -31-------------------------------------------------------0-
          word 2   |                                                          |
                   |                    RF TIMESTAMP                          |
          word 3   |                                                          |
                   -31-------------------------------------------------------0-
                   |         12-bit I0_A1        |       12-bit Q0_A1         |
 n |-     word 4   | (sign extended to 16 bits   | (sign extended to 16 bits) |
 u |               ------------------------------------------------------------
 m |               |         12-bit I1_A1        |       12-bit Q1_A1         |
 _ |      word 5   | (sign extended to 16 bits   | (sign extended to 16 bits) |
 b |               ------------------------------------------------------------
 l |               |           ...               |          ...               |
 o |               ------------------------------------------------------------
 c |      word     |   12-bit Iblock_size_A1     |   12-bit Qblock_size_A1    |
 k |       3 +     | (sign extended to 16 bits   | (sign extended to 16 bits) |
 s |-  block_size  ------------------------------------------------------------

    </pre>

    @since Function signature modified v4.0.0 to take @ref skiq_tx_block_t
    instead of int32_t pointer for transmit data and a new void pointer argument
    for user data to be passed back into the callback function if the transmit
    transfer mode is @ref skiq_tx_transfer_mode_async.

    @note If the caller does not need user data or the transmit transfer mode is
    @ref skiq_tx_transfer_mode_sync, the caller should pass NULL as p_user.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup txfunctions
    @see skiq_start_tx_streaming
    @see skiq_start_tx_streaming_on_1pps
    @see skiq_stop_tx_streaming
    @see skiq_stop_tx_streaming_on_1pps
    @see skiq_read_tx_transfer_mode
    @see skiq_write_tx_transfer_mode
    @see skiq_read_tx_data_flow_mode
    @see skiq_write_tx_data_flow_mode
    @see skiq_tx_block_t

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the desired interface
    @param[in] p_block [::skiq_tx_block_t]  a pointer to the timestamp + I/Q sample data
    @param[in] p_user a pointer to user data that is passed back into the callback function if async
    @return int32_t status where 0=success, @ref SKIQ_TX_ASYNC_SEND_QUEUE_FULL
    indicates out of room to buffer if in @ref skiq_tx_transfer_mode_async
    "asynchronous mode", anything else is an error
*/
EPIQ_API int32_t skiq_transmit(uint8_t card,
                               skiq_tx_hdl_t hdl,
                               skiq_tx_block_t *p_block,
                               void *p_user);

/*****************************************************************************/
/** The skiq_read_rx_gain_index_range() function is responsible for obtaining
    the viable range of gain index values that can be used to call into the
    skiq_write_rx_gain() function. Note that the range provided is inclusive.

    @since Function added in API @b v4.2.0

    @ingroup rxfunctions
    @see skiq_write_rx_gain

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the desired interface
    @param[out] p_gain_index_min pointer to be updated with minimum index value
    @param[out] p_gain_index_max pointer to be updated with maximum index value
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_gain_index_range(uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               uint8_t* p_gain_index_min,
                                               uint8_t* p_gain_index_max);

/**************************************************************************************************/
/** The skiq_write_rx_gain() function is responsible for setting the overall gain of the Rx lineup
    for the specified receiver by means of providing an index that maps to a specified gain.  The
    gain index value is a direct index into the gain table of the radio.  The mapping of gain index
    to gain in dB is dependent on the RFIC used by the product.

    - For Sidekiq mPCIe (::skiq_mpcie), Sidekiq M.2 (::skiq_m2), Sidekiq Stretch (::skiq_m2_2280),
    Sidekiq Z2 (::skiq_z2), and Matchstiq Z3u (::skiq_z3u) each increment of the gain index value 
    results in approximately 1 dB of gain, with approximately 76 dB of total gain available.  
    For details on the gain table, refer to p. 37 of <a
    href="https://www.analog.com/media/en/technical-documentation/user-guides/AD9361_Reference_Manual_UG-570.pdf">AD9361
    Reference Manual UG-570</a>

    - For Sidekiq X2 (::skiq_x2), the A1 (Rx1) & A2 (Rx2) receivers have approximately 30 dB of
    total gain available, where an increment of 1 in the gain index value results in approximately
    0.5 dB increase.  The B1 (ObsRx) receiver has approximately 18 dB of total gain available, where
    an increment of 1 in the gain index value results in approximately 1 dB increase in gain.  For
    details on the gain table available, refer to the "Gain Table" section on p. 120 of the AD9371
    User Guide (UG-992)

    - For Sidekiq X4 (::skiq_x4), each receiver has 30 dB of total gain available, where an
    increment of 1 in the gain index results in approximately 0.5 dB increase.  For details on the
    receiver datapath and gain control blocks, refer to the "Receiver Datapath" on p. 125 of the
    ADRV9008-1/ADRV9008-2/ADRV9009 Hardware Reference Manual (UG-1295)

    - For Sidekiq NV100 (::skiq_nv100), each receiver has 34 dB of total gain available, where an
    increment of 1 in the gain index results in approximately 0.5 dB increase.  For details on the 
    gain table available, refer to the "Receiver Specifications" section on p. 6 of the <a
    href="https://www.analog.com/media/en/technical-documentation/data-sheets/adrv9002.pdf">ADRV9002:
    Dual Narrow/Wideband RF Data Sheet</a>

    @ingroup rxfunctions
    @see skiq_read_rx_gain_index_range
    @see skiq_read_rx_gain
    @see skiq_read_rx_gain_mode
    @see skiq_write_rx_gain_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the desired interface
    @param[in] gain_index the requested rx gain index
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_gain(uint8_t card,
                                    skiq_rx_hdl_t hdl,
                                    uint8_t gain_index);

/**************************************************************************************************/
/** The skiq_read_rx_gain() function is responsible for retrieving the current gain index for the
    specified Rx interface.  The gain index value is a direct index into the gain table of the
    radio.  The mapping of gain index to gain in dB is dependent on the RFIC used by the product.

    - For Sidekiq mPCIe (::skiq_mpcie), Sidekiq M.2 (::skiq_m2), Sidekiq Stretch (::skiq_m2_2280),
    Sidekiq Z2 (::skiq_z2), and Matchstiq Z3u (::skiq_z3u) each increment of the gain index value 
    results in approximately 1 dB of gain, with approximately 76 dB of total gain available.  For 
    details on the gain table, refer to p. 37 of <a
    href="https://www.analog.com/media/en/technical-documentation/user-guides/AD9361_Reference_Manual_UG-570.pdf">AD9361
    Reference Manual UG-570</a>

    - For Sidekiq X2 (::skiq_x2), the A1 (Rx1) & A2 (Rx2) receivers have approximately 30 dB of
    total gain available, where an increment of 1 in the gain index value results in approximately
    0.5 dB increase.  The B1 (ObsRx) receiver has approximately 18 dB of total gain available, where
    an increment of 1 in the gain index value results in approximately 1 dB increase in gain.  For
    details on the gain table available, refer to the "Gain Table" section on p. 120 of the AD9371
    User Guide (UG-992)

    - For Sidekiq X4 (::skiq_x4), each receiver has 30 dB of total gain available, where an
    increment of 1 in the gain index results in approximately 0.5 dB increase.  For details on the
    receiver datapath and gain control blocks, refer to the "Receiver Datapath" on p.125 of the
    ADRV9008-1/ADRV9008-2/ADRV9009 Hardware Reference Manual (UG-1295)

    @ingroup rxfunctions
    @see skiq_read_rx_gain_index_range
    @see skiq_read_rx_gain_mode
    @see skiq_write_rx_gain
    @see skiq_write_rx_gain_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the desired interface
    @param[out] p_gain_index a pointer to be updated with current gain index
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_gain(uint8_t card,
                                   skiq_rx_hdl_t hdl,
                                   uint8_t* p_gain_index);

/*****************************************************************************/
/** The skiq_read_rx_gain_mode() function is responsible for reading the @ref
    skiq_rx_gain_t "current gain mode" being used by the Rx interface.

    @ingroup rxfunctions
    @see skiq_read_rx_gain_index_range
    @see skiq_read_rx_gain
    @see skiq_write_rx_gain
    @see skiq_write_rx_gain_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_gain_mode [::skiq_rx_gain_t] a pointer to where the currently
    set Rx gain mode will be written.  Valid values are ::skiq_rx_gain_manual
    and ::skiq_rx_gain_auto.

    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_gain_mode(uint8_t card,
                                        skiq_rx_hdl_t hdl,
                                        skiq_rx_gain_t* p_gain_mode);

/*****************************************************************************/
/** The skiq_write_rx_gain_mode() function is responsible for writing the @ref
    skiq_rx_gain_t "current gain mode" being used by the Rx interface.

    @ingroup rxfunctions
    @see skiq_read_rx_gain_index_range
    @see skiq_read_rx_gain
    @see skiq_read_rx_gain_mode
    @see skiq_write_rx_gain

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested Rx interface
    @param[in] gain_mode [::skiq_rx_gain_t] the requested Rx gain mode to be
    written.  Valid values are ::skiq_rx_gain_manual and ::skiq_rx_gain_auto.

    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_gain_mode(uint8_t card,
                                         skiq_rx_hdl_t hdl,
                                         skiq_rx_gain_t gain_mode);

/*****************************************************************************/
/** The skiq_write_rx_attenuation_mode() function is responsible for writing the
    @ref skiq_rx_attenuation_mode_t "current attenuation mode" being used by the
    Rx interface.

    @attention This is only supported for <a
    href="https://epiqsolutions.com/sidekiq-x2/">Sidekiq X2</a>.

    @since Function added in API @b v4.4.0

    @ingroup rxfunctions
    @see skiq_read_rx_attenuation
    @see skiq_read_rx_attenuation_mode
    @see skiq_write_rx_attenuation

    @param card card index of the Sidekiq of interest
    @param hdl the handle of the requested Rx interface
    @param mode [::skiq_rx_attenuation_mode_t] the requested Rx attenuation mode
    to be written
    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_attenuation_mode(uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               skiq_rx_attenuation_mode_t mode);

/*****************************************************************************/
/** The skiq_read_rx_attenuation_mode() function is responsible for reading the
    @ref skiq_rx_attenuation_mode_t "current attenuation mode" being used by the
    Rx interface.

    @attention This is only supported for <a
    href="https://epiqsolutions.com/sidekiq-x2/">Sidekiq X2</a>.

    @since Function added in API @b v4.4.0

    @ingroup rxfunctions
    @see skiq_read_rx_attenuation
    @see skiq_write_rx_attenuation
    @see skiq_write_rx_attenuation_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested Rx interface
    @param[out] p_mode [::skiq_rx_attenuation_mode_t] pointer to be updated with
    the current Rx attenuation mode
    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_attenuation_mode(uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               skiq_rx_attenuation_mode_t *p_mode);

/*****************************************************************************/
/** The skiq_write_rx_attenuation() function is responsible for writing the
    Rx attenuation in 0.25 dB steps. Note that the Rx attenuation is applied to
    an external analog attenuator before the Rx signal reaches the RFIC.

    @attention This is only supported for <a
    href="https://epiqsolutions.com/sidekiq-x2/">Sidekiq X2</a>. Refer to the <a
    href="https://epiqsolutions.com/support/viewforum.php?f=324">Sidekiq X2
    Hardware User's Manual</a> for further details. This function will write the
    attenuators called out in "Figure 2: Sidekiq X2 block diagram". Attenuator
    "att2" maps to ::skiq_rx_hdl_A1, "att1" maps to ::skiq_rx_hdl_A2, and "att3"
    maps to ::skiq_rx_hdl_B1.

    @since Function added in API @b v4.4.0

    @ingroup rxfunctions
    @see skiq_read_rx_attenuation
    @see skiq_read_rx_attenuation_mode
    @see skiq_write_rx_attenuation_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested Rx interface
    @param[in] attenuation the attenuation to be applied in quarter dB steps
    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_attenuation(uint8_t card,
                                           skiq_rx_hdl_t hdl,
                                           uint16_t attenuation);

/*****************************************************************************/
/** The skiq_read_rx_attenuation() function is responsible for reading the
    current Rx attenuation, returned in 0.25 dB steps. Note that the Rx
    attenuation is read from an external analog attenuator before the Rx signal
    reaches the RFIC.

    @attention This is only supported for <a
    href="https://epiqsolutions.com/sidekiq-x2/">Sidekiq X2</a>. Refer to the <a
    href="https://epiqsolutions.com/support/viewforum.php?f=324">Sidekiq X2
    Hardware User's Manual</a> for further details. This function will write the
    attenuators called out in "Figure 2: Sidekiq X2 block diagram". Attenuator
    "att2" maps to ::skiq_rx_hdl_A1, "att1" maps to ::skiq_rx_hdl_A2, and "att3"
    maps to ::skiq_rx_hdl_B1.

    @since Function added in API @b v4.4.0

    @ingroup rxfunctions
    @see skiq_read_rx_attenuation_mode
    @see skiq_write_rx_attenuation
    @see skiq_write_rx_attenuation_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_attenuation pointer to take current attenuation in quarter dB steps
    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_attenuation(uint8_t card,
                                          skiq_rx_hdl_t hdl,
                                          uint16_t *p_attenuation);

/*****************************************************************************/
/** The skiq_read_tx_LO_freq() function reads the current setting for the LO
    frequency of the requested tx interface.

    @ingroup txfunctions
    @see skiq_write_tx_LO_freq

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_freq  a pointer to the variable that should be updated with
    the current frequency (in Hertz)
    @param[out] p_tuned_freq: a pointer to the variable that should be updated with
    the actual tuned frequency (in Hertz)

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM Invalid TX handle specified
    @retval -ENODATA TX LO frequency has not yet been configured
*/
EPIQ_API int32_t skiq_read_tx_LO_freq(uint8_t card,
                                      skiq_tx_hdl_t hdl,
                                      uint64_t* p_freq,
                                      double* p_tuned_freq );

/*****************************************************************************/
/** The skiq_write_tx_LO_freq() function writes the current setting for the LO
    frequency of the requested tx interface.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup txfunctions
    @see skiq_read_tx_LO_freq

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[in] freq  the new value for the LO freq (in Hertz)
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_LO_freq(uint8_t card,
                                       skiq_tx_hdl_t hdl,
                                       uint64_t freq);

/*****************************************************************************/
/** The skiq_enable_tx_tone() function configures the RFIC to send out a single
    cycle of a CW tone.

    @note The RFIC is responsible generating the tone.  There is no reliance on
    the FPGA or software for this functionality.  However, a user must call
    skiq_start_tx_streaming() to enable the transmitter.

    @ingroup txfunctions
    @see skiq_disable_tx_tone
    @see skiq_read_tx_tone_freq
    @see skiq_read_tx_tone_freq_offset
    @see skiq_write_tx_tone_freq_offset

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_enable_tx_tone(uint8_t card,
                                     skiq_tx_hdl_t hdl);

/*****************************************************************************/
/** The skiq_disable_tx_tone() function disables the CW tone from being sent out
    when the transmitter is enabled.

    @note A user must also call skiq_stop_tx_streaming() to disable the
    transmitter.

    @ingroup txfunctions
    @see skiq_enable_tx_tone
    @see skiq_read_tx_tone_freq
    @see skiq_read_tx_tone_freq_offset
    @see skiq_write_tx_tone_freq_offset

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_disable_tx_tone(uint8_t card,
                                      skiq_tx_hdl_t hdl);

/*****************************************************************************/
/** The skiq_read_tx_tone_freq() function returns the LO frequency of the TX
    test tone.

    @since Function added in API @b v4.2.0

    @ingroup txfunctions
    @see skiq_enable_tx_tone
    @see skiq_disable_tx_tone
    @see skiq_read_tx_tone_freq_offset
    @see skiq_write_tx_tone_freq_offset

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_freq pointer to where to store the frequency (in Hz) of the test tone
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_tone_freq(uint8_t card,
                                        skiq_tx_hdl_t hdl,
                                        uint64_t *p_freq);

/*****************************************************************************/
/** The skiq_read_tx_tone_freq_offset() function returns the the TX test tone
    offset relative to the configured TX LO frequency.

    @since Function added in API @b v4.9.0

    @ingroup txfunctions
    @see skiq_enable_tx_tone
    @see skiq_disable_tx_tone
    @see skiq_read_tx_tone_freq
    @see skiq_write_tx_tone_freq_offset

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_freq_offset pointer to where to store the frequency (in Hz) offset

    @return int32_t  status where 0=success, anything else is an error

    @retval -ERANGE specified card index is out of range
    @retval -ENODEV specified card has not been initialized
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
*/
EPIQ_API int32_t skiq_read_tx_tone_freq_offset(uint8_t card,
                                               skiq_tx_hdl_t hdl,
                                               int32_t *p_freq_offset);

/*****************************************************************************/
/** The skiq_write_tx_tone_freq_offset() function configures the frequency of
    the TX test tone offset from the configured TX LO frequency.

    @since Function added in API @b v4.9.0
    @note This is not available for all products
    @note The frequency offset generally needs to fall within the +/- 0.5*sample_rate

    @ingroup txfunctions
    @see skiq_enable_tx_tone
    @see skiq_disable_tx_tone
    @see skiq_read_tx_tone_freq
    @see skiq_read_tx_tone_freq_offset

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[in] test_freq_offset test tone frequency (in Hz) offset

    @return int32_t  status where 0=success, anything else is an error

    @retval -ERANGE specified card index is out of range
    @retval -ENODEV specified card has not been initialized
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
*/
EPIQ_API int32_t skiq_write_tx_tone_freq_offset(uint8_t card,
                                                skiq_tx_hdl_t hdl,
                                                int32_t test_freq_offset);

/*****************************************************************************/
/** The skiq_write_tx_attenuation() function configures the attenuation of the
    transmitter for the Tx handle specified.  The value of the attenuation is
    0.25 dB steps such that an attenuation value of 4 would equate to 1 dB of
    actual attenuation.  A value of 0 would provide result in 0 attenuation, or
    maximum transmit power.  Valid attenuation settings are queried using
    skiq_read_parameters().

    @note If the specified attenuation is outside the radio's valid range, the attentuation
    level is set to the nearest allowed value, the maximum or minimum value.
    
    @ingroup txfunctions
    @see skiq_read_tx_attenuation
    @see skiq_read_parameters

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[in] attenuation value of attenuation
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_attenuation(uint8_t card,
                                           skiq_tx_hdl_t hdl,
                                           uint16_t attenuation);

/*****************************************************************************/
/** The skiq_read_tx_attenuation() function reads the attenuation setting of the
    transmitter for the Tx handle specified.  The value of the attenuation is
    0.25 dB steps such that an attenuation value of 4 would equate to 1 dB of
    actual attenuation.

    @ingroup txfunctions
    @see skiq_read_parameters
    @see skiq_write_tx_attenuation

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_attenuation pointer to where to store the attenuation read
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_attenuation(uint8_t card,
                                          skiq_tx_hdl_t hdl,
                                          uint16_t *p_attenuation);


/*****************************************************************************/
/** The skiq_read_tx_sample_rate() function reads the current setting for the
    rate at which samples will be delivered from the FPGA to the RF front end
    for transmission.

    @ingroup txfunctions
    @see skiq_read_tx_sample_rate_and_bandwidth
    @see skiq_write_tx_sample_rate_and_bandwidth

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_rate  a pointer to the variable that should be updated with
    the actual sample rate (in Hertz) currently set for the D/A converter
    @param[out] p_actual_rate  a pointer to the variable that should be updated with
    the actual sample rate (in Hertz) currently set
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_sample_rate(uint8_t card,
                                          skiq_tx_hdl_t hdl,
                                          uint32_t *p_rate,
                                          double* p_actual_rate);

/*****************************************************************************/
/** The skiq_read_tx_block_size() function reads the current setting for the
    block size of transmit packets.

    @note The block size is represented in words and does not include the header
    size, it accounts only for the number of samples.  The total Tx packet size
    includes both the header size and block size.

    @ingroup txfunctions
    @see skiq_write_tx_block_size

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_block_size_in_words  a pointer to the variable that should be
    updated with current Tx block size
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_block_size(uint8_t card,
                                         skiq_tx_hdl_t hdl,
                                         uint16_t *p_block_size_in_words);


/*****************************************************************************/
/** The skiq_write_tx_block_size() function configures the block size of
    transmit packets.

    @note The block size is represented in words and is the size (in words) of
    the IQ samples for each channel, not including the metadata.  When using
    packed mode, this is the number of words (not number of samples) in the
    payload, not including the metadata.  Also, while in packed mode, the value
    specified must result in an even number of samples included in a block.  For
    instance, a block size of 252 * 4/3 = 336 samples per block of data, which
    is a valid configuration.  A block size of 508 * 4/3 - 677.3 samples per
    block would be invalid.

    @attention The validity of the configuration will not be confirmed until
    start streaming is called.

    @note This must be set prior to the Tx interface being started.  If set
    after the Tx interface has been started, the setting will be stored but will
    not be used until the interface is stopped and re-started.

    @ingroup txfunctions
    @see skiq_read_tx_block_size

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[in] block_size_in_words number of words to configure the Tx block size
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_block_size(uint8_t card,
                                          skiq_tx_hdl_t hdl,
                                          uint16_t block_size_in_words);

/*****************************************************************************/
/** The skiq_read_tx_num_underruns() function reads the current number of Tx
    underruns observed by the FPGA.  This value is reset only when calling
    skiq_start_tx_streaming().

    @warning This number is only valid if running with Tx data flow mode set to
    ::skiq_tx_immediate_data_flow_mode.

    @ingroup txfunctions
    @see skiq_read_tx_data_flow_mode
    @see skiq_write_tx_data_flow_mode
    @see skiq_read_tx_num_late_timestamps

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_num_underrun  a pointer to the variable that is updated with
    the number of underruns observed since starting streaming
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_num_underruns(uint8_t card,
                                            skiq_tx_hdl_t hdl,
                                            uint32_t *p_num_underrun);

/*****************************************************************************/
/** The skiq_read_tx_num_late_timestamps() function reads the current number of
    "late" Tx timestamps observed by the FPGA.  When the FPGA encounters a Tx
    timestamp that has occurred in the past, the FPGA Tx FIFO is flushed of all
    packets and a counter is incremented.  This function returns the count of
    how many times the FIFO was flushed due to a timestamp in the past.  The
    value is reset only after calling skiq_stop_tx_streaming().

    @warning    The late timestamp count value is only valid if running with Tx
    data flow mode set to ::skiq_tx_with_timestamps_data_flow_mode and not
    ::skiq_tx_immediate_data_flow_mode or
    ::skiq_tx_with_timestamps_allow_late_data_flow_mode.

    @attention  The late timestamp counter is not updated when in
    ::skiq_tx_with_timestamps_allow_late_data_flow_mode, even if the data is
    transmitted later than its timestamp.

    @ingroup txfunctions
    @see skiq_read_tx_data_flow_mode
    @see skiq_write_tx_data_flow_mode
    @see skiq_read_tx_num_underruns

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_num_late  a pointer to the variable that is updated with
    the number of times the FIFO is flushed due to a "late" timestamp
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_num_late_timestamps(uint8_t card,
                                                  skiq_tx_hdl_t hdl,
                                                  uint32_t *p_num_late);


/*****************************************************************************/
/** The skiq_read_temp() function is responsible for reading and providing
    the current temperature of the unit (in degrees Celsius).

    @ingroup cardfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_temp_in_deg_C a pointer to where the current temp should
    be written

    @return 0 on success, else a negative errno value
    @retval -EAGAIN Temperature sensor measurement is temporarily not available, try again later
    @retval -ENODEV Temperature sensor not available in present ::skiq_xport_init_level_t, try ::skiq_xport_init_level_full
    @retval -EINVAL No supported sensors found
    @retval -EIO I/O communication error occurred during measurement
    @retval -ENOTSUP No sensors for associated Sidekiq product
*/
EPIQ_API int32_t skiq_read_temp(uint8_t card,
                                int8_t* p_temp_in_deg_C);

/*****************************************************************************/
/** The skiq_is_accel_supported() function is responsible for determining if
    the accelerometer is supported on the hardware platform of the card specified.

    @since Function added in API @b v4.2.0

    @ingroup accelfunctions
    @see skiq_read_accel
    @see skiq_read_accel_state
    @see skiq_read_accel_reg
    @see skiq_write_accel_state
    @see skiq_write_accel_reg

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_supported pointer to where to accelerometer support
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_is_accel_supported( uint8_t card, bool *p_supported );

/*************************************************************************************************/
/** The skiq_read_accel() function is responsible for reading and providing the accelerometer data.
    The data format is twos compliment and 16 bits.  If measurements are not available, -EAGAIN is
    returned and the accelerometer should be queried again for position.

    @since As of libsidekiq @b v4.7.2, for all supported products, this function will populate
    @p p_x_data, @p p_y_data, and @p p_z_data with measurements in units of thousandths of standard
    gravity (\f$g_0\f$).

    @ingroup accelfunctions
    @see skiq_is_accel_supported
    @see skiq_read_accel_state
    @see skiq_read_accel_reg
    @see skiq_write_accel_state
    @see skiq_write_accel_reg

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_x_data a pointer to where the X-axis accelerometer measurement is written
    @param[out] p_y_data a pointer to where the Y-axis accelerometer measurement is written
    @param[out] p_z_data a pointer to where the Z-axis accelerometer measurement is written

    @return int32_t  status where 0=success, anything else is an error

    @retval -ERANGE specified card index is out of range
    @retval -ENODEV specified card has not been initialized
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -EAGAIN accelerometer measurement is not available
    @retval -EIO error communicating with the accelerometer
*/
EPIQ_API int32_t skiq_read_accel( uint8_t card,
                                  int16_t *p_x_data,
                                  int16_t *p_y_data,
                                  int16_t *p_z_data );

/*****************************************************************************/
/** The skiq_write_accel_state() function is responsible for enabling or
    disabling the on-board accelerometer (if available) to take measurements.

    @ingroup accelfunctions
    @see skiq_is_accel_supported
    @see skiq_read_accel
    @see skiq_read_accel_state
    @see skiq_read_accel_reg
    @see skiq_write_accel_reg

    @param[in] card card index of the Sidekiq of interest
    @param[in] enabled accelerometer state (1=enabled, 0=disabled)

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_accel_state(uint8_t card, uint8_t enabled);

/*****************************************************************************/
/** The skiq_write_accel_reg() function provides generic write access to the
    on-board ADXL346 accelerometer.

    @since Function added in API @b v4.2.0

    @ingroup accelfunctions
    @see skiq_is_accel_supported
    @see skiq_read_accel
    @see skiq_read_accel_state
    @see skiq_read_accel_reg
    @see skiq_write_accel_state

    @param[in] card card index of the Sidekiq of interest
    @param[in] reg register address to access
    @param[in] p_data pointer to buffer of data to write
    @param[in] len number of bytes to write

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_accel_reg(uint8_t card,
                                      uint8_t reg,
                                      uint8_t* p_data,
                                      uint32_t len);

/*****************************************************************************/
/** The skiq_read_accel_reg() function provides generic read access to the
    onboard ADXL346 accelerometer.

    @since Function added in API @b v4.2.0

    @ingroup accelfunctions
    @see skiq_is_accel_supported
    @see skiq_read_accel
    @see skiq_read_accel_state
    @see skiq_write_accel_state
    @see skiq_write_accel_reg

    @param[in] card card index of the Sidekiq of interest
    @param[in] reg register address to access
    @param[in] p_data pointer to buffer to read data into
    @param[in] len number of bytes to read

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_accel_reg(uint8_t card,
                                     uint8_t reg,
                                     uint8_t* p_data,
                                     uint32_t len);

/*****************************************************************************/
/** The skiq_read_accel_state() function is responsible for reading the current
    state of the accelerometer.

    @ingroup accelfunctions
    @see skiq_is_accel_supported
    @see skiq_read_accel
    @see skiq_read_accel_reg
    @see skiq_write_accel_state
    @see skiq_write_accel_reg

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_enabled pointer to where to store the accelerometer state (1=enabled, 0=disabled)

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_accel_state(uint8_t card,
                                       uint8_t *p_enabled);

/*****************************************************************************/
/** The skiq_write_tcvcxo_warp_voltage() function is responsible for setting a
    new warp value for the reference clock oscillator.  A DAC is controlled by
    this function and the DAC can generate voltage between 0.75 and 2.25V.
    Valid DAC values can vary from product to product, see product manual for
    details. Valid warp voltages for the ref clock oscillator are from 0.75 -
    2.25V (which corresponds to evenly distributed values across all possible
    values in the DAC range).

    @ingroup tcvcxofunctions
    @see skiq_read_tcvcxo_warp_voltage
    @see skiq_read_default_tcvcxo_warp_voltage
    @see skiq_read_user_tcvcxo_warp_voltage
    @see skiq_write_user_tcvcxo_warp_voltage

    @param[in] card card index of the Sidekiq of interest
    @param[in] warp_voltage a value corresponding to the desired DAC
    voltage to be applied.  Valid values can vary from product to product,
    see product manual for details.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tcvcxo_warp_voltage(uint8_t card,
                                                uint16_t warp_voltage);

/*****************************************************************************/
/** The skiq_read_txcvxo_warp_voltage() function is responsible for returning the
    current value of the warp voltage.

    @ingroup tcvcxofunctions
    @see skiq_write_tcvcxo_warp_voltage
    @see skiq_read_default_tcvcxo_warp_voltage
    @see skiq_read_user_tcvcxo_warp_voltage
    @see skiq_write_user_tcvcxo_warp_voltage

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_warp_voltage a pointer to where the currently set warp voltage
    will be written.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tcvcxo_warp_voltage(uint8_t card,
                                               uint16_t* p_warp_voltage);

/*****************************************************************************/
/** The skiq_read_default_txcvxo_warp_voltage() function is responsible for
    returning the default value of the warp voltage.  This default value is
    determined during factory calibration and is read-only.  If no factory
    calibrated value is available, an error is returned.  The default TCVCXO
    warp voltage value is automatically loaded during skiq_init(),
    skiq_init_without_cards(), or skiq_init_by_serial_str() unless a user value
    is defined in which case the user value is loaded during initialization.

    @ingroup tcvcxofunctions
    @see skiq_write_tcvcxo_warp_voltage
    @see skiq_read_tcvcxo_warp_voltage
    @see skiq_read_user_tcvcxo_warp_voltage
    @see skiq_write_user_tcvcxo_warp_voltage

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_warp_voltage a pointer to where the currently set warp voltage
    will be written.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_default_tcvcxo_warp_voltage( uint8_t card,
                                                      uint16_t *p_warp_voltage );

/*****************************************************************************/
/** The skiq_read_user_txcvxo_warp_voltage() function is responsible for
    returning the user defined warp voltage value.  This value can be specified
    by the user and is automatically loaded during a call to skiq_init(),
    skiq_init_without_cards(), or skiq_init_by_serial_str().  This value takes
    precedence over the default value loaded by the factory.

    @ingroup tcvcxofunctions
    @see skiq_write_tcvcxo_warp_voltage
    @see skiq_read_tcvcxo_warp_voltage
    @see skiq_read_default_tcvcxo_warp_voltage
    @see skiq_write_user_tcvcxo_warp_voltage

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_warp_voltage a pointer to where the currently set warp voltage
    will be written.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_user_tcvcxo_warp_voltage( uint8_t card,
                                                   uint16_t *p_warp_voltage );

/*****************************************************************************/
/** The skiq_write_user_txcvxo_warp_voltage() function configures the
    user-defined warp voltage value.  This value can be specified by the user
    and is automatically loaded during a call to skiq_init(),
    skiq_init_without_cards(), or skiq_init_by_serial_str().  This value takes
    precedence over the default value loaded by the factory.

    @ingroup tcvcxofunctions
    @see skiq_write_tcvcxo_warp_voltage
    @see skiq_read_tcvcxo_warp_voltage
    @see skiq_read_default_tcvcxo_warp_voltage
    @see skiq_read_user_tcvcxo_warp_voltage

    @param[in] card card index of the Sidekiq of interest
    @param[in] warp_voltage specifies a warp voltage to set
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_user_tcvcxo_warp_voltage( uint8_t card,
                                                    uint16_t warp_voltage );

/*****************************************************************************/
/** The skiq_write_iq_pack_mode() function is responsible for setting whether or
    not the IQ samples being received/transmitted and to/from the FPGA to/from
    the CPU should be packed/compressed before being sent.  This allows four
    12-bit complex I/Q samples to be transferred in three 32-bit words,
    increasing the throughput efficiency of the channel.  An interface defaults
    to using un-packed mode if the skiq_write_iq_pack_mode() is not called.

    @note That this can be changed at any time, but updates are only honored
    whenever streaming is started.

    If the pack "mode" is set to false, the behavior is to have the I/Q sent up
    as two's complement, sign-extended, little-endian, unpacked in the following
    format:

    <pre>
           -31-------------------------------------------------------0-
           |         12-bit I0           |       12-bit Q0            |
    word 0 | (sign extended to 16 bits   | (sign extended to 16 bits) |
           ------------------------------------------------------------
           |         12-bit I1           |       12-bit Q1            |
    word 1 | (sign extended to 16 bits   | (sign extended to 16 bits) |
           ------------------------------------------------------------
           |         12-bit I2           |       12-bit Q2            |
    word 2 |  (sign extended to 16 bits  | (sign extended to 16 bits) |
           ------------------------------------------------------------
           |           ...               |          ...               |
           ------------------------------------------------------------

   </pre>
    When the mode is set to true, then the 12-bit samples are packed in to
    make optimal use of the available bits, and packed as follows:
   <pre>
           -31-------------------------------------------------------0-
    word 0 |I0b11|...|I0b0|Q0b11|.................|Q0b0|I1b11|...|I1b4|
           ------------------------------------------------------------
    word 1 |I1b3|...|I1b0|Q1b11|...|Q1b0|I2b11|...|I2b0|Q2b11|...|Q2b8|
           -31-------------------------------------------------------0-
    word 2 |Q2b7|...|Q2b0|I3b11|.................|I3b0|Q1311|....|Q3b4|
           ------------------------------------------------------------
           |           ...               |          ...               |
           ------------------------------------------------------------
    </pre>
    (with the above sequence repeated every three words)

    Once the packed I/Q samples are received up in the CPU there are extra
    cycles needed to de-compress/un-pack them.  However, for cases where an
    application simply needs to transfer a large block of contiguous I/Q samples
    up to the CPU for non-real time post processing, this will increase the
    bandwidth without sacrificing dynamic range.

    @warning I/Q pack mode conflicts with ::skiq_rx_stream_mode_low_latency.  As
    such, caller may not configure a card to use both packed I/Q mode and RX low
    latency mode at the same time.  This function will return an error (@c
    -EPERM) if caller sets mode to true and ::skiq_rx_stream_mode_low_latency is
    currently selected.

    @ingroup fpgafunctions
    @see skiq_read_iq_pack_mode
    @see skiq_read_rx_stream_mode
    @see skiq_write_rx_stream_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] mode  false=use normal (non-packed) I/Q mode (default)
                     true=use packed I/Q mode
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_iq_pack_mode(uint8_t card,
                                         bool mode);

/*****************************************************************************/
/** The skiq_read_iq_pack_mode() function is responsible for retrieving the
    current pack mode setting for the Sidekiq card.

    @ingroup fpgafunctions
    @see skiq_write_iq_pack_mode

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_mode  the currently set value of the pack mode setting
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_iq_pack_mode(uint8_t card,
                                        bool* p_mode);

/*****************************************************************************/
/** The skiq_write_iq_order_mode() function is responsible for setting the
    ordering of the complex samples for the Sidekiq card.  Each sample is 
    little-endian, twos-complement, signed, and sign-extended from 12 to 16-bits.
    (when appropriate for the product) By default samples are received/transmitted 
    as I/Q pairs with 'Q' sample occurring first, followed by the 'I' sample, as 
    depicted.

    <pre>
              skiq_iq_order_qi: (default)                skiq_iq_order_iq:
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

    @attention The iq order mode is only applied when tx/rx streaming is
    started and thus may not reflect the current iq order state.

    @attention If the iq order mode is set to skiq_iq_order_iq and an incompatible
    FPGA bitstream is then loaded via skiq_prog_fpga_from_file() or 
    skiq_prog_fpga_from_flash(), the mode will automatically revert to 
    skiq_iq_order_qi without warning.

    @since Function added in @b v4.10.0, requires FPGA @b v3.12.0 or later  

    @ingroup fpgafunctions
    @see skiq_read_iq_order_mode
    
    @param[in] card card index of the Sidekiq of interest
    @param[in] mode [::skiq_iq_order_t]   skiq_iq_order_qi = use Q/I order mode (default)
                                          skiq_iq_order_iq = use swapped order, I/Q 

    @return int32_t  status where 0=success, anything else is an error
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -ENOSYS if the FPGA version does not support IQ ordering mode
    @retval -ENOTSUP if IQ order mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL if an invalid IQ order is specified. See ::skiq_iq_order_t

*/

EPIQ_API int32_t skiq_write_iq_order_mode(uint8_t card,
                                          skiq_iq_order_t mode);

/*****************************************************************************/
/** The skiq_read_iq_order_mode() function is responsible for retrieving the
    current I/Q order mode setting for the Sidekiq card.

    @since Function added in @b v4.10.0, requires FPGA @b v3.12.0 or later

    @ingroup fpgafunctions
    @see skiq_write_iq_order_mode

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_mode [::skiq_iq_order_t]  the currently set value of the order mode setting

    @return int32_t  status where 0=success, anything else is an error
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EFAULT  NULL pointer detected for @p p_mode
    @retval -EIO A fault occurred communicating with the FPGA
    @retval -ENOSYS FPGA does not meet minimum interface version requirements
    
*/

EPIQ_API int32_t skiq_read_iq_order_mode(uint8_t card,
                                         skiq_iq_order_t* p_mode);
                                        
/*****************************************************************************/
/** The skiq_write_rx_data_src() function is responsible for setting the data
    source for the Rx interface.  This is typically complex I/Q samples, but can
    also be set to use an incrementing counter for various test purposes.  This
    must be set prior to calling skiq_start_rx_streaming() for the Rx interface.

    @warning If set after the Rx interface has been started, the setting will be
    stored but will not be used until streaming is stopped and re-started for
    the interface.

    @ingroup fpgafunctions
    @see skiq_read_rx_data_src
    @see skiq_receive
    @see skiq_start_rx_streaming
    @see skiq_start_rx_streaming_on_1pps

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested Rx interface
    @param[in] src [::skiq_data_src_t]  the source of the data (either ::skiq_data_src_iq or ::skiq_data_src_counter)
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_data_src(uint8_t card,
                                        skiq_rx_hdl_t hdl,
                                        skiq_data_src_t src);

/*****************************************************************************/
/** The skiq_read_rx_data_src() function is responsible for retrieving the
    currently set data source value (::skiq_data_src_t).

    @ingroup fpgafunctions
    @see skiq_write_rx_data_src

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested Rx interface
    @param[out] p_src [::skiq_data_src_t]  the currently set value of the pack mode setting
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_data_src(uint8_t card,
                                       skiq_rx_hdl_t hdl,
                                       skiq_data_src_t* p_src);

/*****************************************************************************/
/** The skiq_write_rx_stream_mode() function is responsible for setting the
    receive stream mode for a specified Sidekiq card.  This must be set prior
    to calling skiq_start_rx_streaming() for any Rx interface associated with
    the card.

    @warning If this function is called after @b any Rx interface has started
    streaming, the setting will be stored but will not be used until all receive
    streaming has stopped and re-started for the card.

    @attention If the receive stream mode is set to ::skiq_rx_stream_mode_low_latency and an
    incompatible FPGA bitstream is then loaded via skiq_prog_fpga_from_file(),
    skiq_prog_fpga_from_flash() or skiq_prog_fpga_from_flash_slot(), the mode will automatically
    revert to ::skiq_rx_stream_mode_high_tput without warning.

    @warning ::skiq_rx_stream_mode_low_latency conflicts with I/Q pack mode.  As
    such, caller may not configure a card to use both packed I/Q mode and RX low
    latency mode at the same time.  This function will return an error (@c
    -EPERM) if caller sets stream_mode to ::skiq_rx_stream_mode_low_latency and
    I/Q pack mode is currently set to @c true.

    @since Function added in @b v4.6.0, requires FPGA @b v3.9.0 or later

    @ingroup rxfunctions
    @see skiq_read_rx_stream_mode
    @see skiq_rx_stream_mode_t

    @param[in] card card index of the Sidekiq of interest
    @param[in] stream_mode [::skiq_rx_stream_mode_t]  the desired stream mode for the receive sample blocks

    @return int32_t
    @retval 0 successful setting of RX stream mode
    @retval -1 specified card index is out of range or has not been initialized
    @retval -ENOTSUP specified RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL specified RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
    @retval -EPERM I/Q packed mode is already enabled and conflicts with the requested RX stream mode
*/
EPIQ_API int32_t skiq_write_rx_stream_mode( uint8_t card,
                                            skiq_rx_stream_mode_t stream_mode );

/*****************************************************************************/
/** The skiq_read_rx_stream_mode() function is responsible for retrieving the
    currently stored receive stream mode (::skiq_rx_stream_mode_t).

    @attention The receive stream mode is only applied when receive streaming is
    started and thus may not reflect the current stream state.

    @since Function added in @b v4.6.0, requires FPGA @b v3.9.0 or later

    @ingroup rxfunctions
    @see skiq_write_rx_stream_mode
    @see skiq_rx_stream_mode_t

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_stream_mode [::skiq_rx_stream_mode_t]  the current value of the receive stream mode

    @return int32_t
    @retval 0 successful query of RX stream mode
    @retval -1 specified card index is out of range or has not been initialized
*/
EPIQ_API int32_t skiq_read_rx_stream_mode( uint8_t card,
                                           skiq_rx_stream_mode_t* p_stream_mode );

/**************************************************************************************************/
/** The skiq_read_curr_rx_timestamp() function is responsible for retrieving a current snapshot of
    the Rx timestamp counter (i.e., free-running counter) of the specified interface handle.  This
    timestamp is maintained by the FPGA and is shared across each RFIC regardless of the Rx or Tx
    interface.

    @note by the time the timestamp has been returned back to software, it will already be in the
    past, but this is still useful to determine if a specific timestamp has occurred already or not.

    @attention See @ref AD9361TimestampSlip for details on how calling this function can affect the
    RF timestamp metadata associated with received I/Q blocks.

    @ingroup timestampfunctions
    @see skiq_read_curr_tx_timestamp
    @see skiq_read_curr_sys_timestamp
    @see skiq_reset_timestamps
    @see skiq_update_timestamps

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the interface for which the current timestamp is being read
    @param[out] p_timestamp  a pointer to where the 64-bit timestamp value should be written

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EDOM       if the requested handle is not available or out of range for the Sidekiq platform
    @retval -EFAULT     if @p p_timestamp is NULL
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
*/
EPIQ_API int32_t skiq_read_curr_rx_timestamp(uint8_t card,
                                             skiq_rx_hdl_t hdl,
                                             uint64_t* p_timestamp);

/*****************************************************************************/
/** The skiq_read_curr_tx_timestamp() function is responsible for retrieving the
    currently set value for the timestamp (i.e., free-running counter) of the
    specified interface handle.  This timestamp is maintained by the FPGA and
    is shared across each RFIC regardless of the Rx or Tx interface.

    @note by the time the timestamp has been returned back to
    software, it will already be in the past, but this is still useful to
    determine if a specific timestamp has occurred already or not.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup timestampfunctions
    @see skiq_read_curr_rx_timestamp
    @see skiq_read_curr_sys_timestamp
    @see skiq_reset_timestamps
    @see skiq_update_timestamps

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] the handle of the interface for which the current timestamp
    is being read
    @param[out] p_timestamp  a pointer to where the 64-bit timestamp value
    should be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_curr_tx_timestamp(uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             uint64_t* p_timestamp);

/*****************************************************************************/
/** The skiq_read_curr_sys_timestamp() function is responsible for retrieving the
    currently set value for the system timestamp.  The system timestamp increments
    at the SKIQ_SYS_TIMESTAMP_FREQ rate.  This timestamp is maintained by the
    FPGA and increments independent of the sample rate.

    @note by the time the timestamp has been returned back to
    software, it will already be in the past, but this is still useful to
    determine if a specific timestamp has occurred already or not.

    @ingroup timestampfunctions
    @see skiq_read_curr_rx_timestamp
    @see skiq_read_curr_tx_timestamp
    @see skiq_reset_timestamps
    @see skiq_update_timestamps

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_timestamp  a pointer to where the 64-bit timestamp value
    should be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_curr_sys_timestamp(uint8_t card,
                                              uint64_t* p_timestamp);

/*****************************************************************************/
/** The skiq_reset_timestamp() function is responsible for resetting the
    timestamps (Rx/Tx and system) back to 0.

    @ingroup timestampfunctions
    @see skiq_read_curr_rx_timestamp
    @see skiq_read_curr_tx_timestamp
    @see skiq_read_curr_sys_timestamp
    @see skiq_update_timestamps

    @param[in] card card index of the Sidekiq of interest
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_reset_timestamps(uint8_t card);

/*****************************************************************************/
/** The skiq_update_timestamps() function is responsible for updating the
    both the RF and system timestamps to the value specified.

    @ingroup timestampfunctions
    @see skiq_read_curr_rx_timestamp
    @see skiq_read_curr_tx_timestamp
    @see skiq_read_curr_sys_timestamp
    @see skiq_reset_timestamps

    @param[in] card card index of the Sidekiq of interest
    @param[in] new_timestamp value to set both the RF and system timestamps to
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_update_timestamps(uint8_t card,
                                        uint64_t new_timestamp);

/*****************************************************************************/
/** The skiq_read_libsidekiq_version() function is responsible for returning the
    major/minor/patch/label revision numbers for the version of libsidekiq used
    by the application.  The label revision will be a qualitative description of
    the revision rather than defining the API revision level.

    @since Function signature modified in API @b v4.0.0 to add pointer to a
    revision label.

    @ingroup libfunctions

    @param[out] p_major  a pointer to where the major rev # should be written
    @param[out] p_minor  a pointer to where the minor rev # should be written
    @param[out] p_patch  a pointer to where the patch rev # should be written
    @param[out] p_label  a pointer which will be set to point to a NULL-terminated
                         string, which is possibly the empty string ""
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_libsidekiq_version(uint8_t* p_major,
                                              uint8_t* p_minor,
                                              uint8_t* p_patch,
                                              const char **p_label);

/*****************************************************************************/
/** The skiq_read_fpga_version() function is responsible for returning the
    major/minor revision numbers for the currently loaded FPGA bitstream.

    @deprecated Use skiq_read_fpga_semantic_version() and
    skiq_read_fpga_tx_fifo_size() instead of skiq_read_fpga_version()

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_git_hash  a pointer to where the 32-bit git hash will be written
    @param[out] p_build_date  a pointer to where the 32-bit build date will be
    written
    @param[out] p_major a pointer to where the major rev # should be written
    @param[out] p_minor a pointer to where the minor rev # should be written
    @param[out] p_tx_fifo_size a pointer to where the FPGA's TX FIFO size enumeration should be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_fpga_version(uint8_t card,
                                        uint32_t* p_git_hash,
                                        uint32_t* p_build_date,
                                        uint8_t *p_major,
                                        uint8_t *p_minor,
                                        skiq_fpga_tx_fifo_size_t *p_tx_fifo_size);

/*****************************************************************************/
/** The skiq_read_fpga_semantic_version() function is responsible for returning
    the major/minor/patch revision numbers for the currently loaded FPGA
    bitstream.

    @since Function added in API @b v4.4.0

    @ingroup fpgafunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_major a pointer to where the major rev # should be returned
    @param[out] p_minor a pointer to where the minor rev # should be returned
    @param[out] p_patch a pointer to where the patch rev # should be returned
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_fpga_semantic_version( uint8_t card,
                                                  uint8_t *p_major,
                                                  uint8_t *p_minor,
                                                  uint8_t *p_patch );

/*****************************************************************************/
/** The skiq_read_fpga_tx_fifo_size() function is responsible for returning the
    Transmit FIFO size (::skiq_fpga_tx_fifo_size_t representing the number
    of samples) for the currently loaded FPGA bitstream.

    @since Function added in API @b v4.4.0

    @ingroup fpgafunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_tx_fifo_size [::skiq_fpga_tx_fifo_size_t] reference to where the TX FIFO size enum should be returned
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_fpga_tx_fifo_size( uint8_t card,
                                              skiq_fpga_tx_fifo_size_t *p_tx_fifo_size );

/*****************************************************************************/
/** The skiq_read_fw_version() function is responsible for returning the
    major/minor revision numbers for the microcontroller firmware
    within the Sidekiq unit

    @note This is currently only supported if the USB interface has been
    initialized.

    @attention This function is valid only for @ref ::skiq_mpcie "mPCIe" and
    @ref ::skiq_m2 "M.2" and will otherwise return an error.

    @ingroup cardfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_major  a pointer to where the major rev # should be written
    @param[out] p_minor  a pointer to where the minor rev # should be written
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_fw_version(uint8_t card,
                                      uint8_t* p_major,
                                      uint8_t* p_minor);

/*****************************************************************************/
/** The skiq_read_hw_version() function is responsible for returning the
    hardware version number of the Sidekiq board.

    @deprecated

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_hw_version [::skiq_hw_vers_t]  a pointer to hold the hardware version
    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_hw_version(uint8_t card,
                                      skiq_hw_vers_t* p_hw_version);

/*****************************************************************************/
/** The skiq_read_product_version() function is responsible for returning the
    product version of the Sidekiq board.

    @deprecated

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_product [::skiq_product_t]  a pointer to hold the product version
    @return int32_t: status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_product_version(uint8_t card,
                                           skiq_product_t *p_product);

/*****************************************************************************/
/** The skiq_write_user_fpga_reg() function is used to update the 32-bit value
    of the requested user-definable FPGA register.  This function is useful when
    adding custom logic to the FPGA, which can then controlled by software
    through this interface.

    @ingroup fpgafunctions
    @see skiq_read_user_fpga_reg

    @param[in] card card index of the Sidekiq of interest
    @param[in] addr the register address to access in the FPGA's memory map
    @param[in] data the 32-bit value to be written to the requested FPGA reg
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_user_fpga_reg(uint8_t card,
                                          uint32_t addr,
                                          uint32_t data);

/*****************************************************************************/
/** The skiq_read_user_fpga_reg() function is responsible for reading out the
    current value in a user-definable FPGA register.

    @ingroup fpgafunctions
    @see skiq_write_user_fpga_reg

    @param[in] card card index of the Sidekiq of interest
    @param[in] addr the register address to access in the FPGA's memory map
    @param[out] p_data a pointer to a uint32_t to be updated with the current value
    of the requested FPGA register
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_user_fpga_reg(uint8_t card,
                                         uint32_t addr,
                                         uint32_t* p_data);

/*************************************************************************************************/
/** The skiq_write_and_verify_user_fpga_reg() function is used to update the 32-bit value of the
    requested user-definable FPGA register.  After the register has been written, this function
    verifies that reading the register returns the value previously written.  This is useful to
    ensure that an FPGA register contains the expected value.  This verification should be done in
    cases when performing a read immediately following the write since it is possible that the reads
    and writes could occur out-of-order, depending on the transport.  Additionally, this is useful
    to verify in the cases where the register clock is running at a slower rate, such as the sample
    rate clock.

    @ingroup fpgafunctions
    @see skiq_read_user_fpga_reg
    @see skiq_write_user_fpga_reg

    @since Function added in API @b v4.9.0

    @param[in] card card index of the Sidekiq of interest
    @param[in] addr the register address to access in the FPGA's memory map
    @param[in] data the 32-bit value to be written to the requested FPGA reg

    @retval 0 successful write and verification of user FPGA register
    @retval -EINVAL specified card index is out of range
    @retval -EFAULT addr is outside of valid FPGA user address range
    @retval -ENODEV specified card index has not been initialized
    @retval -EIO data readback does not match what was written
*/
EPIQ_API int32_t skiq_write_and_verify_user_fpga_reg(uint8_t card,
                                                     uint32_t addr,
                                                     uint32_t data);

/*****************************************************************************/
/**
    The skiq_prog_rfic_from_file() function is responsible for pushing
    down a configuration file to the RFIC to reconfigure it.  This allows
    libsidekiq-based apps to reconfigure the RFIC from a config file at
    run-time if needed.

    @note As of @b v3.5.0, programming the RFIC with a default configuration is
    part of skiq_init(), skiq_init_by_serial_str(), or skiq_enable_cards().

    @ingroup rficfunctions

    @param[in] fp pointer to the already opened file to load to the RFIC
    @param[in] card card index of the Sidekiq of interest
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_prog_rfic_from_file(FILE* fp,
                                          uint8_t card);

/**************************************************************************************************/
/**
   The skiq_prog_fpga_from_file() function is responsible for programming the FPGA with an already
   opened bitstream file. This allows libsidekiq-based apps to reprogram the FPGA at run-time if
   needed.

   @note After successful reprogramming is complete, all RX interfaces are reset to the idle (not
   streaming) state.

   @warning Not all Sidekiq products support programming the FPGA from a file.

   @ingroup fpgafunctions
   @see skiq_prog_fpga_from_flash
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_from_flash
   @see skiq_verify_fpga_config_in_flash_slot

   @param[in] card card index of the Sidekiq of interest
   @param[in] fp pointer to already opened configuration file

   @return 0 on success, else a negative errno value
   @retval -ERANGE The specified card index exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
   @retval -ENODEV A card was not detected at the specified card index
   @retval -ENOTSUP Configuring the FPGA from a file is not supported for this part
   @retval -EBADMSG Error occurred transacting with FPGA registers
   @retval -EIO Failed to configure the FPGA from the specified file pointer
   @retval -ESRCH Internal error, Sidekiq transport misidentified or invalid
   @retval -ERANGE Internal error, the system timestamp frequency indicated by the FPGA is out of range
   @retval -ENOTSUP Internal error, Sidekiq RFIC does not support querying system timestamp frequency
*/
EPIQ_API int32_t skiq_prog_fpga_from_file(uint8_t card,
                                          FILE* fp);


/**************************************************************************************************/
/**
   The skiq_prog_fpga_from_flash() function is responsible for programming the FPGA from the image
   previously stored in flash. This allows libsidekiq-based apps to reprogram the FPGA at run-time
   if needed.

   @note After successful reprogramming is complete, all RX interfaces are reset to the idle (not
   streaming) state.

   @ingroup fpgafunctions
   @see skiq_prog_fpga_from_file
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_from_flash
   @see skiq_verify_fpga_config_in_flash_slot

   @param[in] card card index of the Sidekiq of interest

   @return 0 on success, else a negative errno value
   @retval -ERANGE if the specified card index exceeds the maximum (::SKIQ_MAX_NUM_CARDS)
   @retval -ENODEV if a card was not detected at the specified card index
   @retval -EBADMSG Error occurred transacting with FPGA registers
   @retval -EIO Failed to configure the FPGA from the stored configuration bitstream
   @retval -ESRCH Internal error, Sidekiq transport misidentified or invalid
   @retval -ERANGE Internal error, the system timestamp frequency indicated by the FPGA is out of range
*/
EPIQ_API int32_t skiq_prog_fpga_from_flash(uint8_t card);


/**************************************************************************************************/
/**
   The skiq_save_fpga_config_to_flash() function stores a FPGA bitstream into flash memory, allowing
   it to be automatically loaded on power cycle or calling skiq_prog_fpga_from_flash().

   @ingroup fpgafunctions
   @see skiq_prog_fpga_from_file
   @see skiq_prog_fpga_from_flash
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_from_flash
   @see skiq_verify_fpga_config_in_flash_slot

   @param[in] card card index of the Sidekiq of interest
   @param[out] p_file pointer to the FILE containing the FPGA bitstream

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EBADF      if the FILE stream references a bad file descriptor
   @retval -ENODEV     if no entry is found in the flash configuration array
   @retval -EACCES     if no golden FPGA bitstream is found in flash memory
   @retval -EIO        if the transport failed to read from flash memory
   @retval -EFAULT     if @p p_file is NULL
   @retval -ENOTSUP    if Flash access isn't supported for this card
   @retval -EFBIG      if the write would exceed Flash address boundaries and/or the flash config slot's size
   @retval -EFAULT     if the file specified by @p p_file doesn't contain an FPGA sync word
   @retval -ENOENT     (Internal Error) if the Flash data structure hasn't been initialized for this card
*/
EPIQ_API int32_t skiq_save_fpga_config_to_flash(uint8_t card,
                                                FILE* p_file);


/**************************************************************************************************/
/**
   The skiq_verify_fpga_config_from_flash() function verifies the contents of flash memory against a
   given file. This can be used to validate that a given FPGA bitstream is accurately stored within
   flash memory.

   @since Function added in API @b v4.0.0

   @ingroup fpgafunctions
   @see skiq_prog_fpga_from_file
   @see skiq_prog_fpga_from_flash
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_in_flash_slot

   @param[in] card card index of the Sidekiq of interest
   @param[out] p_file pointer to the FILE containing the FPGA bitstream to verify

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EFAULT     if @p p_file is NULL
   @retval -ENOTSUP    if Flash access isn't supported for this card
   @retval -EFBIG      if the file exceeds the Flash address boundaries
   @retval -EIO        if the file could not be read from
   @retval -EXDEV      if the verification failed
   @retval -ENOENT     (Internal Error) if the Flash data structure hasn't been initialized for this card
*/
EPIQ_API int32_t skiq_verify_fpga_config_from_flash(uint8_t card,
                                                    FILE* p_file);


/*****************************************************************************/
/** The skiq_part_string() function returns a string representation of the
    passed in part value.

    @since Function added in API @b v4.4.0

    @ingroup cardfunctions

    @param[in] part [::skiq_part_t] Sidekiq part value
    @return const char* string representing the Sidekiq part
*/
EPIQ_API const char* skiq_part_string( skiq_part_t part );

/*****************************************************************************/
/** The skiq_hardware_vers_string() function returns a string representation
    of the passed in hardware version.

    @deprecated

    @param[in] hardware_vers hardware version value
    @return const char* string representing the hardware version
*/
EPIQ_API const char* skiq_hardware_vers_string( skiq_hw_vers_t hardware_vers );

/*****************************************************************************/
/** The skiq_product_vers_string() function returns a string representation
    of the passed in product version.

    @deprecated

    @param[in] product_vers product version value
    @return const char* string representing the product version
*/
EPIQ_API const char* skiq_product_vers_string( skiq_product_t product_vers );

/*****************************************************************************/
/** The skiq_rf_port_string() function returns a string representation
    of the passed in @ref skiq_rf_port_t.

    @since Function added in API @b v4.5.0

    @ingroup rfportfunctions

    @param[in] rf_port RF port value
    @return const char* string representing the RF port
*/
EPIQ_API const char* skiq_rf_port_string( skiq_rf_port_t rf_port );

/*****************************************************************************/
/** The skiq_part_info() function returns strings representing the various
    components of a part.

    @since Function added in API @b v4.2.0

    @ingroup cardfunctions

    @param[in] card card index of Sidekiq of interest
    @param[out] p_part_number pointer to where to store the part number (ex: "020201")
       Must be able to contain SKIQ_PART_NUM_STRLEN # of bytes.
    @param[out] p_revision pointer to where to store the revision. (ex: "B0")
       Must be able to contain SKIQ_REVISION_STRLEN # of bytes.
    @param[out] p_variant pointer to where to store the variant. (ex: "01")
       Must be able to contain SKIQ_VARIANT_STRLEN # of bytes.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_part_info( uint8_t card,
                                      char *p_part_number,
                                      char *p_revision,
                                      char *p_variant );

/*****************************************************************************/
/** The skiq_read_max_sample_rate() function returns the maximum sample rate
    possible for the Sidekiq card requested based on the current channel mode
    and product.

    @since Function added in API @b v4.2.0

    @deprecated This function has been deprecated and may not return the
    correct maximum sample rate for all handles, this has been replaced
    with @ref skiq_read_parameters.

    @param[in] card card index of Sidekiq of interest
    @param[out] p_max_sample_rate pointer to where to store the maximum sample rate
    @return const char* string representing the product version
*/
EPIQ_API int32_t skiq_read_max_sample_rate( uint8_t card,
                                            uint32_t *p_max_sample_rate );

/*****************************************************************************/
/** The skiq_read_min_sample_rate() function returns the minimum sample rate
    possible for the Sidekiq card requested based on the product.

    @since Function added in API @b v4.2.0

    @deprecated This function has been deprecated and may not return the
    correct minimum sample rate for all handles, this has been replaced
    with @ref skiq_read_parameters.

    @param[in] card card index of Sidekiq of interest
    @param[out] p_min_sample_rate pointer to where to store the minimum sample rate
    @return const char* string representing the product version
*/
EPIQ_API int32_t skiq_read_min_sample_rate( uint8_t card,
                                            uint32_t *p_min_sample_rate );

/*****************************************************************************/
/** The skiq_write_rx_dc_offset_corr() function is used to configure the DC
    offset correction block in the FPGA.  This is a simple 1-pole filter with
    a knee very close to DC.

    @ingroup rxfunctions
    @see skiq_read_rx_dc_offset_corr

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the Rx interface to access
    @param[in] enable true to enable the DC offset correction block
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_dc_offset_corr( uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               bool enable );

/*****************************************************************************/
/** The skiq_read_rx_dc_offset_corr() function is responsible for returning
    whether the FPGA-based DC offset correction block is enabled.

    @ingroup rxfunctions
    @see skiq_write_rx_dc_offset_corr

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the Rx interface to access
    @param[out] p_enable pointer to where to store the enable state, true
    indicates that DC offset correction block is enabled
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_dc_offset_corr( uint8_t card,
                                              skiq_rx_hdl_t hdl,
                                              bool *p_enable );

/*****************************************************************************/
/** The skiq_read_rfic_reg() function reads the value of the RFIC register
    specified.

    @ingroup rficfunctions
    @see skiq_write_rfic_reg

    @param[in] card card index of the Sidekiq of interest
    @param[in] addr RFIC register address to read
    @param[out] p_data pointer to where to store the value read
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_read_rfic_reg(uint8_t card,
                                    uint16_t addr,
                                    uint8_t *p_data);

/*****************************************************************************/
/** The skiq_write_rfic_reg() function writes the data specified to the RFIC
    register provided.

    @attention writing directly to RFIC registers is not recommended.  Modifying
    register settings may result in incorrect or unexpected behavior.

    @ingroup rficfunctions
    @see skiq_read_rfic_reg

    @param[in] card card index of the Sidekiq of interest
    @param[in] addr RFIC register address to write to
    @param[in] data value to actually write to the register
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_write_rfic_reg(uint8_t card,
                                     uint16_t addr,
                                     uint8_t data);

/*****************************************************************************/
/** The skiq_read_rfic_tx_fir_config() function provides access to the current
    number of Tx FIR taps as well as the Tx FIR interpolation.

    @warning any modification of the sample rate and/or channel bandwidth may
    result in a change of the number of taps and/or the interpolation factor.

    @ingroup rficfunctions
    @see skiq_read_rfic_tx_fir_coeffs
    @see skiq_write_rfic_tx_fir_coeffs

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_num_taps pointer to where to store the number of taps
    @param[out] p_fir_interpolation pointer to where to store the interpolation
    factor of the Tx FIR
    @return int32_t status of the operation (0=success, anything else is an
    error)
*/
EPIQ_API int32_t skiq_read_rfic_tx_fir_config( uint8_t card,
                                               uint8_t *p_num_taps,
                                               uint8_t *p_fir_interpolation );

/*****************************************************************************/
/** The skiq_read_rfic_tx_fir_coeffs() function provides access to the current
    Tx FIR coefficients programmed.  To determine the number of taps and
    the interpolation factor of the FIR, use skiq_read_rfic_tx_fir_config().

    @warning any modification of the sample rate and/or channel bandwidth will
    result in an update of the FIR configuration and coefficients.

    @ingroup rficfunctions
    @see skiq_read_rfic_tx_fir_config
    @see skiq_write_rfic_tx_fir_coeffs

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_coeffs pointer to where to store the FIR coefficients
    @return int32_t status of the operation (0=success, anything else is an
    error)
*/
EPIQ_API int32_t skiq_read_rfic_tx_fir_coeffs( uint8_t card,
                                               int16_t *p_coeffs );

/*****************************************************************************/
/** The skiq_write_rfic_tx_fir_coeffs() function allows the coefficients of the
    Tx FIR to be written.  The number of taps and interpolation factor are
    determined by the sample rate and can be queried using
    skiq_read_rfic_tx_fir_config().

    @note Any modification of the Rx/Tx sample rate and/or channel bandwidth
    will result in a change of the coefficients programmed.  If a custom setting
    is used, the Rx/Tx sample rate and bandwidth must be performed first
    (skiq_write_rx_sample_rate_and_bandwidth() and
    skiq_write_tx_sample_rate_and_bandwidth()) after which
    skiq_write_rfic_tx_fir_coeffs() may be called.  Additionally, the analog
    filters will be configured based on the configured channel bandwidth.  For
    any sample rate which results in a interpolation setting of 4 results in the
    automatic doubling of FIR coefficients.  The skiq_read_rfic_tx_fir_coeffs()
    returns the actual coefficient values programmed.

    @attention Writing the FIR coefficients directly using this function is not
    recommended.  Modifying the FIR coefficients may result in incorrect or
    unexpected behavior.

    @ingroup rficfunctions
    @see skiq_read_rfic_tx_fir_config
    @see skiq_read_rfic_tx_fir_coeffs

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_coeffs pointer to where the Tx FIR coefficients are located
    @return int32_t status of the operation (0=success, anything else is an
    error)
*/
EPIQ_API int32_t skiq_write_rfic_tx_fir_coeffs( uint8_t card,
                                                int16_t *p_coeffs );

/*****************************************************************************/
/** The skiq_read_rfic_rx_fir_config() function provides access to the current
    number of Rx FIR taps as well as the Rx FIR decimation.

    @warning any modification of the sample rate and/or channel bandwidth may
    result in a change of number of taps and/or the decimation factor.

    @ingroup rficfunctions
    @see skiq_read_rfic_rx_fir_coeffs
    @see skiq_write_rfic_rx_fir_coeffs

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_num_taps pointer to where to store the number of taps
    @param[out] p_fir_decimation pointer to where to store the FIR decimation factor
    @return int32_t status of the operation (0=success, anything else is an
    error)
*/
EPIQ_API int32_t skiq_read_rfic_rx_fir_config( uint8_t card,
                                               uint8_t *p_num_taps,
                                               uint8_t *p_fir_decimation );

/*****************************************************************************/
/** The skiq_read_rfic_rx_fir_coeffs() function provides access to the current
    Rx FIR coefficients programmed.  To determine the number of taps and
    the decimation factor of the Rx FIR, use skiq_read_rfic_rx_fir_config().

    @warning any modification of the sample rate and/or channel bandwidth will
    result in of the FIR configuration and coefficients.

    @ingroup rficfunctions
    @see skiq_read_rfic_rx_fir_config
    @see skiq_write_rfic_rx_fir_coeffs

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_coeffs pointer to where to store the FIR coefficients
    @return int32_t status of the operation (0=success, anything else is an
    error)
*/
EPIQ_API int32_t skiq_read_rfic_rx_fir_coeffs(uint8_t card,
                                              int16_t *p_coeffs);

/*****************************************************************************/
/** The skiq_write_rfic_rx_fir_coeffs() function allows the coefficients of the
    Rx FIR to be written.  The number of taps and interpolation factor are
    determined by the sample rate and can be queried using
    skiq_read_rfic_rx_fir_config().

    @note any modification of the Rx/Tx sample rate and/or channel bandwidth
    will result in a change of the coefficients programmed.  If a custom setting
    is used, the Rx/Tx sample rate and bandwidth must be performed first
    (skiq_write_rx_sample_rate_and_bandwidth() and
    skiq_write_tx_sample_rate_and_bandwidth()) after which
    skiq_write_rfic_rx_fir_coeffs() may be called.  Additionally, the analog
    filters will be configured based on the configured channel bandwidth.

    @attention Writing the FIR coefficients directly using this function is not
    recommended.  Modifying the FIR coefficients may result in incorrect or
    unexpected behavior.

    @ingroup rficfunctions
    @see skiq_read_rfic_rx_fir_config
    @see skiq_read_rfic_rx_fir_coeffs

    @param[in] card card index of the Sidekiq of interest
    @param[in] p_coeffs pointer to where the Rx FIR coefficients are located
    @return int32_t status of the operation (0=success, anything else is an
    error)
*/
EPIQ_API int32_t skiq_write_rfic_rx_fir_coeffs(uint8_t card,
                                               int16_t *p_coeffs);

/*****************************************************************************/
/** The skiq_write_rx_fir_gain() function is responsible for configuring the
    gain of the Rx FIR filter.  The Rx FIR filter is used in configuring the
    Rx channel bandwidth.

    @ingroup rxfunctions
    @see skiq_read_rx_fir_gain

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] the handle of the Rx interface to access
    @param[in] gain [::skiq_rx_fir_gain_t] gain of the filter
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_write_rx_fir_gain(uint8_t card,
                                        skiq_rx_hdl_t hdl,
                                        skiq_rx_fir_gain_t gain);

/*****************************************************************************/
/** The skiq_read_rx_fir_gain() function is responsible for reading the
    gain of the Rx FIR filter.  The Rx FIR filter is used in configuring the
    Rx channel bandwidth.

    @ingroup rxfunctions
    @see skiq_write_rx_fir_gain

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] the handle of the Rx interface to access
    @param[out] p_gain [::skiq_rx_fir_gain_t] pointer to where to store the gain setting
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_read_rx_fir_gain(uint8_t card,
                                       skiq_rx_hdl_t hdl,
                                       skiq_rx_fir_gain_t *p_gain);

/*****************************************************************************/
/** The skiq_write_tx_fir_gain() function is responsible for configuring the
    gain of the Tx FIR filter.  The Tx FIR filter is used in configuring the
    Tx channel bandwidth.

    @ingroup txfunctions
    @see skiq_read_tx_fir_gain

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] the handle of the Tx interface to access
    @param[in] gain [::skiq_tx_fir_gain_t] gain of the filter
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_write_tx_fir_gain(uint8_t card,
                                        skiq_tx_hdl_t hdl,
                                        skiq_tx_fir_gain_t gain);

/*****************************************************************************/
/** The skiq_read_tx_fir_gain() function is responsible for reading the
    gain of the Tx FIR filter.  The Tx FIR filter is used in configuring the
    Tx channel bandwidth.

    @ingroup txfunctions
    @see skiq_write_tx_fir_gain

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] the handle of the Tx interface to access
    @param[out] p_gain [::skiq_tx_fir_gain_t] pointer to where to store the gain setting
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_read_tx_fir_gain(uint8_t card,
                                     skiq_tx_hdl_t hdl,
                                     skiq_tx_fir_gain_t *p_gain );

/*****************************************************************************/
/** The skiq_read_ref_clock_select() function is responsible for reading the
    reference clock configuration.

    @attention this is not supported on rev B mPCIe

    @ingroup cardfunctions
    @see skiq_read_ext_ref_clock_freq
    @see skiq_write_ref_clock_select

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_ref_clk [::skiq_ref_clock_select_t] pointer to where to store the reference clock setting
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_read_ref_clock_select( uint8_t card,
                                             skiq_ref_clock_select_t *p_ref_clk );

/*****************************************************************************/
/** The skiq_read_ext_ref_clock_freq() function is responsible for reading the
    external reference clock's configured frequency.

    @note The default value is 40MHz if not configured.

    @note This function is only supported for mPCIe and M.2 Sidekiq variants.

    @since Function added in API @b v4.2.0

    @ingroup cardfunctions
    @see skiq_read_ref_clock_select

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_freq pointer to where to store the external clock's frequency
    @return int32_t status of the operation (0=success, anything else is
    an error code)
*/
EPIQ_API int32_t skiq_read_ext_ref_clock_freq( uint8_t card,
                                               uint32_t *p_freq );

/*****************************************************************************/
/** The skiq_read_num_tx_threads() function is responsible for returning the
    number of threads used to transfer data when operating in @ref
    skiq_tx_transfer_mode_async "asynchronous mode".

    @ingroup txfunctions
    @see skiq_write_num_tx_threads
    @see skiq_read_tx_thread_priority
    @see skiq_write_tx_thread_priority

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_num_threads pointer to where to store the number of threads
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_num_tx_threads( uint8_t card,
                                           uint8_t *p_num_threads );

/*****************************************************************************/
/** The skiq_write_num_tx_threads() function is responsible for updating the
    number of threads used to transfer data when operating in @ref
    skiq_tx_transfer_mode_async "asynchronous mode".  This must be set prior to
    the Tx interface being started.  If set after the Tx interface has been
    started, the setting will be stored but will not be used until the interface
    is stopped and re-started.

    @ingroup txfunctions
    @see skiq_read_num_tx_threads
    @see skiq_read_tx_thread_priority
    @see skiq_write_tx_thread_priority

    @param[in] card card index of the Sidekiq of interest
    @param[in] num_threads number of threads to use when running in Tx @ref skiq_tx_transfer_mode_async "asynchronous mode"
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_num_tx_threads( uint8_t card,
                                            uint8_t num_threads );

/*****************************************************************************/
/** The skiq_read_tx_thread_priority() function is responsible for returning the
    priority of the threads when operating in @ref skiq_tx_transfer_mode_async
    "asynchronous mode".

    @ingroup txfunctions
    @see skiq_read_num_tx_threads
    @see skiq_write_num_tx_threads
    @see skiq_write_tx_thread_priority

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_priority pointer to where to store the priority of the TX threads
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_thread_priority( uint8_t card,
                                               int32_t *p_priority );

/*****************************************************************************/
/** The skiq_write_tx_thread_priority() function is responsible for updating the
    priority of the threads used to transfer data when operating in @ref
    skiq_tx_transfer_mode_async "asynchronous mode".  This must be set prior to
    the Tx interface being started.  If set after the Tx interface has been
    started, the setting will be stored but will not be used until the interface
    is stopped and re-started.

    @ingroup txfunctions
    @see skiq_read_num_tx_threads
    @see skiq_write_num_tx_threads
    @see skiq_read_tx_thread_priority

    @param[in] card card index of the Sidekiq of interest
    @param[in] priority TX thread priority
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_thread_priority( uint8_t card,
                                              int32_t priority );

/*****************************************************************************/
/** The skiq_register_critical_error_callback() function allows a custom handler
    to be registered in the case of critical errors.  If a critical error occurs
    and a callback function is registered, then the critical_handler will be
    called.  If no handler is registered, then exit() is called.  Continued use
    of libsidekiq after a critical error has occurred will result in undefined
    behavior.

    @ingroup libfunctions
    @see skiq_register_logging

    @param[in] critical_handler function pointer to handler to call in the case of
    a critical error.  If no handler is registered, exit() will be called.
    @param[in] p_user_data a pointer to user data to be provided as an argument to
    the critical_handler function when called. This can safely be set to NULL
    if not needed. However, this will cause the argument of the critical handler
    to also be set to NULL.
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API void skiq_register_critical_error_callback( void (*critical_handler)(int32_t status, void* p_user_data),
                                                     void* p_user_data );

/*****************************************************************************/
/** The skiq_register_logging() function allows a custom logging handler to be
    registered.  The priority (as by the SKIQ_LOG_* definitions) and the logging
    message are provided to the function.  If no callback is registered, the
    logging messages are displayed in the console as well as syslog.  If it is
    desired to completely disable any output of the library NULL can be
    registered for the logging function, in which case no logging will occur.

    @ingroup libfunctions
    @see skiq_register_critical_error_callback

    @param[in] log_msg function pointer to handler to call when logging a message
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API void skiq_register_logging( void (*log_msg)(int32_t priority,
                                                     const char *message) );

/*****************************************************************************/
/** The skiq_read_num_rx_chans() function is responsible for returning the number
    of Rx channels supported for the Sidekiq card of interest.  The handle for
    the first Rx interface is ::skiq_rx_hdl_A1 and increments from there.

    @ingroup rxfunctions
    @see skiq_rx_hdl_t

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_num_rx_chans pointer to the number of Rx channels
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_num_rx_chans( uint8_t card,
                                         uint8_t *p_num_rx_chans );

/*****************************************************************************/
/** The skiq_read_num_tx_chans() function is responsible for returning the number
    of Tx channels supported for the Sidekiq card of interest.  The handle for
    the first Tx interface is ::skiq_tx_hdl_A1 and increments from there.

    @ingroup txfunctions
    @see skiq_tx_hdl_t

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_num_tx_chans pointer to the number of Tx channels
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_num_tx_chans( uint8_t card,
                                         uint8_t *p_num_tx_chans );

/*****************************************************************************/
/** The skiq_read_rx_iq_resolution() function is responsible for returning the
    resolution (in bits) per RX (ADC) IQ sample.

    @since Function added in API @b v4.2.0

    @ingroup rxfunctions
    @see skiq_receive
    @see skiq_rx_block_t

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_adc_res pointer to where to store the ADC resolution
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_iq_resolution( uint8_t card,
                                             uint8_t *p_adc_res );

/*****************************************************************************/
/** The skiq_read_tx_iq_resolution() function is responsible for returning the
    resolution (in bits) per TX (DAC) IQ sample.

    @since Function added in API @b v4.2.0

    @ingroup txfunctions
    @see skiq_transmit
    @see skiq_tx_block_t

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_dac_res pointer to the number of DAC bits
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_iq_resolution( uint8_t card,
                                             uint8_t *p_dac_res );

/*****************************************************************************/
/** The skiq_read_golden_fpga_present_in_flash() function is responsible for
    determining if there is a valid golden image stored in flash.  The
    p_present is set based on whether a golden FPGA image is detected:
    - 1 means the golden (fallback) FPGA is present
    - 0 means the golden (fallback) FPGA is @b NOT present

    @ingroup fpgafunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_present pointer to where to store an indication of whether the golden image is present
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_golden_fpga_present_in_flash( uint8_t card,
                                                         uint8_t *p_present );

/*****************************************************************************/
/** The skiq_read_rfic_control_output_rx_gain_config() function provides the
    mode and enable settings to configure the control output to present the
    gain of the handle specified.

    @ingroup rficfunctions
    @see skiq_write_rfic_control_output_config
    @see skiq_read_rfic_control_output_config
    @see skiq_rx_block_t::rfic_control

    @since Function added in @b v4.9.0, requires FPGA @b v3.11.0 or later for Sidekiq X2 and X4

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] RX handle of the gain setting to present in control output
    @param[out] p_mode pointer to where to store the control output mode setting
    @param[out] p_ena pointer to where to store the control output enable setting
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rfic_control_output_rx_gain_config( uint8_t card,
                                                               skiq_rx_hdl_t hdl,
                                                               uint8_t *p_mode,
                                                               uint8_t *p_ena );

/*****************************************************************************/
/** The skiq_write_rfic_control_output_config() function allows the control
    output configuration of the RFIC to be configured. The control output
    readings are included within each receive packet's metadata
    (skiq_rx_block_t::rfic_control).

    For details on the fields available for the control output, refer to the
    "Monitor Output" section of the appropriate reference manual.

    - For Sidekiq mPCIe / m.2 / Z2, refer to p.73 of the <a
    href="http://www.analog.com/media/en/technical-documentation/user-guides/AD9361_Reference_Manual_UG-570.pdf">AD9361
    Reference Manual UG-570</a>

    - For Sidekiq X2, refer to Table 142 on p.192 of the AD9371 User Guide (UG-992)

    - For Sidekiq X4, refer to Table 130 on p.214 of the ADRV9008-1/ADRV9008-2/ADRV9009 Hardware Reference Manual UG-1295

    @ingroup rficfunctions
    @see skiq_read_rfic_control_output_config
    @see skiq_rx_block_t::rfic_control
    @see skiq_read_rfic_control_output_rx_gain_config

    @param[in] card card index of the Sidekiq of interest
    @param[in] mode control output mode
    @param[in] ena control output enable
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rfic_control_output_config( uint8_t card,
                                                        uint8_t mode,
                                                        uint8_t ena );

/*****************************************************************************/
/** The skiq_read_rfic_control_output_config() function allows the control
    output configuration of the RFIC to be read.

    For details on the fields available for the control output, refer to the
    "Monitor Output" section of the appropriate reference manual.

    For Sidekiq mPCIe / m.2 / Z2, refer to p.73 of the <a
    href="http://www.analog.com/media/en/technical-documentation/user-guides/AD9361_Reference_Manual_UG-570.pdf">AD9361
    Reference Manual UG-570</a>:

    - For Sidekiq X2, refer to Table 142 on p.192 of the AD9371 User Guide (UG-992):

    - For Sidekiq X4, refer to Table 130 on p.214 of the ADRV9008-1/ADRV9008-2/ADRV9009 Hardware Reference Manual UG-1295:

    @ingroup rficfunctions
    @see skiq_write_rfic_control_output_config
    @see skiq_read_rfic_control_output_rx_gain_config
    @see skiq_rx_block_t::rfic_control

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_mode pointer to where to store the control output mode setting
    @param[out] p_ena pointer to where to store the control output enable setting
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rfic_control_output_config( uint8_t card,
                                                       uint8_t *p_mode,
                                                       uint8_t *p_ena );

/*****************************************************************************/
/** The skiq_enable_rfic_control_output_rx_gain() function applies the RFIC
    mode and enable settings to configure the control output to represent the
    gain of the handle specified.  This is equivalent to calling
    skiq_read_rfic_control_output_rx_gain_config() followed by
    skiq_write_rfic_control_output_config() with the appropriate mode and enable
    settings for the RX handle.

    @ingroup rficfunctions
    @see skiq_write_rfic_control_output_config
    @see skiq_read_rfic_control_output_config
    @see skiq_rx_block_t::rfic_control

    @since Function added in @b v4.9.0, requires FPGA @b v3.11.0 or later for Sidekiq X2 and X

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] RX handle of the gain setting to present in control output
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_enable_rfic_control_output_rx_gain( uint8_t card,
                                                          skiq_rx_hdl_t hdl );

/*****************************************************************************/
/** The skiq_read_rx_LO_freq_range() function allows an application to obtain
    the maximum and minimum LO frequencies that a Sidekiq can tune to receive.
    This information may also be accessed using skiq_read_parameters().

    @ingroup rxfunctions
    @see skiq_read_max_rx_LO_freq
    @see skiq_read_min_rx_LO_freq
    @see skiq_read_parameters
    @see skiq_rx_param_t::lo_freq_min
    @see skiq_rx_param_t::lo_freq_max

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_max pointer to update with maximum LO frequency
    @param[out] p_min pointer to update with minimum LO frequency
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_LO_freq_range( uint8_t card,
                                             uint64_t* p_max,
                                             uint64_t* p_min );

/*****************************************************************************/
/** The skiq_read_max_rx_LO_freq() function allows an application to obtain the
    maximum LO frequency that a Sidekiq can tune to receive.  This information
    may also be accessed using skiq_read_parameters().

    @ingroup rxfunctions
    @see skiq_read_rx_LO_freq_range
    @see skiq_read_min_rx_LO_freq
    @see skiq_read_parameters
    @see skiq_rx_param_t::lo_freq_max

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_max pointer to update with maximum LO frequency
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_max_rx_LO_freq( uint8_t card,
                                           uint64_t* p_max );

/*****************************************************************************/
/** The skiq_read_min_rx_LO_freq() function allows an application to obtain
    minimum LO frequency that a Sidekiq can tune to receive.  This information
    may also be accessed using skiq_read_parameters().

    @ingroup rxfunctions
    @see skiq_read_rx_LO_freq_range
    @see skiq_read_max_rx_LO_freq
    @see skiq_read_parameters
    @see skiq_rx_param_t::lo_freq_min

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_min pointer to update with minimum LO frequency
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_min_rx_LO_freq( uint8_t card,
                                           uint64_t* p_min );

/*****************************************************************************/
/** The skiq_read_tx_LO_freq_range() function allows an application to obtain
    the maximum and minimum frequencies that a Sidekiq can tune to transmit.
    This information may also be accessed using skiq_read_parameters().

    @ingroup txfunctions
    @see skiq_read_max_tx_LO_freq
    @see skiq_read_min_tx_LO_freq
    @see skiq_read_parameters
    @see skiq_tx_param_t::lo_freq_min
    @see skiq_tx_param_t::lo_freq_max

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_max pointer to update with maximum LO frequency
    @param[out] p_min pointer to update with minimum LO frequency
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_LO_freq_range( uint8_t card,
                                             uint64_t* p_max,
                                             uint64_t* p_min );

/*****************************************************************************/
/** The skiq_read_max_tx_LO_freq() function allows an application to obtain the
    maximum frequency that a Sidekiq can tune to transmit.  This information may
    also be accessed using skiq_read_parameters().

    @ingroup txfunctions
    @see skiq_read_tx_LO_freq_range
    @see skiq_read_min_tx_LO_freq
    @see skiq_read_parameters
    @see skiq_tx_param_t::lo_freq_max

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_max pointer to update with maximum LO frequency
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_max_tx_LO_freq( uint8_t card,
                                           uint64_t* p_max );

/*****************************************************************************/
/** The skiq_read_min_tx_LO_freq() function allows an application to obtain
    minimum frequency that a Sidekiq can tune to transmit at.  This information may
    also be accessed using skiq_read_parameters().

    @ingroup txfunctions
    @see skiq_read_tx_LO_freq_range
    @see skiq_read_max_tx_LO_freq
    @see skiq_read_parameters
    @see skiq_tx_param_t::lo_freq_min

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_min pointer to update with minimum LO frequency
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_min_tx_LO_freq( uint8_t card,
                                           uint64_t* p_min );

/*****************************************************************************/
/** The skiq_read_rx_filters_avail() function allows an application to obtain
    the preselect filters available for the specified card and handle.

    @note By default, when the LO frequency of the handle is adjusted, the
    filter encompassing the configured LO frequency is automatically configured.

    @warning There will never be more than ::skiq_filt_invalid filters returned
    and p_filters should be sized such that it can hold that many filter values.

    @since Function added in API @b v4.2.0

    @ingroup cardfunctions
    @see skiq_read_filter_range
    @see skiq_read_rx_preselect_filter_path
    @see skiq_read_tx_filters_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] RX handle of the filter availability in question
    @param[out] p_filters [:skiq_filt_t] pointer to list of filters available
    @param[out] p_num_filters pointer to where to store the number of filters
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_filters_avail( uint8_t card,
                                             skiq_rx_hdl_t hdl,
                                             skiq_filt_t *p_filters,
                                             uint8_t *p_num_filters );

/*****************************************************************************/
/** The skiq_read_tx_filters_avail() function allows an application to obtain
    the preselect filters available for the specified card and handle.

    @note by default, when the LO frequency of the handle is adjusted, the
    filter encompassing the configured LO frequency is automatically configured.

    @warning There will never be more than ::skiq_filt_invalid filters returned
    and p_filters should be sized such that it can hold that many filter values.

    @since Function added in API @b v4.2.0

    @ingroup cardfunctions
    @see skiq_read_filter_range
    @see skiq_read_tx_filter_path
    @see skiq_read_rx_filters_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] TX handle of the filter availability in question
    @param[out] p_filters [::skiq_filt_t] pointer to list of filters available
    @param[out] p_num_filters pointer to where to store the number of filters
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_filters_avail( uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             skiq_filt_t *p_filters,
                                             uint8_t *p_num_filters);

/*****************************************************************************/
/** The skiq_read_filter_range() function provides a mechanism to determine
    the frequency range covered by the specified filter.

    @since Function added in API @b v4.2.0

    @ingroup cardfunctions
    @see skiq_read_tx_filters_avail
    @see skiq_read_rx_filters_avail

    @param[in] filter [::skiq_filt_t] filter of interest
    @param[out] p_start_freq pointer to where to store the start frequency covered by the filter
    @param[out] p_end_freq pointer to where to store the end frequency covered by the filter
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_filter_range( skiq_filt_t filter,
                                         uint64_t *p_start_freq,
                                         uint64_t *p_end_freq );

/*****************************************************************************/
/** The skiq_read_rf_port_config_avail() function determines the RF port
    configuration options supported by the specified Sidekiq.  The RF port
    configuration controls the Rx/Tx capabilities for a given RF port.

    @ingroup rfportfunctions
    @see skiq_read_rf_port_config
    @see skiq_write_rf_port_config
    @see skiq_read_rf_port_operation
    @see skiq_write_rf_port_operation

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_fixed pointer indicating if fixed RF port config available
    @param[out] p_trx pointer indicating if TRX RF port config avail
    @return 0 on success, else a negative errno value
    @retval -ERANGE if the requested card index is out of range
    @retval -ENODEV if the requested card index is not initialized
    @retval -EINVAL reference to p_fixed or p_trx is NULL
*/

EPIQ_API int32_t skiq_read_rf_port_config_avail( uint8_t card,
                                                 bool *p_fixed,
                                                 bool *p_trx );

/*****************************************************************************/
/** The skiq_read_rf_port_config() function reads the current RF port
    configuration for the specified Sidekiq.

    @ingroup rfportfunctions
    @see skiq_read_rf_port_config_avail
    @see skiq_write_rf_port_config
    @see skiq_read_rf_port_operation
    @see skiq_write_rf_port_operation

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_config [::skiq_rf_port_config_t] pointer to the current antenna configuration
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rf_port_config( uint8_t card,
                                           skiq_rf_port_config_t *p_config );

/*****************************************************************************/
/** The skiq_write_rf_port_config() function allows the RF port configuration of
    the Sidekiq card specified to be configured.  To determine the available RF
    port configuration options, use skiq_read_rf_port_config_avail().

    @note Only particular hardware variants support certain RF port
    configurations.

    @ingroup rfportfunctions
    @see skiq_read_rf_port_config
    @see skiq_read_rf_port_config_avail
    @see skiq_read_rf_port_operation
    @see skiq_write_rf_port_operation

    @param[in] card card index of the Sidekiq of interest
    @param[in] config [::skiq_rf_port_config_t] RF port configuration to apply
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rf_port_config( uint8_t card,
                                            skiq_rf_port_config_t config );

/*****************************************************************************/
/** The skiq_read_rf_port_operation() function reads the operation mode of the
    RF port(s).  If the transmit flag is set, then the port(s) are configured to
    transmit, otherwise it is configured for receive.

    @ingroup rfportfunctions
    @see skiq_read_rf_port_config
    @see skiq_read_rf_port_config_avail
    @see skiq_write_rf_port_config
    @see skiq_write_rf_port_operation

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_transmit pointer to flag indicating whether to transmit or receive
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rf_port_operation( uint8_t card,
                                              bool *p_transmit );

/*****************************************************************************/
/** The skiq_write_rf_port_operation() function sets the operation mode of the
    RF port(s) to either transmit or receive.  If the transmit flag is set, then
    the port(s) are configured to transmit, otherwise it is configured for
    receive.

    @ingroup rfportfunctions
    @see skiq_read_rf_port_config
    @see skiq_read_rf_port_config_avail
    @see skiq_write_rf_port_config
    @see skiq_read_rf_port_operation

    @param[in] card card index of the Sidekiq of interest
    @param[in] transmit flag indicating whether to transmit or receive
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rf_port_operation( uint8_t card,
                                               bool transmit );

/*****************************************************************************/
/** The skiq_read_rx_rf_ports_avail_for_hdl() function reads a list of RF ports
    supported for the specified RX handle.

    @since Function added in API @b v4.5.0

    @note The fixed port list is only available for use when the RF port
    configuration is set to @ref skiq_rf_port_config_fixed.  The TRx port list is only
    available for use when the RF port configuration is set to @ref skiq_rf_port_config_trx.

    @note p_num_fixed_rf_port_list and p_trx_rf_port_list must contain at least
    skiq_rf_port_max number of elements.

    @ingroup rfportfunctions
    @see skiq_read_rx_rf_port_for_hdl
    @see skiq_write_rx_rf_port_for_hdl

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl RX handle of interest
    @param[out] p_num_fixed_rf_ports pointer to the number of fixed RF ports available
    @param[out] p_fixed_rf_port_list [::skiq_rf_port_t] pointer list of fixed RF ports
    @param[out] p_num_trx_rf_ports pointer to the number of TRX RF ports available
    @param[out] p_trx_rf_port_list [::skiq_rf_port_t] pointer list of TRX RF ports
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_rf_ports_avail_for_hdl( uint8_t card,
                                                      skiq_rx_hdl_t hdl,
                                                      uint8_t *p_num_fixed_rf_ports,
                                                      skiq_rf_port_t *p_fixed_rf_port_list,
                                                      uint8_t *p_num_trx_rf_ports,
                                                      skiq_rf_port_t *p_trx_rf_port_list );

/*****************************************************************************/
/** The skiq_read_rx_rf_port_for_hdl() function reads the current RF port
    configured for the RX handle specified.

    @since Function added in API @b v4.5.0

    @ingroup rfportfunctions
    @see skiq_read_rx_rf_ports_avail_for_hdl
    @see skiq_write_rx_rf_port_for_hdl

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl RX handle of interest
    @param[out] p_rf_port [::skiq_rf_port_t] pointer to the current RF port
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_rx_rf_port_for_hdl( uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               skiq_rf_port_t *p_rf_port );

/*****************************************************************************/
/** The skiq_write_rx_rf_port_for_hdl() function configures the RF port for use
    with the RX handle.

    @since Function added in API @b v4.5.0

    @ingroup rfportfunctions
    @see skiq_read_rx_rf_ports_avail_for_hdl
    @see skiq_read_rx_rf_port_for_hdl

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl RX handle of interest
    @param[out] rf_port [::skiq_rf_port_t] RF port to use for hdl
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_rx_rf_port_for_hdl( uint8_t card,
                                                skiq_rx_hdl_t hdl,
                                                skiq_rf_port_t rf_port );

/*****************************************************************************/
/** The skiq_read_tx_rf_ports_avail_for_hdl() function reads a list of RF ports
    supported for the specified TX handle.

    @since Function added in API @b v4.5.0

    @note The fixed port list is only available for use when the RF port
    configuration is set to @ref skiq_rf_port_config_fixed.  The TRx port list is only
    available for use when the RF port configuration is set to @ref skiq_rf_port_config_trx.

    @note p_num_fixed_rf_port_list and p_trx_rf_port_list must contain at least
    skiq_rf_port_max number of elements.

    @ingroup rfportfunctions
    @see skiq_read_tx_rf_port_for_hdl
    @see skiq_write_tx_rf_port_for_hdl

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl TX handle of interest
    @param[out] p_num_fixed_rf_ports pointer to the number of ports available
    @param[out] p_fixed_rf_port_list [::skiq_rf_port_t] pointer list of fixed RF ports
    @param[out] p_num_trx_rf_ports pointer to the number of TRX RF ports available
    @param[out] p_trx_rf_port_list [::skiq_rf_port_t] pointer list of TRX RF ports
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_rf_ports_avail_for_hdl( uint8_t card,
                                                      skiq_tx_hdl_t hdl,
                                                      uint8_t *p_num_fixed_rf_ports,
                                                      skiq_rf_port_t *p_fixed_rf_port_list,
                                                      uint8_t *p_num_trx_rf_ports,
                                                      skiq_rf_port_t *p_trx_rf_port_list );


/*****************************************************************************/
/** The skiq_read_tx_rf_port_for_hdl() function reads the current RF port
    configured for the TX handle specified.

    @since Function added in API @b v4.5.0

    @ingroup rfportfunctions
    @see skiq_read_tx_rf_ports_avail_for_hdl
    @see skiq_write_tx_rf_port_for_hdl

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl TX handle of interest
    @param[out] p_rf_port [::skiq_rf_port_t] pointer to the current RF port
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_rf_port_for_hdl( uint8_t card,
                                               skiq_tx_hdl_t hdl,
                                               skiq_rf_port_t *p_rf_port );

/*****************************************************************************/
/** The skiq_write_tx_rf_port_for_hdl() function configures the RF port for use
    with the TX handle.

    @since Function added in API @b v4.5.0

    @ingroup rfportfunctions
    @see skiq_read_tx_rf_ports_avail_for_hdl
    @see skiq_read_tx_rf_port_for_hdl

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl TX handle of interest
    @param[out] rf_port [::skiq_rf_port_t] RF port to use for hdl
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_rf_port_for_hdl( uint8_t card,
                                                skiq_tx_hdl_t hdl,
                                                skiq_rf_port_t rf_port );

/*****************************************************************************/
/** The skiq_read_rx_cal_offset() function provides a receive calibration offset
    based on the current settings of the receive handle.  This function may not
    be used if the gain mode for the handle is set to @ref skiq_rx_gain_auto and
    will return an error.

    @since Function added in API @b v4.0.0

    @ingroup calfunctions
    @see skiq_read_rx_cal_offset_by_LO_freq
    @see skiq_read_rx_cal_offset_by_gain_index
    @see skiq_read_rx_cal_offset_by_LO_freq_and_gain_index
    @see skiq_read_rx_cal_data_present
    @see skiq_read_rx_cal_data_present_for_port

    @param card card index of the Sidekiq of interest
    @param hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_cal_off_dB reference to container for calibration offset in dB

    @return int32_t status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_read_rx_cal_offset( uint8_t card,
                                          skiq_rx_hdl_t hdl,
                                          double *p_cal_off_dB );

/*****************************************************************************/
/** The skiq_read_rx_cal_offset_by_LO_freq() function provides a receive
    calibration offset given an LO frequency and based on the present gain index
    of the receive handle.  This function may not be used if the gain mode for
    the handle is set to @ref skiq_rx_gain_auto and will return an error.

    @since Function added in API @b v4.0.0

    @ingroup calfunctions
    @see skiq_read_rx_cal_offset
    @see skiq_read_rx_cal_offset_by_gain_index
    @see skiq_read_rx_cal_offset_by_LO_freq_and_gain_index
    @see skiq_read_rx_cal_data_present
    @see skiq_read_rx_cal_data_present_for_port

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] lo_freq LO frequency in Hertz
    @param[out] p_cal_off_dB reference to container for calibration offset in dB

    @return int32_t status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_read_rx_cal_offset_by_LO_freq( uint8_t card,
                                                     skiq_rx_hdl_t hdl,
                                                     uint64_t lo_freq,
                                                     double *p_cal_off_dB );

/*****************************************************************************/
/** The skiq_read_rx_cal_offset_by_gain_index() function provides a receive
    calibration offset given a receive gain index and based on the present LO
    frequency of the receive handle.  This function is useful when the gain mode
    for the handle is set to @ref skiq_rx_gain_auto and the caller feeds in the
    gain index from the @ref skiq_rx_block_t::rfic_control "receive packet's
    metadata".

    @since Function added in API @b v4.0.0

    @ingroup calfunctions
    @see skiq_read_rx_cal_offset
    @see skiq_read_rx_cal_offset_by_LO_freq
    @see skiq_read_rx_cal_offset_by_LO_freq_and_gain_index
    @see skiq_read_rx_cal_data_present
    @see skiq_read_rx_cal_data_present_for_port

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] gain_index gain index as set in the RFIC
    @param[out] p_cal_off_dB reference to container for calibration offset in dB

    @return int32_t status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_read_rx_cal_offset_by_gain_index( uint8_t card,
                                                        skiq_rx_hdl_t hdl,
                                                        uint8_t gain_index,
                                                        double *p_cal_off_dB );

/*****************************************************************************/
/** The skiq_read_rx_cal_offset_by_LO_freq_and_gain_index() function provides a
    receive calibration offset given an LO frequency and receive gain index and
    based on the present RX FIR filter gain of the receive handle.  This
    function is useful when the gain mode for the handle is set to @ref
    skiq_rx_gain_auto and the caller feeds in the gain index from the @ref
    skiq_rx_block_t::rfic_control "receive packet's metadata" and when the radio
    is not presently tuned to the frequency of interest.

    @since Function added in API @b v4.0.0

    @ingroup calfunctions
    @see skiq_read_rx_cal_offset
    @see skiq_read_rx_cal_offset_by_LO_freq
    @see skiq_read_rx_cal_offset_by_gain_index
    @see skiq_read_rx_cal_data_present
    @see skiq_read_rx_cal_data_present_for_port

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] lo_freq LO frequency in Hertz
    @param[in] gain_index gain index as set in the RFIC
    @param[out] p_cal_off_dB reference to container for calibration offset in dB

    @return int32_t status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_read_rx_cal_offset_by_LO_freq_and_gain_index( uint8_t card,
                                                                    skiq_rx_hdl_t hdl,
                                                                    uint64_t lo_freq,
                                                                    uint8_t gain_index,
                                                                    double *p_cal_off_dB );

/*****************************************************************************/
/** The skiq_read_rx_cal_data_present() function provides an indication for
    whether or not receiver calibration data is present for a specified card and
    handle.  If the receiver calibration data is not present, the default
    calibration (if supported / available) in calibration offset queries.

    @since Function added in API @b v4.4.0

    @ingroup calfunctions
    @see skiq_read_rx_cal_offset
    @see skiq_read_rx_cal_offset_by_LO_freq
    @see skiq_read_rx_cal_offset_by_gain_index
    @see skiq_read_rx_cal_offset_by_LO_freq_and_gain_index
    @see skiq_read_rx_cal_data_present_for_port

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_present reference to a boolean value indicating data presence

    @return int32_t status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_read_rx_cal_data_present( uint8_t card,
                                                skiq_rx_hdl_t hdl,
                                                bool *p_present );

/*****************************************************************************/
/** The skiq_read_rx_cal_data_present_for_port() function provides an indication
    for whether or not receive calibration data is present for a specified card,
    handle, and RF port.  If the receive calibration data is not present, the
    default calibration (if supported / available) is used in
    skiq_read_rx_cal_offset(), skiq_read_rx_cal_offset_by_LO_freq(),
    skiq_read_rx_cal_offset_by_gain_index(), and
    skiq_read_rx_cal_offset_by_LO_freq_and_gain_index().

    @since Function added in API @b v4.5.0

    @ingroup calfunctions
    @see skiq_read_rx_cal_offset
    @see skiq_read_rx_cal_offset_by_LO_freq
    @see skiq_read_rx_cal_offset_by_gain_index
    @see skiq_read_rx_cal_offset_by_LO_freq_and_gain_index
    @see skiq_read_rx_cal_data_present

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] port [::skiq_rf_port_t] RF port of interest
    @param[out] p_present reference to a boolean value indicating data presence

    @return int32_t status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_read_rx_cal_data_present_for_port( uint8_t card,
                                                         skiq_rx_hdl_t hdl,
                                                         skiq_rf_port_t port,
                                                         bool *p_present );

/*****************************************************************************/
/** The skiq_read_last_tx_timestamp() function queries the FPGA to determine
    what transmit timestamp it last encountered.  The last transmit timestamp has two
    interpretations.  Firstly, if the current RF timestamp is greater than the
    timestamp returned by this function, then the FPGA has already transmitted
    the block.  Secondly, if the current RF timestamp is less than the timestamp
    returned by this function, then the FPGA is holding the transmit block and
    waiting until the RF timestamp matches the block's transmit timestamp.

    @warning The last transmit timestamp is only representative if the transmit
    flow mode is @ref skiq_tx_with_timestamps_data_flow_mode.

    @since Function added in API @b v4.0.0, requires FPGA @b v3.5 or later

    @ingroup fpgafunctions
    @see skiq_read_tx_data_flow_mode
    @see skiq_write_tx_data_flow_mode

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] transmit handle of interest
    @param[out] p_last_timestamp pointer to 64-bit timestamp value, will be zero if not transmitting
    @return int32_t status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_last_tx_timestamp( uint8_t card,
                                              skiq_tx_hdl_t hdl,
                                              uint64_t *p_last_timestamp );


/*****************************************************************************/
/** The skiq_read_usb_enumeration_delay() function reads the number of
    milliseconds that the Sidekiq should delay USB enumeration, if supported.

    @warning This function will return an error if called on a unit that does
    not have an FX2 placed on it.

    @since Function added in API @b v4.2.0, requires firmware @b v2.7 or later

    @ingroup cardfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_delay_ms pointer to take total enumeration delay in milliseconds
    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_usb_enumeration_delay( uint8_t card,
                                                  uint16_t *p_delay_ms );

/*****************************************************************************/
/** The skiq_read_sys_timestamp_freq() function reads the system timestamp frequency (in Hz).  This
    API replaces usage of ::SKIQ_SYS_TIMESTAMP_FREQ.  This frequency represents the frequency at
    which the System Timestamp increments.

    @attention On the Sidekiq X2 platform, this frequency value may change when the receive or
    transmit sample rate changes.

    @since Function added in API @b v4.2.0

    @ingroup timestampfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_sys_timestamp_freq pointer to where to store the system timestamp frequency
    @return int32_t  status where 0=success, anything else is an error

    @retval 0 successful query of the system timestamp frequency
    @retval -EINVAL specified card index is out of range
    @retval -EINVAL reference to p_sys_timestamp_freq is NULL
    @retval -ENODEV specified card index has not been initialized
*/
EPIQ_API int32_t skiq_read_sys_timestamp_freq( uint8_t card,
                                               uint64_t *p_sys_timestamp_freq );

/*************************************************************************************************/
/** The skiq_read_rx_block_size returns the expected RX block size (in bytes) for a specified
    ::skiq_rx_stream_mode_t.

    @since Function added in API @b v4.6.0

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] stream_mode [::skiq_rx_stream_mode_t] RX stream mode associated with RX block size

    @return int32_t
    @retval >0 expected block size (in bytes) for the specified RX stream mode
    @retval -1 specified card index is out of range or has not been initialized
    @retval -ENOTSUP specified RX stream mode is not supported for the loaded FPGA bitstream
    @retval -EINVAL specified RX stream mode is not a valid mode, see ::skiq_rx_stream_mode_t for valid modes
*/
EPIQ_API int32_t skiq_read_rx_block_size( uint8_t card,
                                          skiq_rx_stream_mode_t stream_mode );

/*****************************************************************************/
/** The skiq_read_tx_quadcal_mode() function reads the TX quadrature calibration
    algorithm mode.

    @since Function added in API @b v4.6.0

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] transmit handle of interest
    @param[out] p_mode [::skiq_tx_quadcal_mode_t] the currently set value of the
    TX quadrature calibration mode setting

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_read_tx_quadcal_mode( uint8_t card,
                                            skiq_tx_hdl_t hdl,
                                            skiq_tx_quadcal_mode_t *p_mode );

/*****************************************************************************/
/** The skiq_write_tx_quadcal_mode() function writes the TX quadrature calibration
    algorithm mode.  If automatic mode is configured, writing the TX LO frequency
    may result in the TX quadrature calibration algorithm to be run, resulting in
    the transmission of calibration waveforms which can take a significant amount
    of time to complete.  If manual mode is configured, it is the user's
    responsibility to determine when to run the TX quadrature
    calibration algorithm via ::skiq_run_tx_quadcal().

    @since Function added in API @b v4.6.0

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] transmit handle of interest
    @param[in] mode [::skiq_tx_quadcal_mode_t] TX quadrature calibration mode
    to configure

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_write_tx_quadcal_mode( uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             skiq_tx_quadcal_mode_t mode );

/*****************************************************************************/
/** The skiq_run_tx_quadcal() performs the TX quadrature calibration
    algorithm based on the current RFIC settings.

    @note This quadrature calibration may take some time to complete.  Additionally, running of the
    TX quadrature algorithm results in transmissions of calibration waveforms,
    resulting in the appearance of erroneous transmissions in the spectrum during
    execution of the algorithm. Streaming RX or TX while running the TX quadrature algorithm will
    result in a momentary gap in received and/or transmitted samples.  It is recommended
    that this is ran after the desired Tx LO frequency has been configured.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @attention In the case of Sidekiq X2, calibration is performed on all TX handles,
    regardless of the handle specified.

    @since Function added in API @b v4.6.0

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] transmit handle of interest

    @return int32_t  status where 0=success, anything else is an error
*/
EPIQ_API int32_t skiq_run_tx_quadcal( uint8_t card, skiq_tx_hdl_t hdl );

/*****************************************************************************/
/** The skiq_read_rx_cal_mode() function reads the RX calibration mode.

    @since Function added in API @b v4.13.0

    @ingroup rxfunctions
    @see skiq_write_rx_cal_mode
    @see skiq_run_rx_cal
    @see skiq_read_rx_cal_type_mask
    @see skiq_write_rx_cal_type_mask
    @see skiq_read_rx_cal_types_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_mode [::skiq_rx_cal_mode_t] the currently set value of the
    RX calibration mode setting

    @retval 0 Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EFAULT  NULL pointer detected for @p p_mode
*/
EPIQ_API int32_t skiq_read_rx_cal_mode( uint8_t card,
                                        skiq_rx_hdl_t hdl,
                                        skiq_rx_cal_mode_t *p_mode );

/*****************************************************************************/
/** The skiq_write_rx_cal_mode() function writes the RX calibration mode.  If automatic mode is
    configured, writing the RX LO frequency may result in the RX calibrations to be performed prior
    to completing the tune operation.  The types of calibrations performed are controlled by the
    [::skiq_rx_cal_type_t] configuration.  If manual mode is configured, it is the user's
    responsibility to determine when to run the RX calibration via skiq_run_rx_cal().

    @since Function added in API @b v4.13.0

    @ingroup rxfunctions
    @see skiq_read_rx_cal_mode
    @see skiq_run_rx_cal
    @see skiq_read_rx_cal_type_mask
    @see skiq_write_rx_cal_type_mask
    @see skiq_read_rx_cal_types_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] mode [::skiq_rx_cal_mode_t] RX calibration mode to configure

    @retval 0 Success
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_write_rx_cal_mode( uint8_t card,
                                         skiq_rx_hdl_t hdl,
                                         skiq_rx_cal_mode_t mode );

/*****************************************************************************/
/** The skiq_run_rx_cal() performs the RX calibration based on the current RFIC
    settings and RX calibrations enabled.

    @note that this may take some time to complete, depending on the calibration
    types enabled, RF environment, the Sidekiq product (<100 ms to >1 second).

    @note streaming RX or TX while running the RX calibration will result in a 
    momentary gap in received and/or transmitted samples.  It is recommended
    that the function is ran after the desired RX LO frequency has been configured.

    @attention In the case of Sidekiq X4, calibration is performed on all 
    enabled RX handles, regardless of the handle specified. 

    @since Function added in API @b v4.13.0

    @ingroup rxfunctions
    @see skiq_read_rx_cal_mode
    @see skiq_write_rx_cal_mode
    @see skiq_read_rx_cal_type_mask
    @see skiq_write_rx_cal_type_mask
    @see skiq_read_rx_cal_types_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest

    @retval 0 Success
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -ENODEV  Generic error accessing card
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_run_rx_cal( uint8_t card, skiq_rx_hdl_t hdl );


/*****************************************************************************/
/** The skiq_read_rx_cal_type_mask() function reads the RX calibration types configured.

    @since Function added in API @b v4.13.0

    @ingroup rxfunctions
    @see skiq_read_rx_cal_mode
    @see skiq_write_rx_cal_mode
    @see skiq_run_rx_cal
    @see skiq_write_rx_cal_type_mask
    @see skiq_read_rx_cal_types_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_cal_mask a bitmask of the currently enabled RX
    calibration types

    @retval 0 Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EFAULT  NULL pointer detected for @p p_cal_mask
*/
EPIQ_API int32_t skiq_read_rx_cal_type_mask( uint8_t card,
                                             skiq_rx_hdl_t hdl,
                                             uint32_t *p_cal_mask );

/*****************************************************************************/
/** The skiq_write_rx_cal_type_mask() function writes the RX calibration types to 
    use when calibration is ran either manuallly or automatically. 

    @since Function added in API @b v4.13.0

    @ingroup rxfunctions
    @see skiq_read_rx_cal_mode
    @see skiq_write_rx_cal_mode
    @see skiq_run_rx_cal
    @see skiq_read_rx_cal_type_mask
    @see skiq_read_rx_cal_types_avail

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] cal_mask bitmask of calibration types to perform.  This should be formed by ORing [::skiq_rx_cal_type_t] for each calibration type to enable.

    @return int32_t  status where 0=success, else a negative errno value
    @retval 0 Success
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EINVAL  Invalid mask specified for product
*/
EPIQ_API int32_t skiq_write_rx_cal_type_mask( uint8_t card,
                                              skiq_rx_hdl_t hdl,
                                              uint32_t cal_mask );

/*****************************************************************************/
/** The skiq_read_rx_cal_types_avail() function provides a bitmask of all of 
    the RX calibration types available.

    @since Function added in API @b v4.13.0

    @ingroup rxfunctions
    @see skiq_read_rx_cal_mode
    @see skiq_write_rx_cal_mode
    @see skiq_run_rx_cal
    @see skiq_read_rx_cal_type_mask
    @see skiq_write_rx_cal_type_mask

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_cal_mask pointer to a bitmask of the RX calibration types [::skiq_rx_cal_type_t] available

    @retval 0 Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EFAULT  NULL pointer detected for @p p_cal_mask
*/
EPIQ_API int32_t skiq_read_rx_cal_types_avail( uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               uint32_t *p_cal_mask );

/*************************************************************************************************/
/** The skiq_read_iq_complex_multiplier() function provides the complex multiplication factor that
    is currently in use for the supplied receive handle.

    @attention I/Q phase and amplitude multiplication factors are only supported on a subset of
    Sidekiq products and only if the FPGA is @b v3.10.0 or later

    @since Function added in API @b v4.7.0, requires FPGA @b v3.10.0 or later

    @ingroup calfunctions
    @see skiq_read_iq_cal_complex_multiplier
    @see skiq_read_iq_cal_complex_multiplier_by_LO_freq
    @see skiq_write_iq_complex_multiplier_absolute
    @see skiq_write_iq_complex_multiplier_user
    @see skiq_read_iq_complex_cal_data_present

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_factor [::float_complex_t *] reference to the complex multiplication factor

    @return int32_t status where 0=success, anything else is an error
    @retval 0 Success
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -ENOSYS  Sidekiq platform is not running an FPGA that meets the minimum interface version requirements
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EINVAL  An invalid / unsupported receive handle was specified

 */
EPIQ_API int32_t skiq_read_iq_complex_multiplier( uint8_t card,
                                                  skiq_rx_hdl_t hdl,
                                                  float_complex_t *p_factor );


/*************************************************************************************************/
/** The skiq_write_iq_complex_multiplier_absolute() function overwrites the complex multiplication
    factor that is currently in use for the supplied receive handle.

    @attention I/Q phase and amplitude multiplication factors are only supported on a subset of
    Sidekiq products and only if the FPGA is @b v3.10.0 or later

    @since Function added in API @b v4.7.0, requires FPGA @b v3.10.0 or later

    @ingroup calfunctions
    @see skiq_read_iq_cal_complex_multiplier
    @see skiq_read_iq_cal_complex_multiplier_by_LO_freq
    @see skiq_read_iq_complex_multiplier
    @see skiq_write_iq_complex_multiplier_user
    @see skiq_read_iq_complex_cal_data_present

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] factor [::float_complex_t] complex multiplication factor to overwrite factory calibrated settings

    @return int32_t status where 0=success, anything else is an error
    @retval 0 Success
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -ENOSYS  Sidekiq platform is not running an FPGA that meets the minimum interface version requirements
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform

 */
EPIQ_API int32_t skiq_write_iq_complex_multiplier_absolute( uint8_t card,
                                                            skiq_rx_hdl_t hdl,
                                                            float_complex_t factor );


/*************************************************************************************************/
/** The skiq_write_iq_complex_multiplier_user() function further applies an I/Q phase and amplitude
    correction to the factory specified calibration factors.  This function may be useful to users
    that have a two or four antenna configuration that they wish to "zero" out by applying an
    additional correction factor.

    @attention I/Q phase and amplitude multiplication factors are only supported on a subset of
    Sidekiq products and only if the FPGA is @b v3.10.0 or later

    <pre>
    i'[n] + j*q'[n] = (i[n] + j*q[n])*(re_cal + j*im_cal)*(re_user + j*im_user)
    </pre>

    @since Function added in API @b v4.7.0, requires FPGA @b v3.10.0 or later

    @ingroup calfunctions
    @see skiq_read_iq_cal_complex_multiplier
    @see skiq_read_iq_cal_complex_multiplier_by_LO_freq
    @see skiq_read_iq_complex_multiplier
    @see skiq_write_iq_complex_multiplier_absolute
    @see skiq_read_iq_complex_cal_data_present

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] factor [::float_complex_t] complex multiplication factor to apply in addition to factory calibrated settings

    @return int32_t status where 0=success, anything else is an error
    @retval 0 Success
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
    @retval -ENOSYS  Sidekiq platform is not running an FPGA that meets the minimum interface version requirements
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform

 */
EPIQ_API int32_t skiq_write_iq_complex_multiplier_user( uint8_t card,
                                                        skiq_rx_hdl_t hdl,
                                                        float_complex_t factor );


/*************************************************************************************************/
/** The skiq_read_iq_cal_complex_multiplier() function provides the complex multiplication factor
    based on the current settings of the receive handle as determined by factory settings.

    @warning The factors returned by this function may not represent the current factors in use
    whenever they are overwritten by skiq_write_iq_complex_multiplier_absolute() or
    skiq_write_iq_complex_multiplier_user().  Use the skiq_read_iq_complex_multiplier() instead
    to query the current factors.

    @attention IQ phase and amplitude calibration may be present but it is only active if the FPGA
    is @b v3.10.0 or later.

    @since Function added in API @b v4.7.0

    @ingroup calfunctions
    @see skiq_read_iq_cal_complex_multiplier_by_LO_freq
    @see skiq_read_iq_complex_multiplier
    @see skiq_write_iq_complex_multiplier_absolute
    @see skiq_write_iq_complex_multiplier_user
    @see skiq_read_iq_complex_cal_data_present

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_factor [::float_complex_t *] reference to the complex multiplication factor

    @return int32_t status where 0=success, anything else is an error
    @retval 0        Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform

 */
EPIQ_API int32_t skiq_read_iq_cal_complex_multiplier( uint8_t card,
                                                      skiq_rx_hdl_t hdl,
                                                      float_complex_t *p_factor );


/*************************************************************************************************/
/** The skiq_read_iq_cal_complex_multiplier_by_LO_freq() function provides the complex
    multiplication factor at given a receive LO frequency for the receive handle as determined by
    factory settings.

    @warning The factor returned by this function may not represent the current factor in use.  They
    may have been overwritten by skiq_write_iq_complex_multiplier_absolute() or
    skiq_write_iq_complex_multiplier_user().  Use the skiq_read_iq_complex_multiplier() instead to
    query the factor that is currently in use.

    @attention IQ phase and amplitude calibration data may be present but is only active if the FPGA
    is @b v3.10.0 or later.

    @since Function added in API @b v4.7.0

    @ingroup calfunctions
    @see skiq_read_iq_cal_complex_multiplier
    @see skiq_read_iq_complex_multiplier
    @see skiq_write_iq_complex_multiplier_absolute
    @see skiq_write_iq_complex_multiplier_user
    @see skiq_read_iq_complex_cal_data_present

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] lo_freq receive LO frequency of interest
    @param[out] p_factor [::float_complex_t *] reference to the complex multiplication factor

    @return int32_t status where 0=success, anything else is an error
    @retval 0        Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform

 */
EPIQ_API int32_t skiq_read_iq_cal_complex_multiplier_by_LO_freq( uint8_t card,
                                                                 skiq_rx_hdl_t hdl,
                                                                 uint64_t lo_freq,
                                                                 float_complex_t *p_factor );


/*************************************************************************************************/
/** The skiq_read_iq_complex_cal_data_present() function provides an indication for whether or not
    I/Q phase and amplitude calibration data is present for a specified card and handle.

    @warning If the calibration data is not present, there is no default calibration.  As such,
    there will be no IQ phase and amplitude correction.

    @attention I/Q phase and amplitude multiplication factors are only supported on a subset of
    Sidekiq products and only if the FPGA is @b v3.10.0 or later

    @since Function added in API @b v4.7.0

    @ingroup calfunctions
    @see skiq_read_iq_cal_complex_multiplier
    @see skiq_read_iq_cal_complex_multiplier_by_LO_freq
    @see skiq_read_iq_complex_multiplier
    @see skiq_write_iq_complex_multiplier_absolute
    @see skiq_write_iq_complex_multiplier_user

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_present reference to a boolean value indicating data presence

    @return int32_t status where 0=success, anything else is an error
    @retval 0        Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform

 */
EPIQ_API int32_t skiq_read_iq_complex_cal_data_present( uint8_t card,
                                                        skiq_rx_hdl_t hdl,
                                                        bool *p_present );


/*************************************************************************************************/
/** The skiq_read_1pps_source() function reads the currently configured source of the 1PPS signal.

    @since Function added in API @b v4.7.0

    @ingroup ppsfunctions
    @see skiq_write_1pps_source

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_pps_source [::skiq_1pps_source_t] pointer to 1pps source

    @note p_pps_source updated only upon success

    @return int32_t
    @retval 0 Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EBADMSG Error occurred transacting with FPGA registers
    @retval -ESRCH Internal error, Sidekiq part misidentified or invalid
*/
EPIQ_API int32_t skiq_read_1pps_source( uint8_t card,
                                        skiq_1pps_source_t *p_pps_source );


/*************************************************************************************************/
/** The skiq_write_1pps_source() function configures the source of the 1PPS signal.

    @note Refer to the hardware user's manual for physical location of signal

    @warning Not all sources are available with all Sidekiq products

    @attention Supported sources may depend on FPGA bitstream

    @since Function added in API @b v4.7.0

    @ingroup ppsfunctions
    @see skiq_read_1pps_source

    @param[in] card card index of the Sidekiq of interest
    @param[in] pps_source [::skiq_1pps_source_t] source of 1PPS signal

    @return int32_t
    @retval 0 Success
    @retval -ERANGE  Requested card index is out of range
    @retval -ENODEV  Requested card index is not initialized
    @retval -EBADMSG Error occurred transacting with FPGA registers
    @retval -ENOSYS FPGA bitstream does not support specified 1PPS source
    @retval -ENOTSUP Sidekiq product does not specified 1PPS source
    @retval -EINVAL Invalid 1PPS source specified
*/
EPIQ_API int32_t skiq_write_1pps_source( uint8_t card,
                                         skiq_1pps_source_t pps_source );


/*****************************************************************************/
/** The skiq_read_calibration_date() function reads details on when calibration
    was last performed.  Additionally, a recommended date to perform the next
    calibration is provided.

    @since Function added in API @b v4.7.0

    @ingroup cardfunctions
    @see skiq_read_part_info
    @see skiq_read_parameters

    @param[in] card card index of the Sidekiq of interest
    @param[out] p_last_cal_year pointer to where to store the year when calibration
                was last performed.
    @param[out] p_last_cal_week pointer to where to store the week number
                when the calibration was last performed.  The week number with
                the calibration year provides a full representation of when the calibration
                was performed.
    @param[out] p_cal_interval pointer to where to store the interval (in years) of how
                often calibration should be performed.  The year of the last calibration
                (adjusted by this interval) along with the week of the last calibration
                provides a recommendation for when the next calibration should be performed.

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -ENOENT Calibration date information cannot be located
*/
EPIQ_API int32_t skiq_read_calibration_date( uint8_t card,
                                             uint16_t *p_last_cal_year,
                                             uint8_t *p_last_cal_week,
                                             uint8_t *p_cal_interval );


/*****************************************************************************/
/** The skiq_write_rx_freq_tune_mode() function configures the frequency tune 
    mode for the handle specified.

    @since Function added in API @b v4.10.0

    @note For Sidekiq X4, this configures the tune mode for both receive and transmit
    of the RFIC specified by the RX handle (ex. RX A1/A2/C1 configures RFIC A)

    @note For Sidekiq X2, skiq_freq_tune_mode_hop_on_timestamp is not supported.  
    Additionally, skiq_rx_hdl_B1 is not supported.

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] mode [::skiq_freq_tune_mode_t] tune mode

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -ENOTSUP Mode is not supported by hardware
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_write_rx_freq_tune_mode( uint8_t card,
                                               skiq_rx_hdl_t hdl,
                                               skiq_freq_tune_mode_t mode );

/*****************************************************************************/
/** The skiq_read_rx_freq_tune_mode() function reads the configured frequency 
    tune mode for the handle specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_mode [::skiq_freq_tune_mode_t] pointer to tune mode

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_rx_freq_tune_mode( uint8_t card,
                                              skiq_rx_hdl_t hdl,
                                              skiq_freq_tune_mode_t *p_mode );

/*****************************************************************************/
/** The skiq_write_tx_freq_tune_mode() function configures the frequency tune 
    mode for the handle specified.

    @since Function added in API @b v4.10.0

    @note For Sidekiq X4, this configures the tune mode for both receive and transmit
    of the RFIC specified by the TX handle (ex. TX A1/A2 configures RFIC A)

    @note For Sidekiq X2, skiq_freq_tune_mode_hop_on_timestamp is not supported.  

    @attention See @ref AD9361TimestampSlip for details on how calling this function
    can affect the RF timestamp metadata associated with received I/Q blocks.

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] transmit handle of interest
    @param[in] mode [::skiq_freq_tune_mode_t] tune mode

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -ENOTSUP Mode is not supported by hardware
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_write_tx_freq_tune_mode( uint8_t card,
                                               skiq_tx_hdl_t hdl,
                                               skiq_freq_tune_mode_t mode );

/*****************************************************************************/
/** The skiq_read_tx_freq_tune_mode() function reads the configured frequency 
    tune mode for the handle specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] receive handle of interest
    @param[out] p_mode [::skiq_freq_tune_mode_t] pointer to tune mode

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform    
*/
EPIQ_API int32_t skiq_read_tx_freq_tune_mode( uint8_t card,
                                              skiq_tx_hdl_t hdl,
                                              skiq_freq_tune_mode_t *p_mode );


/*****************************************************************************/
/** The skiq_write_rx_freq_hop_list() function configures the frequency hop 
    list to the values specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] num_freq number of frequencies included in freq_list;
               this value cannot exceed ::SKIQ_MAX_NUM_FREQ_HOPS
    @param[in] freq_list list of frequencies supported in hopping list
    @param[in] initial_index initial index of frequency for first hop

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range or # freqs out of range or initial index out of range 
    @retval -ERANGE Number of frequencies is not less than SKIQ_MAX_NUM_FREQ_HOPS
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EINVAL freq_list contains invalid frequency
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_write_rx_freq_hop_list( uint8_t card,
                                              skiq_rx_hdl_t hdl,
                                              uint16_t num_freq,
                                              uint64_t freq_list[],
                                              uint16_t initial_index );

/*****************************************************************************/
/** The skiq_read_rx_freq_hop_list() function populates the frequency hop list 
    with the frequency values previously specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_num_freq pointer to number of frequencies included in list
    @param[out] freq_list hopping list currently configured; this list should 
                be able to hold at least ::SKIQ_MAX_NUM_FREQ_HOPS

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_rx_freq_hop_list( uint8_t card,
                                             skiq_rx_hdl_t hdl,
                                             uint16_t *p_num_freq,
                                             uint64_t freq_list[SKIQ_MAX_NUM_FREQ_HOPS] );

/*****************************************************************************/
/** The skiq_write_tx_freq_hop_list() function configures the frequency hop list 
    to the values specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] receive handle of interest
    @param[in] num_freq number of frequencies included in freq_list
               this value cannot exceed ::SKIQ_MAX_NUM_FREQ_HOPS
    @param[in] freq_list list of frequencies supported in hopping list
    @param[in] initial_index initial index of frequency for first hop

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range or # freqs out of range or initial index out of range 
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval -EINVAL freq_list contains invalid frequency
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_write_tx_freq_hop_list( uint8_t card,
                                              skiq_tx_hdl_t hdl,
                                              uint16_t num_freq,
                                              uint64_t freq_list[],
                                              uint16_t initial_index );

/*****************************************************************************/
/** The skiq_read_tx_freq_hop_list() function populates the frequency hop list 
    with the values previously specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] receive handle of interest
    @param[out] p_num_freq pointer to number of frequencies included in list
    @param[out] freq_list hopping list currently configured; this list should 
                be able to hold at least ::SKIQ_MAX_NUM_FREQ_HOPS

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_tx_freq_hop_list( uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             uint16_t *p_num_freq,
                                             uint64_t freq_list[SKIQ_MAX_NUM_FREQ_HOPS] );

/*****************************************************************************/
/** The skiq_write_next_rx_freq_hop() function performs the various 
    configuration required to support the next frequency hop but does not 
    execute the hop until skiq_perform_rx_freq_hop() is called.

    @since Function added in API @b v4.10.0

    @note For Sidekiq X4, this updates both the RX and TX LO frequency.
    @note For any radio based on the AD9361 RF IC (mPCIe, m.2, Z2), when operating
    in the ::skiq_freq_tune_mode_hop_on_timestamp, this updates both the RX and TX LO frequency
    based on the index specified.

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] freq_index index into hopping list of frequency to configure

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range or freq index out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM   Requested handle is not available or out of range for the Sidekiq platform
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_write_next_rx_freq_hop( uint8_t card,
                                              skiq_rx_hdl_t hdl,
                                              uint16_t freq_index );

/*****************************************************************************/
/** The skiq_write_next_tx_freq_hop() function performs the various 
    configuration required to support the next frequency hop but does not 
    execute the hop until skiq_perform_tx_freq_hop() is called.

    @since Function added in API @b v4.10.0

    @note For Sidekiq X4, this updates both the RX and TX LO frequency.
    @note For any radio based on the AD9361 RF IC (mPCIe, m.2, Z2), when operating
    in the ::skiq_freq_tune_mode_hop_on_timestamp, this updates both the RX and TX LO frequency
    based on the index specified.

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] transmit handle of interest
    @param[in] freq_index index into hopping list of frequency to configure

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range or freq index out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
    @retval non-zero Unspecified error occurred
*/
EPIQ_API int32_t skiq_write_next_tx_freq_hop( uint8_t card,
                                              skiq_tx_hdl_t hdl,
                                              uint16_t freq_index );

/*****************************************************************************/
/** The skiq_perform_rx_freq_hop() function performs the frequency hop for the 
    handle specified.

    @since Function added in API @b v4.10.0

    @note For Sidekiq X4, this updates both the RX and TX LO frequency.
    @note For any radio based on the AD9361 RF IC (mPCIe, m.2, Z2), when operating
    in the ::skiq_freq_tune_mode_hop_on_timestamp, this updates both the RX and TX LO frequency
    based on the index specified.
    @note if operating in ::skiq_freq_tune_mode_hop_on_timestamp and a rf_timestamp that has
    already passed is specified, the frequency hop will be executed immediately.  If
    running in ::skiq_freq_tune_mode_hop_immediate, the timestamp parameter is ignored.

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[in] rf_timestamp timestamp to execute the hop (only for ::skiq_freq_tune_mode_hop_on_timestamp)

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_perform_rx_freq_hop( uint8_t card,
                                           skiq_rx_hdl_t hdl,
                                           uint64_t rf_timestamp );

/*****************************************************************************/
/** The skiq_perform_tx_freq_hop() function performs the frequency hop for the 
    handle specified.

    @since Function added in API @b v4.10.0

    @note For Sidekiq X4, this updates both the RX and TX LO frequency.
    @note For any radio based on the AD9361 RF IC (mPCIe, m.2, Z2), when operating
    in the ::skiq_freq_tune_mode_hop_on_timestamp, this updates both the RX and TX LO frequency
    based on the index specified.
    @note if operating in ::skiq_freq_tune_mode_hop_on_timestamp and a rf_timestamp that has
    already passed is specified, the frequency hop will be executed immediately.  If
    running in ::skiq_freq_tune_mode_hop_immediate, the timestamp parameter is ignored.

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] receive handle of interest
    @param[in] rf_timestamp timestamp to execute the hop (only for ::skiq_freq_tune_mode_hop_on_timestamp)

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized or an error occurred while applying hopping
                    config to RF IC
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_perform_tx_freq_hop( uint8_t card,
                                           skiq_tx_hdl_t hdl,
                                           uint64_t rf_timestamp );

/**************************************************************************************************/
/** The skiq_read_curr_rx_freq_hop() function reads the current frequency hopping configuration for
    the handle specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_hop_index pointer to the current hopping index
    @param[out] p_curr_freq pointer to the current frequency

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EINVAL Invalid pointers provided
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM   Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_curr_rx_freq_hop( uint8_t card,
                                             skiq_rx_hdl_t hdl,
                                             uint16_t *p_hop_index,
                                             uint64_t *p_curr_freq );

/*****************************************************************************/
/** The skiq_read_next_rx_freq_hop() function reads the next frequency hopping 
    configuration for the handle specified.  This is the configuration that 
    will be applied the next "perform hop" function is called.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t] receive handle of interest
    @param[out] p_hop_index pointer to the current hopping index
    @param[out] p_curr_freq pointer to the current frequency

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EINVAL Invalid pointers provided
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_next_rx_freq_hop( uint8_t card,
                                             skiq_rx_hdl_t hdl,
                                             uint16_t *p_hop_index,
                                             uint64_t *p_curr_freq );

/*****************************************************************************/
/** The skiq_read_curr_tx_freq_hop() function reads the current frequency 
    hopping configuration for the handle specified.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] receive handle of interest
    @param[out] p_hop_index pointer to the current hopping index
    @param[out] p_curr_freq pointer to the current frequency

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EINVAL Invalid pointers provided
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_curr_tx_freq_hop( uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             uint16_t *p_hop_index,
                                             uint64_t *p_curr_freq );

/*****************************************************************************/
/** The skiq_read_next_tx_freq_hop() function reads the next frequency hopping 
    configuration for the handle specified.  This is the configuration that 
    will be applied the next "perform hop" function is called.

    @since Function added in API @b v4.10.0

    @ingroup hopfunctions
    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t] receive handle of interest
    @param[out] p_hop_index pointer to the current hopping index
    @param[out] p_curr_freq pointer to the current frequency

    @return int32_t
    @retval 0 successful
    @retval -ERANGE Requested card index is out of range
    @retval -ENODEV Requested card index is not initialized
    @retval -EINVAL Invalid pointers provided
    @retval -EPROTO Tune mode is not hopping
    @retval -EDOM    Requested handle is not available or out of range for the Sidekiq platform
*/
EPIQ_API int32_t skiq_read_next_tx_freq_hop( uint8_t card,
                                             skiq_tx_hdl_t hdl,
                                             uint16_t *p_hop_index,
                                             uint64_t *p_curr_freq );

/**************************************************************************************************/
/**
   @brief This function is responsible for programming the FPGA from an image stored in flash at the
   specified slot.

   @note A Sidekiq card can have anywhere between @c 1 and @c N slots available for storing FPGA
   images (bitstreams).  Use skiq_read_fpga_config_flash_slots_avail() to query the number of slots
   available.

   @note The API function @c skiq_prog_fpga_from_flash(card) is equivalent to calling
   @c skiq_prog_fpga_from_flash_slot(card, 0)

   @note After successful reprogramming is complete, all RX interfaces are reset to the idle (not
   streaming) state.

   @since Function added in API @b v4.12.0

   @ingroup fpgafunctions
   @see skiq_prog_fpga_from_file
   @see skiq_prog_fpga_from_flash
   @see skiq_save_fpga_config_to_flash
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_from_flash
   @see skiq_verify_fpga_config_in_flash_slot
   @see skiq_read_fpga_config_flash_slot_metadata
   @see skiq_find_fpga_config_flash_slot_metadata
   @see skiq_read_fpga_config_flash_slots_avail

   @param[in] card      requested Sidekiq card ID
   @param[in] slot      requested flash configuration slot

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EIO        if an error occurred during FPGA re-programming
   @retval -EBADMSG    if an error occurred transacting with FPGA registers
   @retval -ESRCH      (Internal Error) if transport cannot be resolved after programming
 */
EPIQ_API int32_t skiq_prog_fpga_from_flash_slot( uint8_t card,
                                                 uint8_t slot );

/**************************************************************************************************/
/**
   @brief This function stores a FPGA bitstream into flash memory at the specified slot.  If the
   slot is @c 0, it is automatically loaded on power cycle or calling @c
   skiq_prog_fpga_from_flash(card).  If the slot is greater than @c 0 (and the card has more than
   one slot available), the FPGA configuration can be loaded by calling @c
   skiq_prog_fpga_from_flash_slot(card, slot) with the same specified @p slot value.

   @note A user may wish to store a hash or other related identifier of the bitstream in the @p
   metadata to make identifying the stored bitstream more robust than something another user may use
   (simple index or similar).

   @note The specified @p metadata is stored with the FPGA configuration at the specified slot.
   This allows for a user to quickly associate the stored configuration among several images.  This
   also then gives the user the option to skip calling skiq_verify_fpga_config_in_flash_slot()
   since that function can take a relatively long time.

   @since Function added in API @b v4.12.0

   @ingroup flashfunctions
   @see skiq_prog_fpga_from_file
   @see skiq_prog_fpga_from_flash
   @see skiq_save_fpga_config_to_flash
   @see skiq_verify_fpga_config_from_flash
   @see skiq_verify_fpga_config_in_flash_slot
   @see skiq_read_fpga_config_flash_slot_metadata
   @see skiq_find_fpga_config_flash_slot_metadata
   @see skiq_read_fpga_config_flash_slots_avail

   @param[in] card      requested Sidekiq card ID
   @param[in] slot      requested flash configuration slot
   @param[in] p_file    FILE stream reference for the requested FPGA bitstream
   @param[in] metadata  metadata to associate with the FPGA bitstream at the specified slot

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EBADF      if the FILE stream references a bad file descriptor
   @retval -ENODEV     if no entry is found in the flash configuration array
   @retval -EACCES     if no golden FPGA bitstream is found in flash memory
   @retval -EIO        if the transport failed to read from flash memory
   @retval -EFAULT     if @p p_file is NULL
   @retval -ENOENT     if the Flash data structure hasn't been initialized for this card
   @retval -ENOTSUP    if Flash access isn't supported for this card
   @retval -EFBIG      if the write would exceed Flash address boundaries and/or the flash config slot's size
   @retval -EFAULT     if the file specified by @p p_file doesn't contain an FPGA sync word
*/
EPIQ_API int32_t skiq_save_fpga_config_to_flash_slot( uint8_t card,
                                                      uint8_t slot,
                                                      FILE *p_file,
                                                      uint64_t metadata );

/**************************************************************************************************/
/**
   @brief This function verifies the contents of flash memory at a specified against the provided
   FILE reference @p p_file and @p metadata. This can be used to validate that a given FPGA
   bitstream and its metadata are accurately stored within flash memory.

   @since Function added in API @b v4.12.0

   @ingroup flashfunctions
   @see skiq_prog_fpga_from_file
   @see skiq_prog_fpga_from_flash
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_from_flash
   @see skiq_read_fpga_config_flash_slot_metadata
   @see skiq_find_fpga_config_flash_slot_metadata
   @see skiq_read_fpga_config_flash_slots_avail

   @param[in] card      requested Sidekiq card ID
   @param[in] slot      requested flash configuration slot
   @param[in] p_file    FILE stream reference for the requested FPGA bitstream
   @param[in] metadata  metadata to verify at the specified slot

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EBADF      if the FILE stream references a bad file descriptor
   @retval -EFBIG      if the FILE stream reference points to a file that exceeds the flash config slot's size
   @retval -EINVAL     if the @p slot index exceed number of accessible slots
   @retval -ENODEV     if no entry is found in the flash configuration array
   @retval -ENOTSUP    if Flash access isn't supported for this card
   @retval -EFAULT     if @p p_file is NULL
   @retval -ENOENT     (Internal Error) if the Flash data structure hasn't been initialized for this card
*/
EPIQ_API int32_t skiq_verify_fpga_config_in_flash_slot( uint8_t card,
                                                        uint8_t slot,
                                                        FILE *p_file,
                                                        uint64_t metadata );

/**************************************************************************************************/
/**
   @brief This function reads the stored metadata associated with the specified slot value.

   @note This allows a user to be more efficient in determining which bitstreams are stored in a
   given Sidekiq card without having to dump the full contents of each flash slot.

   @since Function added in API @b v4.12.0

   @ingroup flashfunctions
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_in_flash_slot
   @see skiq_find_fpga_config_flash_slot_metadata
   @see skiq_read_fpga_config_flash_slots_avail

   @param[in] card         requested Sidekiq card ID
   @param[in] slot         requested flash configuration slot
   @param[out] p_metadata  populated with retrieved metadata when return value indicates success

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -ENODEV     if no entry is found in the flash configuration array
   @retval -EFAULT     if @p p_metadata is NULL
   @retval -EINVAL     if the @p slot index exceed number of accessible slots
   @retval -ENOENT     (Internal Error) if the Flash data structure hasn't been initialized for this card
   @retval -ENOTSUP    if Flash access isn't supported for this card
   @retval -EFBIG      (Internal Error) if the read would exceed Flash address boundaries
 */
EPIQ_API int32_t skiq_read_fpga_config_flash_slot_metadata( uint8_t card,
                                                            uint8_t slot,
                                                            uint64_t *p_metadata );

/**************************************************************************************************/
/**
   @brief This function uses calls to skiq_read_fpga_config_flash_slots_avail() and
   skiq_read_fpga_config_flash_slot_metadata() to provide the caller with the lowest slot index
   whose metadata matches the specified @p metadata.

   @since Function added in API @b v4.12.0

   @ingroup flashfunctions
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_in_flash_slot
   @see skiq_read_fpga_config_flash_slot_metadata
   @see skiq_read_fpga_config_flash_slots_avail

   @param[in] card         requested Sidekiq card ID
   @param[in] metadata     requested metadata
   @param[out] p_slot      populated with first slot index where metadata matches when return value indicates success

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -ENODEV     if no entry is found in the flash configuration array
   @retval -ENOENT     if the Flash data structure hasn't been initialized for this card
   @retval -ENOTSUP    if Flash access isn't supported for this card
   @retval -ESRCH      if the metadata was not found in any of the device's flash slots
   @retval -EFBIG      (Internal Error) if the read would exceed Flash address boundaries
   @retval -EFAULT     if @p p_slot is NULL
 */
EPIQ_API int32_t skiq_find_fpga_config_flash_slot_metadata( uint8_t card,
                                                            uint64_t metadata,
                                                            uint8_t *p_slot );

/**************************************************************************************************/
/**
   @brief This function provides the number of FPGA configuration slots available for a specified
   Sidekiq card.

   @note A Sidekiq card can have anywhere between 0 and N slots available for storing FPGA images
   (bitstreams).  See below for a caveat.

   @warning Some Sidekiq cards do not have slots that are accessible in every host or carrier
   configuration.

   @since Function added in API @b v4.12.0

   @ingroup flashfunctions
   @see skiq_prog_fpga_from_flash_slot
   @see skiq_save_fpga_config_to_flash_slot
   @see skiq_verify_fpga_config_in_flash_slot
   @see skiq_read_fpga_config_flash_slot_metadata
   @see skiq_find_fpga_config_flash_slot_metadata

   @param[in] card         requested Sidekiq card ID
   @param[out] p_nr_slots  populated with the number of flash configuration slots when return value indicates success

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -ENODEV     if no entry is found in the flash configuration array
   @retval -EFAULT     if @p p_nr_slots is NULL
 */
EPIQ_API int32_t skiq_read_fpga_config_flash_slots_avail( uint8_t card,
                                                          uint8_t *p_nr_slots );

/**************************************************************************************************/
/**
    @brief  Set the state of the exit handler.

    @param[in]  enabled     if false, disable the libsidekiq exit handler, else enable it.

    By default, libsidekiq registers a handler function that is called when the running program is
    exited; this exit handler attempts to clean up after the library and free allocated resources.
    If this behavior is not desired for some reason, this function may be called with @p state set
    to false to bypass registering the exit handler.

    @since  Function added in API @b v4.14.0

    @ingroup libfunctions

    @note   The exit handler is installed after cards are initialized (using functions like
            skiq_init() or skiq_enable_cards()), so this function must be called before card
            initialization.
    @note   The exit handler is not called if the host application crashes (for example, due to
            a segmentation fault).
    @note   libsidekiq applications should still call ::skiq_exit() when access to the radios
            is no longer needed; the exit handler is installed as a safety measure to ensure
            proper cleanup.

    @return 0 on success
*/
EPIQ_API int32_t skiq_set_exit_handler_state( bool enabled );

/**************************************************************************************************/
/**
   @brief This function allows the user to switch between different reference clock sources.  This
   change is run-time only and is not written to the card nor permanent.

   @note For non-volatile storage of reference clock configuration see ref_clock test app.

   @warning Sidekiq M.2 (::skiq_m2) and Sidekiq mPCIe (::skiq_mpcie) runtime reference clock source
   configuration is not supported.
   @warning Programming the reference clock dynamically using this function will 
   initiate a full RF initialization process. The user should either call this function 
   prior to RF configuration or reconfigure RF parameters after invoking this function, 
   otherwise the user specified configuration will be lost.

   @ingroup cardfunctions
   @see skiq_read_ext_ref_clock_freq
   @see skiq_read_ref_clock_select

   @since  Function added in API @b v4.14.0

   @param[in] card              requested Sidekiq card ID
   @param[in] ref_clock_source  [::skiq_ref_clock_select_t] requested reference clock source to switch card to

   @return 0 on success, else a negative errno value
   @retval -EINVAL      if the requested reference select is invalid
   @retval -ENOTSUP     if the requested card is not supported
   @retval -ERANGE      if the requested card is not within the valid range of all cards
   @retval -ENODEV      if the requested card is not activated
*/

EPIQ_API int32_t skiq_write_ref_clock_select( uint8_t card,
                                              skiq_ref_clock_select_t ref_clock_source );

/**************************************************************************************************/
/**
   @brief This function allows the user to switch between different external reference clock
   frequencies.  This change is run-time only and is not written to the card nor permanent.  This
   will automatically update the reference clock selection to an external reference clock source.
   When changing the frequency, a supported external reference clock frequency must be used per
   the card specification.

   @note For non-volatile storage of external clock frequency configuration see ref_clock test app.

   @note Runtime reference clock frequency switching is only supported on Sidekiq Stretch
   (::skiq_m2_2280) and Sidekiq NV100 (::skiq_nv100) (as of libsidekiq v4.17.0).

   @warning Switching the reference clock frequency here will stop receiving and transmitting.
   @warning Programming the reference clock dynamically using this function will 
   initiate a full RF initialization process. The user should either call this function 
   prior to RF configuration or reconfigure RF parameters after invoking this function, 
   otherwise the user specified configuration will be lost.
   

   @ingroup cardfunctions
   @see skiq_read_ext_ref_clock_freq
   @see skiq_read_ref_clock_select

   @since  Function added in API @b v4.17.0

   @param[in] card              requested Sidekiq card ID
   @param[in] ext_freq          requested external reference clock frequency to switch to (10MHz,
   or 40MHz on both Stretch and NV100 and Stretch also supports 30.72MHz)

   @return 0 on success, else a negative errno value
   @retval -EINVAL      if the requested frequency is invalid
   @retval -ENOTSUP     if the requested card is not supported
   @retval -ERANGE      if the requested card is not within the valid range of all cards
   @retval -ENODEV      if the requested card is not activated
*/
EPIQ_API int32_t skiq_write_ext_ref_clock_freq(uint8_t card, uint32_t ext_freq);

/**************************************************************************************************/
/**
   @brief skiq_write_rx_rfic_pin_ctrl_mode selects the source of RFIC Rx enable on supported RFICs.
   This signal disables or enables the receiver signal path. Normally managed in software by
   libsidekiq, some Sidekiq platforms can be controlled by the FPGA.

   @attention Modifying RFIC pin control mode on <a
   href="https://epiqsolutions.com/rf-transceiver/sidekiq-x4/">Sidekiq X4</a>Sidekiq X4</a>
   (::skiq_x4) is supported starting in @b v4.14.0 while other Sidekiq products are not supported at
   this version.  For details regarding GPIO pin mappings, please refer to the "FMC Pin Map" section
   of <a href="https://support.epiqsolutions.com/viewforum.php?f=396">Sidekiq X4 Hardware User's
   Manual</a>.
   
   @since Function added in API @b v4.14.0

   @ingroup rficfunctions
   @see skiq_read_rx_rfic_pin_ctrl_mode

   @param[in] card       requested Sidekiq card ID
   @param[in] hdl        [::skiq_rx_hdl_t] handle of the requested rx interface
   @param[in] mode       [::skiq_rfic_pin_mode_t] desired mode

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -ENOTSUP    if the requested mode isn't supported for this card

 */
EPIQ_API int32_t skiq_write_rx_rfic_pin_ctrl_mode( uint8_t card,
                                                   skiq_rx_hdl_t hdl,
                                                   skiq_rfic_pin_mode_t mode );


/**************************************************************************************************/
/**
   @brief skiq_write_tx_rfic_pin_ctrl_mode selects the source of RFIC Tx enable on supported RFICs.
   This signal disables or enables the transmitter signal path. Normally managed in software by
   libsidekiq, some Sidekiq platforms can be controlled by the FPGA.

   @attention Modifying RFIC pin control mode on <a
   href="https://epiqsolutions.com/rf-transceiver/sidekiq-x4/">Sidekiq X4</a>Sidekiq X4</a>
   (::skiq_x4) is supported starting in @b v4.14.0 while other Sidekiq products are not supported at
   this version.  For details regarding GPIO pin mappings, please refer to the "FMC Pin Map" section
   of <a href="https://support.epiqsolutions.com/viewforum.php?f=396">Sidekiq X4 Hardware User's
   Manual</a>.

   @since Function added in API @b v4.14.0

   @ingroup rficfunctions
   @see skiq_read_tx_rfic_pin_ctrl_mode

   @param[in] card       requested Sidekiq card ID
   @param[in] hdl        [::skiq_tx_hdl_t] handle of the requested Tx interface
   @param[in] mode       [::skiq_rfic_pin_mode_t] desired mode

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -ENOTSUP    if the requested mode isn't supported for this card

 */
EPIQ_API int32_t skiq_write_tx_rfic_pin_ctrl_mode( uint8_t card,
                                                   skiq_tx_hdl_t hdl,
                                                   skiq_rfic_pin_mode_t mode );

/**************************************************************************************************/
/**
   @brief This function reads the source of control used to enable/disable RFIC Rx.

   @attention Modifying RFIC pin control mode on <a
   href="https://epiqsolutions.com/rf-transceiver/sidekiq-x4/">Sidekiq X4</a>Sidekiq X4</a>
   (::skiq_x4) is supported starting in @b v4.14.0 while other Sidekiq products are not supported at
   this version.  For details regarding GPIO pin mappings, please refer to the "FMC Pin Map" section
   of <a href="https://support.epiqsolutions.com/viewforum.php?f=396">Sidekiq X4 Hardware User's
   Manual</a>.

   @since Function added in API @b v4.14.0

   @ingroup rficfunctions
   @see skiq_write_rfic_pin_ctrl_mode

   @param[in]  card       requested Sidekiq card ID
   @param[in]  hdl        [::skiq_rx_hdl_t] handle of the requested Rx interface
   @param[out] p_mode     pointer to [::skiq_rfic_pin_mode_t] configured mode

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EFAULT     if @p p_mode is NULL
   @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality

 */
EPIQ_API int32_t skiq_read_rx_rfic_pin_ctrl_mode( uint8_t card,
                                                  skiq_rx_hdl_t hdl,
                                                  skiq_rfic_pin_mode_t *p_mode );

/**************************************************************************************************/
/**
   @brief This function reads the source of control used to enable/disable RFIC Tx.

   @attention Modifying RFIC pin control mode on <a
   href="https://epiqsolutions.com/rf-transceiver/sidekiq-x4/">Sidekiq X4</a>Sidekiq X4</a>
   (::skiq_x4) is supported starting in @b v4.14.0 while other Sidekiq products are not supported at
   this version.  For details regarding GPIO pin mappings, please refer to the "FMC Pin Map" section
   of <a href="https://support.epiqsolutions.com/viewforum.php?f=396">Sidekiq X4 Hardware User's
   Manual</a>.

   @since Function added in API @b v4.14.0

   @ingroup rficfunctions
   @see skiq_write_tx_rfic_pin_ctrl_mode

   @param[in]  card       requested Sidekiq card ID
   @param[in]  hdl        [::skiq_tx_hdl_t] handle of the requested Tx interface
   @param[out] p_mode     pointer to [::skiq_rfic_pin_mode_t] configured mode

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the requested card index is out of range
   @retval -ENODEV     if the requested card index is not initialized
   @retval -EFAULT     if @p p_mode is NULL
   @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
 */

EPIQ_API int32_t skiq_read_tx_rfic_pin_ctrl_mode( uint8_t card,
                                                  skiq_tx_hdl_t hdl,
                                                  skiq_rfic_pin_mode_t *p_mode );

/**************************************************************************************************/
/**
    @brief Indicates whether the GPSDO is available for product and FPGA bitstream

    @since Function added in API @b v4.15.0

    @ingroup gpsdofunctions

    @param[in]  card            card index of the Sidekiq of interest
    @param[out] p_supported     the status of the GPSDO support on the specified card

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the specified card index is out of range
    @retval -ENODEV     if the specified card has not been initialized
    @retval -EFAULT     if @p p_supported is NULL
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
*/
EPIQ_API int32_t skiq_is_gpsdo_supported( uint8_t card, skiq_gpsdo_support_t *p_supported );

/**************************************************************************************************/
/**
    @brief Enable the GPSDO control algorithm on the specified card

    @attention  When the GPSDO is enabled, the FPGA takes control of the warp voltage thus disabling
                manual control of the voltage.  Specifically,  @b skiq_write_tcvcxo_warp_voltage()
                is not allowed when GPSDO enabled.

    @attention  When GPSDO is enabled, the FPGA takes ownership of the temperature sensor.  Temperature
                data may not immediately be available, as noted by the -EAGAIN error code returned when
                the temperature is queried via @b skiq_read_temp()

    @since Function added in API @b v4.15.0

    @ingroup gpsdofunctions

    @param[in]  card        card index of the Sidekiq of interest

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the specified card index is out of range
    @retval -ENODEV     if the specified card has not been initialized
    @retval -ENOTSUP    if the specified card does not support an FPGA-based GPSDO
    @retval -ENOSYS     if the loaded FPGA bitstream does not implement GPSDO functionality
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
*/
EPIQ_API int32_t skiq_gpsdo_enable( uint8_t card );

/**************************************************************************************************/
/**
    @brief  Disable the GPSDO control algorithm on the specified card

    @since Function added in API @b v4.15.0

    @ingroup gpsdofunctions

    @param[in]  card        card index of the Sidekiq of interest

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the specified card index is out of range
    @retval -ENODEV     if the specified card has not been initialized
    @retval -ENOTSUP    if the specified card does not support an FPGA-based GPSDO
    @retval -ENOSYS     if the loaded FPGA bitstream does not implement GPSDO functionality
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
*/
EPIQ_API int32_t skiq_gpsdo_disable( uint8_t card );

/**************************************************************************************************/
/**
    @brief Check the enable status of the GPSDO control algorithm on the specified card

    @since Function added in API @b v4.15.0

    @ingroup gpsdofunctions

    @param[in]  card            card index of the Sidekiq of interest
    @param[out] p_is_enabled    true if the GPSDO algorithm is enabled, else false

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the specified card index is out of range
    @retval -ENODEV     if the specified card has not been initialized
    @retval -EFAULT     if @p p_is_enabled is NULL
    @retval -ENOTSUP    if the specified card does not support an FPGA-based GPSDO
    @retval -ENOSYS     if the loaded FPGA bitstream does not implement GPSDO functionality
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
*/
EPIQ_API int32_t skiq_gpsdo_is_enabled( uint8_t card, bool *p_is_enabled );

/**************************************************************************************************/
/**
   @brief Calculate the frequency accuracy of the FPGA's GPSDO oscillator frequency (in ppm)

   @note The developer may also want to use the skiq_gpsdo_is_locked() API function if
   skiq_gpsdo_read_freq_accuracy() returns @a -EAGAIN to determine what condition is causing the
   function to indicate failure

   @since Function added in API @b v4.15.0

   @ingroup gpsdofunctions
   @see skiq_gpsdo_is_locked

   @param[in]  card        card index of the Sidekiq of interest
   @param[out] p_ppm       calculated ppm (parts per million) of the GPSDO's frequency accuracy

   @return 0 on success, else a negative errno value
   @retval -ERANGE     if the specified card index is out of range
   @retval -ENODEV     if the specified card has not been initialized
   @retval -ENOTSUP    if the specified card does not support an FPGA-based GPSDO
   @retval -ENOSYS     if the loaded FPGA bitstream does not implement GPSDO functionality
   @retval -ESRCH      if the measurement is not available because the GPSDO is disabled
   @retval -EAGAIN     if the measurement is not available because:
                       - the GPS module does not have a valid fix -OR-
                       - the GPSDO algorithm is not locked
   @retval -EBADMSG    if an error occurred transacting with FPGA registers
   @retval -EFAULT     if NULL is provided for p_ppm
*/
EPIQ_API int32_t skiq_gpsdo_read_freq_accuracy( uint8_t card, double *p_ppm );

/**************************************************************************************************/
/**
    @brief Check the lock status of the GPSDO control algorithm on the specified card

    @since Function added in API @b v4.17.0

    @ingroup gpsdofunctions

    @param[in]  card            card index of the Sidekiq of interest
    @param[out] p_is_locked     true if the GPSDO is locked, else false

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the specified card index is out of range
    @retval -ENODEV     if the specified card has not been initialized
    @retval -ENOTSUP    if the specified card does not support an FPGA-based GPSDO
    @retval -ENOSYS     if the loaded FPGA bitstream does not implement GPSDO functionality
    @retval -EBADMSG    if an error occurred transacting with FPGA registers
    @retval -EFAULT     if NULL is provided for p_is_locked
*/
EPIQ_API int32_t skiq_gpsdo_is_locked( uint8_t card, bool *p_is_locked );

/*****************************************************************************/
/** The skiq_read_rx_analog_filter_bandwidth() function reads the current
    setting for the RX analog filter bandwidth.  

    @since Function added in 4.17.0

    @note that this value is automatically updated when the channel bandwidth
    is changed  
    @note This is not available for all products 

    @ingroup rxfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[out] p_bandwidth pointer to the variable that should be updated with
    the actual bandwidth of the analog filter bandwidth

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EFAULT     if @p p_mode is NULL
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
*/
EPIQ_API int32_t skiq_read_rx_analog_filter_bandwidth( uint8_t card, 
                                                       skiq_rx_hdl_t hdl,
                                                       uint32_t *p_bandwidth );

/*****************************************************************************/
/** The skiq_read_tx_analog_filter_bandwidth() function reads the current
    setting for the TX analog filter bandwidth.  

    @since Function added in 4.17.0

    @note that this value is automatically updated when the channel bandwidth
    is changed  
    @note This is not available for all products 

    @ingroup txfunctions

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[out] p_bandwidth pointer to the variable that should be updated with
    the actual bandwidth of the analog filter bandwidth

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EFAULT     if @p p_mode is NULL
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
*/
EPIQ_API int32_t skiq_read_tx_analog_filter_bandwidth( uint8_t card, 
                                                       skiq_tx_hdl_t hdl,
                                                       uint32_t *p_bandwidth );

/*****************************************************************************/
/** The skiq_write_rx_analog_filter_bandwidth() function writes the current
    bandwidth of the analog filter.  

    @since Function added in 4.17.0

    @note that this value is overwritten when the bandwidth is
    configured with ::skiq_write_rx_sample_rate_and_bandwidth  
    @note This is not available for all products 
    @note not all bandwidth settings are valid and actual setting can be queried 
    @note For AD9361 products, the analog filter bandwidth is typically set to
    the configured channel bandwidth and is automatically configured to this value
    when the sample rate and channel bandwidth is configured.  This function allows 
    the analog filter bandwidth to be overwritten, where the corner frequency of the 
    3rd order Butterworth filter is set to 1.4x of half the specified bandwidth.

    @ingroup rxfunctions
    @see skiq_write_rx_sample_rate_and_bandwidth

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_rx_hdl_t]  the handle of the requested rx interface
    @param[in] bandwidth specifies the analog filter bandwidth in Hertz

    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EFAULT     if @p p_mode is NULL
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
*/
EPIQ_API int32_t skiq_write_rx_analog_filter_bandwidth( uint8_t card,
                                                        skiq_rx_hdl_t hdl, 
                                                        uint32_t bandwidth );

/*****************************************************************************/
/** The skiq_write_tx_analog_filter_bandwidth() function writes the current
    bandwidth of the analog filter.  

    @since Function added in 4.17.0

    @note that this value is overwritten when the bandwidth is
    configured with ::skiq_write_rx_sample_rate_and_bandwidth  
    @note This is not available for all products 
    @note not all bandwidth settings are valid and actual setting can be queried 
    @note For AD9361 products, the analog filter bandwidth is typically set to
    the configured channel bandwidth and is automatically configured to this value
    when the sample rate and channel bandwidth is configured.  This function allows 
    the analog filter bandwidth to be overwritten, where the corner frequency of the 
    3rd order Butterworth filter is set to 1.6x of half the specified bandwidth.

    @ingroup txfunctions
    @see skiq_write_tx_sample_rate_and_bandwidth

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl [::skiq_tx_hdl_t]  the handle of the requested tx interface
    @param[in] bandwidth specifies the analog filter bandwidth in Hertz
    @return 0 on success, else a negative errno value
    @retval -ERANGE     if the requested card index is out of range
    @retval -ENODEV     if the requested card index is not initialized
    @retval -EFAULT     if @p p_mode is NULL
    @retval -ENOTSUP Card index references a Sidekiq platform that does not currently support this functionality
*/
EPIQ_API int32_t skiq_write_tx_analog_filter_bandwidth( uint8_t card,
                                                        skiq_tx_hdl_t hdl, 
                                                        uint32_t bandwidth );

/*****************************************************************************/
/** The skiq_write_rx_sample_shift() function allows the user to set a sample 
    delay value on either A1 or A2.
    This is currently only supported on the NV100 for SKIQ_MAX_SAMPLE_SHIFT_NV100
    samples per channel.

    @since Function added in v4.18.0

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl handle to apply the sample delay shift on
    @param[in] shift_delay # samples to delay, valid for [0, 4] range

    @return 0 on success, else a negative errno value
    @retval -EINVAL  Requested shift or handle value is not supported
    @retval -ENOTSUP Sample shift register not supported on this device
    @retval -EIO A fault occurred communicating with the FPGA
*/
EPIQ_API int32_t skiq_write_rx_sample_shift(uint8_t card, 
                                            skiq_rx_hdl_t hdl, 
                                            uint8_t shift_delay);


/*****************************************************************************/
/** The skiq_read_rx_sample_shift() function allows the user to read a sample
    delay value on either A1 or A2.
    This is currently only supported on the NV100

    @since Function added in v4.18.0

    @param[in] card card index of the Sidekiq of interest
    @param[in] hdl handle to read the sample delay shift on
    @param[out] shift_delay # samples currently delayed

    @return 0 on success, else a negative errno value
    @retval -EINVAL  Requested handle value is not supported
    @retval -ENOTSUP Sample shift register not supported on this device
    @retval -EIO A fault occurred communicating with the FPGA
*/
EPIQ_API int32_t skiq_read_rx_sample_shift(uint8_t card, 
                                            skiq_rx_hdl_t hdl, 
                                            uint8_t *shift_delay);

#ifdef __cplusplus
}
#endif

#endif

