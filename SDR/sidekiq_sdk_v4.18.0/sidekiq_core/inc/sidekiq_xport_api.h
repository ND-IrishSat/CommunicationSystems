#ifndef __SIDEKIQ_XPORT_API_H__
#define __SIDEKIQ_XPORT_API_H__

/*****************************************************************************/
/** @file sidekiq_xport_api.h

 <pre>
 Copyright 2016-2021 Epiq Solutions, All Rights Reserved
 </pre>

    @brief This file contains the public interface of the sidekiq_xport_api
    provided by libsidekiq.

    @page CustomTransport Sidekiq Transport Layer

@brief This page is a discussion of the existing Sidekiq transport layers
available to developers and includes options for implementing and using a custom
transport layer.

    @section STLOverview Overview

Sidekiq was developed under the assumption that there would always be either a
PCIe or USB interface available to connect the host system and the card itself.
This has worked out reasonably well up until now, with a common libsidekiq
library capable of supporting either of these interfaces.  Most developers will
only need to use the "stock" transport layers.  The software/FPGA architecture
for using PCIe as the transport interface is shown below.

@image html sidekiq_software_fpga_block_diagram.png
@image latex sidekiq_software_fpga_block_diagram.eps "Software / FPGA Architecture" width=10cm

However, for some applications, it is necessary to provide a custom transport
interface between libsidekiq and the FPGA.  In this case, the architecture to
support this would need to have a custom software layer at the bottom of the
software stack, as well as a custom FPGA interface.  Hosting a Sidekiq card in
custom hardware may benefit from a custom transport layer.  This alternate
architecture with a custom transport is shown below.

@image html sidekiq_software_fpga_alternate_transport_block_diagram.png
@image latex sidekiq_software_fpga_alternate_transport_block_diagram.eps "Software / FPGA Architecture - Custom Transport" width=10cm

@section CTInterface Custom Transport Interface

Support for a custom transport implementation is available in the latest
libsidekiq release starting with v3.2.0 and necessitates the development of
three new software/FPGA components.  This is detailed in the sections below.

- @b libsidekiq: This is the primary library that supports the use of a separate
  external transport implementation.  The existing PCIe and USB transport layers
  used by libsidekiq are already bundled with libsidekiq and are available for
  use by applications.  Applications that wish to use a custom transport
  implementation may enhance their application by registering the custom
  transport implementation (skiq_register_custom_transport()) and initializing
  libsidekiq to use that custom transport (skiq_init_xport()).

  - <tt>int32_t skiq_register_custom_transport(skiq_xport_card_functions_t *
  functions)</tt> - This function performs registration of all the required card
  function pointers so that libsidekiq knows which functions should be called in
  the custom transport implementation for the required transport operations.
  Card functions (probe, init, exit) are registered with this function.  Probe
  and init functions can register functions related to FPGA, RX, and TX
  functionality depending on a custom transport's capability.  This includes
  functions for reading/writing FPGA registers, starting/stopping streaming,
  receiving contiguous blocks of samples, and flushing the transport.  See the
  @ref CTImplementationGuide "Sidekiq Custom Transport Implementation Guide"
  section in Sidekiq API documentation for more details.

  - <tt>int32_t skiq_init ( skiq_xport_type_t type,
  skiq_xport_init_level_t level, uint8_t * p_card_nums, uint8_t num_cards )</tt>
  - This function is used to initialize the library and cards.  If using the
  custom transport implementation, the transport functions must be registered
  prior to calling this API.  The specified level may be '
  @ref skiq_xport_init_level_basic "basic"' where only FPGA related functions
  are registered, or '@ref skiq_xport_init_level_full "full"'where RX / TX
  streaming related functions are registered.  See the Sidekiq API
  documentation for more details on this function and related enumerations.

- <b>Custom SW "Driver"</b>: This is the Linux kernel space driver that may be
  required by the custom transport layer to support both register and streaming
  operations with hardware.  The assumption here is that the custom transport
  implementation calls this software interface to provide the physical transport
  between the CPU and the FPGA.

- <b>Custom FPGA Interface</b>: This is the FPGA block that manages both the
  register and streaming interfaces on the FPGA.  The assumption here is that
  the vast majority of the stock Sidekiq FPGA reference design will be
  preserved, replacing only the PCIe/DMA interface currently provided by
  Northwest Logic with the custom transport interface.

    @section CTImplementationGuide Custom Transport Implementation Guide

    @subsection Overview Overview

The current implementation of custom FPGA transport support has the ability to
perform register transactions as well as receive sample streaming.  While the
transmit sample streaming hooks are in place, a custom transport implementation
may leave them at their defaults.  This release is based on libsidekiq v3.1.0
and adds the custom FPGA transport support and is planned to be released as
libsidekiq v3.2.0.

    @subsection TransportImplementation Transport Implementation

In the SDK's top-level directory there are two new directories that have example
implementations.  The first, @b custom_xport_bare, is a simple stub example
where all functions print their parameters and return success.  This example can
be used as a basis for new work.  The second example, @b custom_xport_example,
is the PCIe transport layer implementation and can be compiled and referenced as
a complete example.

    @subsection Compiling Compiling the Transport Implementation

-# Change directory to either @b custom_xport_bare or @b custom_xport_example
-# Type <tt>make BUILD_CONFIG=x86_64.gcc</tt>

This builds the custom transport and some example test applications.  A test
application's object file is linked with the custom transport object(s) and
libsidekiq's static library.  The example test applications are the same as
their test_apps counterpart with the exception of calling
skiq_register_custom_transport(), and specifying
::skiq_xport_type_custom in calls to skiq_init().

    @subsection API API

Below is a summary of the function sets that should be supported by the transport library.

    @subsection CardFunctions Card Functions

All three card functions are required for a custom transport implementation.
Pointers to the functions are collected into a ::skiq_xport_card_functions_t
struct and passed to skiq_register_custom_transport() function before calling
skiq_init_xport().

- <tt>int32_t (*card_probe)( uint64_t *p_uid_list, uint8_t *p_num_cards );</tt>
- <tt>int32_t (*card_init)( skiq_xport_init_level_t level, uint64_t xport_uid );</tt>
- <tt>int32_t (*card_exit)( skiq_xport_init_level_t level, uint64_t xport_uid );</tt>

The @ref card_probe and @ref card_init are responsible for further registering
the remaining transport function sets (FPGA, RX, and TX) based on the caller's
::skiq_xport_init_level_t request and the card's capabilities.  Refer to the
@b custom_xport_bare example.

    @subsection FPGAFunctions FPGA Functions

The FPGA functions implement transporting requests to read / write registers as
well as bringing up / down the transport link to the FPGA (for reprogramming).
Pointers to the functions are collected into a ::skiq_xport_fpga_functions_t
struct and registered on a per-card basis by calling
xport_register_fpga_functions().

- <tt>int32_t (*fpga_reg_read)( uint64_t xport_uid, uint32_t addr, uint32_t* p_data );</tt>
- <tt>int32_t (*fpga_reg_write)( uint64_t xport_uid, uint32_t addr, uint32_t data );</tt>
- <tt>int32_t (*fpga_down)( uint64_t xport_uid );</tt>
- <tt>int32_t (*fpga_up)( uint64_t xport_uid );</tt>

    @subsection RXFunctions RX Functions

The RX functions implement preparing the transport link for starting, stopping,
pausing, and resuming receive sample data streaming as well as flushing buffered
receive data and transporting receive sample data.  Pointers to the functions
are collected into a ::skiq_xport_rx_functions_t struct and registered on a
per-card basis by calling xport_register_rx_functions().

- <tt>int32_t (*rx_start_streaming)( uint64_t xport_uid, skiq_rx_hdl_t hdl );</tt>
- <tt>int32_t (*rx_stop_streaming)( uint64_t xport_uid, skiq_rx_hdl_t hdl );</tt>
- <tt>int32_t (*rx_pause_streaming)( uint64_t xport_uid );</tt>
- <tt>int32_t (*rx_resume_streaming)( uint64_t xport_uid );</tt>
- <tt>int32_t (*rx_flush)( uint64_t xport_uid );</tt>
- <tt>int32_t (*rx_receive)( uint64_t xport_uid, uint8_t **pp_data, uint32_t *p_data_len );</tt>

    @subsection TXFunctions TX Functions

The TX functions implement preparing the transport link for starting and
stopping transmit sample data as well as transporting transmit sample data.
Pointers to the functions are collected into a ::skiq_xport_tx_functions_t
struct and registered on a per-card basis by calling
xport_register_tx_functions().

- <tt>int32_t (*tx_initialize)( uint64_t xport_uid, skiq_tx_transfer_mode_t tx_transfer_mode, uint32_t num_bytes_to_send, uint8_t num_send_threads, void (*tx_complete_cb)(int32_t status, uint32_t *p_data) );</tt>
- <tt>int32_t (*tx_start_streaming)( uint64_t xport_uid, skiq_tx_hdl_t hdl );</tt>
- <tt>int32_t (*tx_pre_stop_streaming)( uint64_t xport_uid, skiq_tx_hdl_t hdl );</tt>
- <tt>int32_t (*tx_stop_streaming)( uint64_t xport_uid, skiq_tx_hdl_t hdl );</tt>
- <tt>int32_t (*tx_transmit)( uint64_t xport_uid, skiq_tx_hdl_t hdl, int32_t *p_samples, void *p_private );</tt>

@note It is NOT recommended to use xport_register_fpga_functions(),
xport_register_rx_functions(), xport_register_tx_functions(),
xport_unregister_fpga_functions(), xport_unregister_rx_functions(), or
xport_unregister_tx_functions() from functions other than the @ref card_init and
@ref card_exit implementations.

 */

/***** INCLUDES *****/
#include <stdint.h>

#include "sidekiq_xport_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***** CARD FUNCTIONS *****/

/*****************************************************************************/
/** The skiq_register_custom_transport function registers a set of custom
 * transport card functions.  At a minimum, the .card_probe and .card_init
 * function pointers must point to valid implementations and cannot be NULL.
 * Only one custom transport implementation may be registered and is accessed
 * indirectly by specifying skiq_xport_type_custom in calls to skiq_init().
 *
 * @param[in] functions reference to a skiq_xport_card_functions_t struct to register
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_register_custom_transport( skiq_xport_card_functions_t *functions );

/*****************************************************************************/
/** The skiq_unregister_custom_transport function unregisters (removes) the
 * current custom transport card functions.
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t skiq_unregister_custom_transport( void );

/******************************************************************************/
/** The xport_register_fpga_functions function is to be used by a custom
 * transport .card_init implementation to register a set of FPGA functions.
 *
 * @param[in] p_xport_id pointer to the transport identifier
 * @param[in] functions reference to a skiq_xport_fpga_functions_t struct to register
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t xport_register_fpga_functions( skiq_xport_id_t *p_xport_id,
                                                skiq_xport_fpga_functions_t *functions );

/******************************************************************************/
/** The xport_register_rx_functions function is to be used by a custom transport
 * .card_init implementation to register a set of RX functions.  RX functions
 * are used in calls to skiq_start_rx_streaming,
 * skiq_start_rx_streaming_on_1pps, skiq_stop_rx_streaming,
 * skiq_stop_rx_streaming_on_1pps, skiq_write_rx_sample_rate_and_bandwidth, and
 * skiq_receive.
 *
 * @param[in] p_xport_id pointer to the transport identifier
 * @param[in] functions reference to a skiq_xport_rx_functions_t struct to register
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t xport_register_rx_functions( skiq_xport_id_t *p_xport_id,
                                              skiq_xport_rx_functions_t *functions );

/******************************************************************************/
/** The xport_register_tx_functions function is to be used by a custom transport
 * .card_init implementation to register a set of TX functions.  TX functions
 * are used in calls to skiq_start_tx_streaming,
 * skiq_start_tx_streaming_on_1pps, skiq_stop_tx_streaming,
 * skiq_stop_tx_streaming_on_1pps, and skiq_receive.
 *
 * @param[in] p_xport_id pointer to the transport identifier
 * @param[in] functions reference to a skiq_xport_tx_functions_t struct to register
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t xport_register_tx_functions( skiq_xport_id_t *p_xport_id,
                                              skiq_xport_tx_functions_t *functions );

/*****************************************************************************/
/** The xport_unregister_fpga_functions function is to be used by a custom
 * transport .card_init and/or .card_exit implementation to clear the set of
 * FPGA functions.
 *
 * @param[in] p_xport_id pointer to the transport identifier
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t xport_unregister_fpga_functions( skiq_xport_id_t *p_xport_id );

/*****************************************************************************/
/** The xport_unregister_rx_functions function is to be used by a custom
 * transport .card_init and/or .card_exit implementation to clear the set of RX
 * functions.
 *
 * @param[in] p_xport_id pointer to the transport identifier
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t xport_unregister_rx_functions( skiq_xport_id_t *p_xport_id );

/*****************************************************************************/
/** The xport_unregister_tx_functions function is to be used by a custom
 * transport .card_init and/or .card_exit implementation to clear the set of TX
 * functions.
 *
 * @param[in] p_xport_id pointer to the transport identifier
 *
 * @return int32_t  status where 0=success, anything else is an error
 */
EPIQ_API int32_t xport_unregister_tx_functions( skiq_xport_id_t *p_xport_id );

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
