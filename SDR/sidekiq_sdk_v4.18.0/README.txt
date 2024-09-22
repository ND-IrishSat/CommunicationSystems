Sidekiq Update v4.18.0: Jan-2024
Epiq Solutions

Introduction
------------

The following are included in this tarball:
  - sidekiq_sdk_v4.18.0
     - prebuilt apps for 64-bit x86, other platforms available for download
     - libsidekiq for 64-bit x86, 32-bit x86, variants of ARM
     - test app source files
  - Sidekiq firmware v2.9
  - Updated FPGA bitstream (v3.18.0)
     - PCI transport for Sidekiq NV100
     - PCI and USB transports for Sidekiq M.2
     - PCI transport for Sidekiq Stretch
  - Updated FPGA bitstreams (v3.17.2)
    - PCI transport for Sidekiq X4 (AMS WB3XBM carrier)
  - Updated FPGA bitstreams (v3.17.1)
     - PCI transport for Sidekiq X4 (HTG-K800 carrier, both KU060 and KU115)
     - PCI transport for Sidekiq X4 (HTG-K810 carrier, both KU060 and KU115)
  - Unchanged FPGA bitstreams (v3.17.0)
     - AXI transport for Matchstiq Z3u
     - AXI transport for Sidekiq Z2
  - Unchanged FPGA bitstreams (v3.16.1)
     - PCI transport for Sidekiq X2 (HTG-K800 carrier with KU060)
  - Unchanged FPGA bitstreams (v3.13.1)
     - PCI and USB transports for Sidekiq mPCIe
  - pre-built kernel modules
     - dmadriver v5.4.9.0
     - pci_manager v1.2.4
     - skiq_platform_device v1.0.0
     - sidekiq_uart v1.0.4
     - sidekiq_gps v1.1.2


New Features for libsidekiq/FPGA
--------------------------------
o New Features for v4.18.0

  === Sidekiq M.2 ===
     - Added support for external 50MHz reference clock

  === Sidekiq X4 ===
     - FPGA bitstream v3.17.2
      - Added support supplying 100MHz reference clock over FMC in PDK on AMS WB3XBM

  === NV100 ===
    - Added RX Alignment registers for Sidekiq NV100
      - New API function(s):
          o skiq_write_rx_sample_shift()
          o skiq_read_rx_sample_shift()
      - RX Alignment registers support a calibration routine for A1/A2 channels that 
        maintain rx sample alignment when stopping and restarting streaming. Contact 
        customer support for instructions.
    - Added support for A2 handle on Tx
    - Remapped TxA1 to LO1. See NV100 Hardware User manual for more details. 
      -The updated LO mapping is:
        TxA1 ==> LO1
        TxA2 ==> LO1
        TxB1 ==> LO2
        RxA1 ==> LO1
        RxA2 ==> LO1
        RxB1 ==> LO2

o New Features for v4.17.6
  
  === All ===
    - Added ability for library to support new hardware revisions for existing products
      - Any future hardware revision which does not impact the software interface will 
        be supported by libsidekiq v4.17.6 or later. Customers interested in using a 
        fixed libsidekiq version should consider updating.

  === NV100 ===
    - Added support for A2 handle on Rx
    - Updated rfic firmware now supports phase-coherent two channel Rx
      - Use A1/A2 handles for coherent Rx 

o New Features for v4.17.5
  === Matchstiq Z3u and Sidekiq Stretch and Sidekiq NV100 ===
    - GPSDO Persistent Execution 

o New Features for v4.17.4
  === Sidekiq Z2 and Matchstiq Z3u ===
     - Network Transport:
        - Added transmit capability
           - Related API function(s):
              o skiq_start_tx_streaming()
              o skiq_stop_tx_streaming()
              o skiq_transmit()
        - Enable configuration of Rx/Tx calibration modes
           - Related API function(s):
              o skiq_write_rx_cal_mode()
              o skiq_write_rx_cal_type_mask()
              o skiq_write_tx_quadcal_mode()
        - Allow FPGA maniplulation of the Rx stream
           - Related API function(s):
              o skiq_write_rx_dc_offset_corr()
              o skiq_read_rx_dc_offset_corr()
              o skiq_write_rx_data_src()
              o skiq_read_rx_data_src()
              o skiq_write_iq_order_mode()
              o skiq_read_iq_order_mode()

o New Features for v4.17.3
  === Matchstiq Z3u ===
    - Added support for revision D hardware

o New Features for v4.17.2

  === No new features for v4.17.2 ===

o New Features for v4.17.1

  === No new features for v4.17.1 ===

o New Features for v4.17.0

  === All ===
    - Add support for Sidekiq NV100 rev C
    - Extend on-the-fly reference clock source switching to include frequency
       - Related API function(s):
          o skiq_write_ext_ref_clock_freq()
    - Expose GPSDO lock state through new public API function
       - Related API functions(s):
          o skiq_gpsdo_is_locked()
       - Example Test Application: (new)
          o read_gpsdo
    - Expose Sidekiq card's warp capabilities in skiq_param_t
    - Improve rx_samples_on_trigger.c example application

  === Sidekiq mPCIe, M.2, Stretch, Z2, and Matchstiq Z3u ===
    - Enable user override of AD9361 analog filter settings
       - Related API function(s):
          o skiq_read_rx_analog_filter_bandwidth()
          o skiq_read_tx_analog_filter_bandwidth()
          o skiq_write_rx_analog_filter_bandwidth()
          o skiq_write_tx_analog_filter_bandwidth()

  === Matchstiq Z3u ===
    - Significantly increased the sustained receive performance

  === Sidekiq X4 ===
    - Update fdd_rx_tx_samples.c example application to add support for Sidekiq
      X4


Bug Fixes for libsidekiq/FPGA
-----------------------------
o Bug Fixes for v4.18.0

  === All ===
    - Writing the rf port for transmit now returns -ENOTSUP on devices that do not
      support this functionality
      - Related API function(s):
          o skiq_write_tx_rf_port_for_hdl()

  === M2, NV100, Stretch ===
    - FPGA bitstream v3.18.0
      - Fixed under-run reporting on B1/A2
      - Timestamp synchronization fixes

  === Sidekiq NV100, M.2 ===
    - FPGA bitstream v3.18.0
      - Fix an issue where Tx2 channel (TxB1/TxA2) underruns was incorrectly reported
        - Related API function(s):
            o skiq_read_tx_num_underruns()
            o skiq_read_tx_num_late_timestamps()
            
  === Sidekiq NV100 ===
    - FPGA bitstream v3.18.0
      - updated partial reconfiguration use case that caused rx2 to not establish an lssi link
      - fixes rf timestamp alignment between A1 and A2 channels
      - sign change on handle A2 so that it matches A1 input channel

o Bug Fixes for v4.17.6

  === All ===
    - Fix memory leak that can occur if an application repeatedly calls skiq functions 
      outside of initialization
      - Related API function(s):
          o skiq_get_cards()
          o skiq_read_serial_string()
          o skiq_get_card_from_serial_string()

  === Sidekiq NV100, Stretch, Z3u ===
    - Improved determinism of acquisitions triggered on a pps edge
      - Related API function(s):
          o skiq_start_rx_streaming_on_1pps()
          o skiq_start_rx_streaming_multi_on_trigger()
    - Fix instability with GPSDO on card intialization

  === Sidekiq NV100 ===
    - Improved sample latency and coherence performance between Rx1/Rx2

  === Sidekiq X4 ===
    - Updated bitstream improves Rx deterministic latency
    - Correct an issue where hop lists with certain frequencies can throw errors
      and fail to tune

  === Sidekiq X4, HTG-K800 or HTG-K810, KU115 variant ===
     - Updated bitstream resolves PCIe rescan failure after reconfiguring FPGA

  === Sidekiq Z2 and Matchstiq Z3u ===
    - Network Transport:
      - Fix build configuration issue that would remove network transport support
        from applications compiled against the libisdekiq binaries installed 
        by 4.17.5

o Bug Fixes for v4.17.5

  === All ===
    - Network Transport now supports platforms with glibc 2.34 or later

  === Sidekiq NV100, Stretch, M.2 ===
    - Updated bitstream resolves intermittent header corruption in low 
      latency mode. 

  === Sidekiq X2 ===
    - Reported attenuation value can no longer return out of range values
       - Related API function(s):
          o skiq_read_tx_attenuation()

o Bug Fixes for v4.17.4

  === All ===
    - Update Sidekiq API Doc to reflect A/D, D/A, and IMU capabilities

  === Sidekiq NV100 ===
     - Correct a misconfiguration of the channel bandwidth on NV100
        - Related API functions(s):
           o skiq_write_rx_sample_rate_and_bandwidth()
     - Restore the calibration settings which were overwritten after changing
       sample rate
        - Related API functions(s):
           o skiq_write_rx_sample_rate_and_bandwidth()
           o skiq_write_rx_cal_type_mask()

  === Sidekiq X4 ===
     - Resolved an issue preventing the LO from being tuned to certain
       frequencies
        - Related API functions(s):
           o skiq_write_rx_LO_freq()
           o skiq_write_tx_LO_freq()

  === Sidekiq X2, X4 ===
    - Increase the maximum permitted value when adjusting the warp voltage
       - Related API function(s):
          o skiq_write_tcvcxo_warp_voltage()

  === Sidekiq mPCIe, M.2 ===
    - Fix an issue where the on-board temperature was incorrectly reported
      after subsequent calls to skiq_read_temp()
       - Related API function(s):
          o skiq_read_temp()

  === Matchstiq Z3u ===
    - Added default calibration data for use on units without factory
      generated calibration data

o Bug Fixes for v4.17.3
  === Sidekiq Z2 and Matchstiq Z3u ===
     - Network Transport:
        - Stability improvements

o Bug Fixes for v4.17.2

  === All ===
     - Disable libsidekiq’s internal custom transport when a user registers
       their own transport so as not to interfere with device detection
     - Downgrade logging message severity from WARN->INFO on a valid
       operation mode of reduced number of Tx/Rx channels

  === Test Apps ===
     - read_gpsdo.c
        - Set "host" as the default 1PPS source
        - Allow users to configure external 1PPS source
     - tdd_rx_tx_samples.c
        - Add additional command line parameters and utility functions
     - tx_samples_async.c
        - Restructure example application, improve the capability / command line options

  === Sidekiq mPCIe, M.2 ===
     - Report FPGA bitstream version as unavailable instead of v0.0.0 if the
       expected transport is absent

  === Sidekiq NV100 ===
     - Correct the reporting of the Tx channel bandwidth on NV100
        - Related API functions(s):
           o skiq_read_tx_sample_rate_and_bandwidth()

  === Sidekiq X2 and Sidekiq X4 ===
  - FPGA bitstream v3.16.1
     - Fix Errata SW5: Transmit on timestamp will no longer erroneously send
       four samples upon reception of the first packet.
     - X4 transmit at maximum block size (65532) no longer drops packets

  === Sidekiq Z2 and Matchstiq Z3u ===
     - Network Transport:
        - Significantly improve initialization time

o Bug Fixes for v4.17.1

  === Test Apps ===
     - rx_samples_on_trigger.c
        - Fix output file naming
     - pps_tester.c
        - Fix card detection logic

  === Sidekiq NV100 ===
     - Fix receive path by setting RF input port after every re-tune of the LO
     - Fix last calibrated LO frequency tracking for transmit paths
     - Update RFIC profiles to accurately report RF transmit bandwidth

  === Sidekiq X4 ===
     - Fix internal handle assignment following a re-initialization of a card.
       This can happen in several cases of interacting with the libsidekiq API:
        - If a user calls:
           - skiq_disable_cards() followed by a call to skiq_enable_cards()
           - skiq_exit() followed by a call to skiq_init()
           - skiq_write_ref_clock_select() on its own
           - skiq_write_ext_ref_clock_freq() on its own
           - skiq_prog_fpga_from_flash() on its own

  === Matchstiq Z3u ===
     - Fix setting RF isolation during TRX

  === Sidekiq mPCIe, M.2, Stretch, Z2, and Matchstiq Z3u ===
     - Fix configuring RFIC back to defaults after a re-initialization.  This can
       happen in several cases of interacting with the libsidekiq API.
        - If a user calls:
           - skiq_disable_cards() followed by a call to skiq_enable_cards()
           - skiq_exit() followed by a call to skiq_init()
           - skiq_write_ref_clock_select() on its own
           - skiq_write_ext_ref_clock_freq() on its own
           - skiq_prog_fpga_from_flash() on its own
        - Resolves "Rx filter caching issue after FPGA reprogramming" issue


o Bug Fixes for v4.17.0

  === All ===
    - Update skiq_write_rx_dc_offset_corr() to return -ENOTSUP for Sidekiq X2 and X4
    - Fix defect where skiq_gpsdo_read_freq_accuracy() reports success when there's
      no fix or 1PPS
    - Disable all receive / transmit channels when disabling a specified card
    - Fix defect when reading TCVCXO warp voltage prior to full initialization
    - Fix defect where a card failed initialization if skiq_rx_cal_mode_auto is set
    - Fix defect related to hotplugging when utilizing a custom transport

  === Sidekiq Z2 and Matchstiq Z3u ===
    - Fix defect causing timestamp gaps when using network transport


Notes for libsidekiq/FPGA
-------------------------

Starting with v4.10.0 of libsidekiq and its associated System Updates, SRFS
(System RF Server) will no longer be released on any platform other than the
Matchstiq S10 / S20 series.  Any concerns, comments and questions can be posted
to the customer's private subforum through the Epiq Solutions Support website.

Alternatives to SRFS include:

  - ERA (https://support.epiqsolutions.com/viewforum.php?f=348)
  - gr-sidekiq (master branch from https://github.com/epiqsolutions/gr-sidekiq)


Known Issues with libsidekiq/FPGA
---------------------------------

o Reprogramming the Sidekiq Stretch's FPGA bitstream does not restore the
  GPSDO algorithm state nor status.

o Sidekiq X4 applications that are transmit-only and use 245.76Msps, 250Msps,
  491.52Msps, or 500Msps as a sample rate require a user to set the sample rate
  for one of the ORX ports (RxC1 or RxD1) to the same sample rate.

o Long transmit QEC calibration times on X4
  Transmit QEC calibration can take up to 24 seconds between 5GHz and 5.5GHz.

o Invalid samples on Sidekiq Stretch after improper app shutdown
  If a libsidekiq application exits (or crashes) without calling skiq_exit() or
  skiq_stop_rx_streaming() (or its variants), the next time Stretch is
  initialized, the I/Q samples are invalid and do not change.  The current
  workaround when using a Sidekiq Stretch and a libsidekiq application
  improperly ends is to reprogram the FPGA from flash.

  *UPDATE*: Applications built against libsidekiq v4.14.0 or later can
  automatically call skiq_exit() at exit and can address this issue whenever
  skiq_exit() is successful.

o Underruns in Packed Mode
  While transmitting in packed and immediate mode, if an underrun occurs, the
  packed data will no longer be aligned.  Underruns must be avoided when running
  in packed mode.

o Stale Sample Data on NVIDIA Jetson TX1 / TX2 / certain Gateworks variants
  With specific sample rates, the received data may be appear to be stale.  This
  can be detected by monitoring the timestamps and ensure that it always
  increments.  UPDATE: This behavior can be mitigated by pinning the process /
  thread that is calling skiq_receive to a single core (e.g. `taskset` from the
  command line or setting the CPU affinity in C/C++)

o X2 / X4 Thunderbolt3 Chassis
  Plugging in the Thunderbolt3 Chassis does not always result in a PCIe link
  between the Kintex UltraScale FPGA and a host platform.  A user may need to
  unplug and plug the TB3 cable in order for the host to rescan the link.  Use
  'lspci -d 19aa:' on a Linux host to see if a connection has been made.

o Hot-plugging
  Hot-plugging is only supported in applications built against libsidekiq
  v4.14.0 and later.  With the introduction of a card manager in v4.0.0, the
  card cache can become stale if a Sidekiq is unplugged or plugged in before or
  after running libsidekiq applications prior to v4.14.0.  It is recommended to
  have all Sidekiq units connected before running any hot-plug incapable
  libsidekiq applications.  Should the card cache become stale, running a
  hot-plug capable application or rebooting the system will correct the issue.

o Extra I/Q Sample Block received using synced trigger source
  When using skiq_stop_rx_streaming_multi_synced() or `skiq_trigger_src_synced`,
  there is a chance that a user will receive one additional I/Q sample block on
  one receive handle than the other streaming handles.  This is understood, but
  not addressed in this release.  It may be addressed in a future software
  and/or FPGA release.  The impact to the user is that for N sample blocks, one
  handle could receive N+1 sample blocks, however all N sample blocks would
  arrive with sample timestamps synchronized if no receive underruns occur
  during streaming.

o Rx filter caching issue after FPGA reprogramming -- ** Resolved in v4.17.1 **
  The Rx filter configuration cache may not be restored after reprogramming the FPGA.
  To workaround this issue, the cache can be restored by tuning the LO
  (skiq_write_rx_lo_freq / skiq_write_tx_lo_freq) twice: once at a frequency
  greater than 3GHz and then once below 3GHz.

Limited Network Transport Capabilities
-------------------------------

  === API function support ===
    The following API functions are not supported in libsidekiq v4.16.0 via
    remote operation using the network-based transport:
      - Reprogramming the RFIC via skiq_prog_rfic_from_file()
      - Reprogramming the FPGA via skiq_prog_fpga_from_file()
      - Accessing the flash via
         - skiq_verify_fpga_config_from_flash()
         - skiq_save_fpga_config_to_flash_slot()
         - skiq_verify_fpga_config_in_flash_slot()
      - Frequency hopping via
         - skiq_write_rx_freq_tune_mode()
         - skiq_read_rx_freq_tune_mode()
         - skiq_write_tx_freq_tune_mode()
         - skiq_read_tx_freq_tune_mode()
         - skiq_write_rx_freq_hop_list()
         - skiq_read_rx_freq_hop_list()
         - skiq_write_tx_freq_hop_list()
         - skiq_read_tx_freq_hop_list()
         - skiq_write_next_rx_freq_hop()
         - skiq_write_next_tx_freq_hop()
         - skiq_perform_rx_freq_hop()
         - skiq_perform_tx_freq_hop()
         - skiq_read_curr_rx_freq_hop()
         - skiq_read_curr_tx_freq_hop()
         - skiq_read_next_rx_freq_hop()
         - skiq_read_next_tx_freq_hop()
      - Configuring the RX/TX calibration modes via
         - skiq_write_rx_cal_mode()
         - skiq_write_rx_cal_type_mask()
         - skiq_write_tx_quadcal_mode()
      - TX streaming via
         - skiq_start_tx_streaming()
         - skiq_stop_tx_streaming()
         - skiq_transmit()
      - Only a single remote card can be used
      - Only a single handle can be used
      - RF port selection is not supported on Z2


Limited Sidekiq X2 Capabilities
-------------------------------

  === RX Sample Rates ===

    Only the following RX sample rate / channel bandwidths (and available
    decimation stages) are supported.  For example, if 50 Msps is available and
    the handle supports decimation by 2, 4, 8, 16, 32, and 64, then 25, 12.5,
    6.25, 3.125, 1.5625, and 0.78125 Msps are also available (starting with FPGA
    bitstream v3.12.0 and libsidekiq v4.10.0).

  A1/A2 only
    - 30.72 Msps Sample Rate / 18 / 20 / 25 MHz Channel Bandwidth
    - 36.864 Msps Sample Rate / 30.228 MHz Channel Bandwidth

  A1/A2/B1
    - 50 Msps Sample Rate / 41 MHz Channel Bandwidth
    - 61.44 Msps Sample Rate / 25 MHz Channel Bandwidth
    - 61.44 Msps Sample Rate / 50 MHz Channel Bandwidth
    - 73.728 Msps / 30.228 MHz Channel Bandwidth
    - 73.728 Msps / 60.456 MHz Channel Bandwidth
    - 100 Msps Sample Rate / 82 MHz Channel Bandwidth
    - 122.88 Msps Sample Rate / 100 MHz Channel Bandwidth
    - 153.6 Msps Sample Rate / 100 MHz Channel Bandwidth

  B1 only
    - 245.76 Msps Sample Rate / 100 MHz Channel Bandwidth
    - 245.76 Msps Sample Rate / 200 MHz Channel Bandwidth

  Decimated handles (default A1/A2)
    - Decimation by 2, 4, 8, 16, 32, and 64 supported by default, these
      decimation rates apply to all above supported sample rates

  === TX Sample Rates ===

    Only the following TX sample rate / channel bandwidths are supported.

  A1/A2 only
    - 50 Msps Sample Rate / 41 MHz Channel Bandwidth
    - 61.44 Msps Sample Rate / 50 MHz Channel Bandwidth
    - 73.728 Msps Sample Rate / 60.456 MHz Channel Bandwidth
    - 100 Msps Sample Rate / 82 MHz Channel Bandwidth
    - 122.88 Msps Sample Rate / 100 MHz Channel Bandwidth
    - 153.6 Msps Sample Rate / 100 MHz Channel Bandwidth

  Note: Additional rates may be available, contact Epiq Solutions for details.
  Additionally, a user can create and configure the radio with their own
  profile.  Refer to the SDK Manual (skiq_prog_rfic_from_file) for additional
  details.

  === Initialization Duration ===

   The procedure to initialize the hardware is slower than desired.

  === TX Timestamp Mode ===

    Certain TX sample rates result in the TX timestamp incrementing at half of
    the configured TX sample rate.  This is caused by the RX sample rate (or
    base RX sample rate in the case of decimation) being configured for half the
    rate of the TX sample rate.  The following configurations result in this
    behavior:

   - RX 30.72 Msps Sample Rate / 18 / 20 / 25 MHz Channel Bandwidth;
     TX 61.44 Msps Sample Rate / 36 / 40 / 50 MHz Channel Bandwidth
   - RX 36.864 Msps Sample Rate / 30.228 MHz Channel Bandwidth;
     TX 73.728 Msps Sample Rate / 60.456 MHz Channel Bandwidth


Limited Sidekiq Z2 Capabilities
-------------------------------

o Transmit
  Asynchronous transmit via the API is not currently supported.


Limited Sidekiq X4 Capabilities
-------------------------------

  === RX Sample Rates ===

    Only the following sample rate / channel bandwidths (and available
    decimation stages) are supported.  For example, if 100 Msps is available and
    the handle supports decimation by 2, 4, 8, 16, 32, and 64, then 50, 25,
    12.5, 6.25, 3.125, and 1.5625 Msps are also available (starting in with FPGA
    bitstream v3.12.0 and libsidekiq v4.10.0).

    A1/A2/B1/B2/C1/D1
      = Available in v4.9.0 and later
        - 61.44 Msps Sample Rate / 50 MHz Channel Bandwidth
        - 100 Msps Sample Rate / 82 MHz Channel Bandwidth
        - 122.88 Msps Sample Rate / 100 MHz Channel Bandwidth
        - 245.76 Msps Sample Rate / 200 MHz Channel Bandwidth
      = Available in v4.10.0 and later
        - 250 Msps Sample Rate / 200 MHz Channel Bandwidth
      = Available in v4.10.1 and later
        - 50 Msps Sample Rate / 41 MHz Channel Bandwidth
      = Available in v4.11.1 and later
        - 50 Msps / 20 MHz Channel Bandwidth
        - 61.44 Msps Sample Rate / 25 MHz Channel Bandwidth
        - 73.728 Msps Sample Rate / 60.456 / 30.228 MHz Channel Bandwidth
        - 76.8 Msps Sample Rate / 30.72 MHz Channel Bandwidth
        - 122.88 Msps Sample Rate / 72 / 64 / 61.44 MHz Channel Bandwidth
        - 153.6 Msps Sample Rate / 100 MHz Channel Bandwidth
        - 245.76 Msps Sample Rate / 100 MHz Channel Bandwidth
        - 250 Msps Sample Rate / 100 MHz Channel Bandwidth
      = Available in v4.12.0 and later
        - 200 Msps Sample Rate / 164 MHz Channel Bandwidth

    C1/D1
      = Available in v4.11.1 and later
        - 491.52 Msps / 400 / 450 MHz Channel Bandwidth
        - 500 Msps / 400 / 450 MHz Channel Bandwidth

    Decimated handles (only on A1/A2)
      = Available in libsidekiq v4.10.0 and FPGA bitstream v3.12.0 and later
        - Decimation by 2, 4, 8, 16, 32, and 64 supported by default, these
          decimation rates apply to all above supported sample rates

  === TX Sample Rates ===

    Only the following TX sample rate / channel bandwidths are supported.
    Streaming transmit samples over PCIe is only available for single channel
    transmit on A1 or dual channel transmit on handle pairs A1/A2 or A1/B1.
    Alternatively, all transmit handles (A1/A2/B1/B2) can use FPGA BRAM (see
    Transmit for B1/B2 below for more details).

    A1/A2/B1/B2 (PCIe streaming only for A1/A2)
      = Available in v4.9.0 and later
        - 61.44 Msps Sample Rate / 50 MHz Channel Bandwidth
        - 100 Msps Sample Rate / 82 MHz Channel Bandwidth
        - 122.88 Msps Sample Rate / 100 MHz Channel Bandwidth
        - 245.76 Msps Sample Rate / 200 MHz Channel Bandwidth
      = Available in v4.10.0 and later
        - 250 Msps Sample Rate / 200 MHz Channel Bandwidth
      = Available in v4.10.1 and later
        - 50 Msps Sample Rate / 41 MHz Channel Bandwidth
      = Available in v4.11.1 and later
        - 50 Msps / 20 MHz Channel Bandwidth
        - 61.44 Msps Sample Rate / 25 MHz Channel Bandwidth
        - 73.728 Msps Sample Rate / 60.456 / 30.228 MHz Channel Bandwidth
        - 76.8 Msps Sample Rate / 30.72 MHz Channel Bandwidth
        - 122.88 Msps Sample Rate / 72 / 64 / 61.44 MHz Channel Bandwidth
        - 153.6 Msps Sample Rate / 100 MHz Channel Bandwidth
        - 245.76 Msps Sample Rate / 100 MHz Channel Bandwidth
        - 250 Msps Sample Rate / 100 MHz Channel Bandwidth
      = Available in v4.12.0 and later (requires FPGA v3.13.0 or later)
        - 200 Msps Sample Rate / 164 MHz Channel Bandwidth
        - 491.52 Msps / 400 / 450 MHz Channel Bandwidth
        - 500 Msps / 400 / 450 MHz Channel Bandwidth

    Note: Additional rates may be available, contact Epiq Solutions for details.

  === Initialization Duration ===

    The procedure to initialize the hardware is slower than desired.

  === Transmit for B1/B2 ===

    In releases previous to v4.10.0, transmit on B1/B2 can only be used with a
    TX test tone.

    Starting in FPGA bitstream v3.12.0 and libsidekiq v4.10.0,
    transmit on B1/B2 through the FPGA user app (see tx_samples_from_FPGA_RAM.c
    example code) is supported.

    Starting in FPGA bitstream v3.13.0 and libsidekiq v4.13.0, these transmit
    streaming modes are supported:
     - skiq_chan_mode_single:
       - TxA1 only
     - skiq_chan_mode_dual:
       - TxA1 and TxA2
       - TxA1 and TxB1

  === Transmit Calibration ===

    Transmit calibration does not operate with sample rates <= 61.44Msps.  This
    may result in a less than optimal image and DC offset when operating at
    sample rates at or below this limitation.

    Transmit QEC calibration can take up to 24 seconds between 5GHz and 5.5GHz.

  === LO Tuning Resolution ===

    Currently, the LO can only be tuned to 1 kHz boundaries.  If a frequency on
    a non-1 kHz boundary is requested, the actual configured frequency will be
    automatically adjusted to ensure that it falls within the 1 kHz resolution
    required.


Limited Sidekiq Stretch Capabilities
------------------------------------

  === All hosts ===
   - Configurable Rx pre-select filter selection limited in software
   - FPGA partial reconfiguration limited to .bit files


Limited Sidekiq NV100 Capabilities
------------------------------------

  === Sample Rates ===
   - The sample rates supported for both receive and transmit are (in Msps):
     0.541667, 1.92, 2.4576, 2.8, 3.84, 4, 4.9152, 5.6, 7.68, 9.8304, 10, 11.2,
     15.36, 16, 20, 21.6667, 22, 23.04, 30.72, 40, 61.44

  === Differing sample rates ===
   - NV100 supports dissimilar rates, where the receivers and transmitters can be
     configured at different rates. The following are supported:
      - Receive rate 1.4 Msps, transmit rate 5.6 Msps
      - Receive rate 11.2 Msps, transmit rate 5.6 Msps

  === RF Receive Bandwidth ===
   - RF receive bandwidth is configurable per-handle between 5% and 80% of the
     sample rate in 0.5% increments (for both standard and disparate rates).
   - Outside of this configuration range, the following percentages of RF
     receive bandwidth are also available: 3%, 86%, 89%, 95%, 96%, 99%

Any comments and questions can be posted to the support forums at
https://www.epiqsolutions.com/support

Software Errata
---------------
For an up-to-date listing of the Software Errata, please visit:

    https://support.epiqsolutions.com/viewtopic.php?f=115&t=2536


Errata SW1
==========

Issue Description:
 The HTG-K800 FMC carrier's flash contents may fail to program successfully
 (~7%). When the flash fails to program in this manner, the user bitstream will
 subsequently fail to configure the FPGA and the fallback FPGA bitstream is
 ignored. If a user encounters this issue, currently the only means of recovery
 is to overwrite the carrier card flash contents using the JTAG pod.

Affected Product(s):
 o Sidekiq X2 PDK (Thunderbolt 3 Chassis)

Affected API Function(s):
 o skiq_save_fpga_config_to_flash

Impact Version(s):
 o libsidekiq v4.2.x
 o libsidekiq v4.4.x
 o libsidekiq v4.6.x

Resolution:
 When using the store_user_fpga, use the --verify command line option to verify
 the bitstream is correct after programming it to flash. In the software API,
 use the skiq_verify_fpga_config_from_flash() function after using
 skiq_save_fpga_config_to_flash() and check that the return code of
 skiq_verify_fpga_config_from_flash() is 0.

Fixed in:
 libsidekiq v4.7.0 is slated to fix this intermittent flash erase / programming
 behavior. However, as with any unattended update procedure, it is still
 recommended that the verification step be performed where critical operations
 are in use.


Errata SW2
==========

Issue Description:
 The expected locations of the fallback (aka golden) and user bitstreams have
 been updated in the HTG-K800 FMC carrier card's flash starting in libsidekiq
 v4.7.0 and later.

Affected Product(s):
 o Sidekiq X2 PDK (Thunderbolt 3 Chassis)

Affected API Function(s):
 o skiq_save_fpga_config_to_flash

Impact Version(s):
 o libsidekiq v4.2.x
 o libsidekiq v4.4.x
 o libsidekiq v4.6.x

Resolution:
 In order to better support FPGA bitstream fallback and to support full-sized
 FPGA bitstreams, it was necessary to change the locations of the fallback and
 user bitstreams.

 The sidekiq_hardware_updater_for_v4.7.0.sh script will take care of flashing
 the fallback and user bitstreams to their new locations (only if needed).

 After the bitstreams are relocated, if an application compiled against an
 impacted version (see above) of libsidekiq uses
 skiq_save_fpga_config_to_flash(), an error will be emitted stating that "golden
 FPGA not present, cannot update user FPGA image".  A user must use libsidekiq
 v4.7.0 or later in conjunction with relocated bitstreams in order to store a
 user bitstream to non-volatile memory.

 If a user wishes to revert the relocation, they may run a
 sidekiq_hardware_updater from a release prior to v4.7.0.


Errata SW3
==========

Issue Description:
 A flash transaction on the HTG-K800 FMC carrier can fail intermittently and
 without detection. A user may see the warning message "Quad SPI operation bit
 needed assertion" emitted, however the software does not react correctly to
 address the warning.

Affected Product(s):
 o Sidekiq X2 PDK (Thunderbolt 3 Chassis)
 o Sidekiq X4 PDK (Thunderbolt 3 Chassis) and Sidekiq X4 PCIe Blade

Affected API Function(s):
 o skiq_save_fpga_config_to_flash
 o skiq_verify_fpga_config_from_flash

Affected Test Application(s) / Utilities:
 o sidekiq_hardware_updater.sh
 o store_user_fpga

Impact Version(s):
 o libsidekiq v4.2.x
 o libsidekiq v4.4.x
 o libsidekiq v4.6.x
 o libsidekiq v4.7.x
 o libsidekiq v4.8.x
 o libsidekiq v4.9.0 through v4.9.3

Resolution:
 Do not use the sidekiq_hardware_updater.sh utility prior to v4.9.4 to upgrade
 or downgrade an X2 PDK or X4 PDK system. Use the updaters and installers from
 the System Update v4.9.4 20190503 or later

 Do not use the store_user_fpga test application prior to v4.9.4 on an X2 PDK or
 X4 PDK system. Use the store_user_fpga test application from the v4.9.4 SDK or
 the run-time/complete installer from the System Update v4.9.4 20190503

 If your application uses either of the affected API functions, you *must*
 update to libsidekiq SDK v4.9.4 or there is a risk of incorrect flash behavior
 (corruption).

Errata SW4
==========

Issue Description:
 Memory allocated for skiq_tx_block_t may not be aligned on a memory page
 boundary.  When a misaligned skiq_tx_block_t is transferred to the FPGA,
 it is mishandled, resulting in corrupt samples inadvertently added to
 the sample block being transmitted.

Affected Product(s):
 o Sidekiq X4 HTG platforms with 8 lane PCIe support (HTG-K800, HTG-K810)
 o FPGA bitstreams >= v3.14.1

Affected API Function(s) / Macros:
 o skiq_tx_block_allocate, skiq_tx_block_allocate_by_bytes
 o SKIQ_TX_BLOCK_INITIALIZER
 o SKIQ_TX_BLOCK_INITIALIZER_BY_BYTES, SKIQ_TX_BLOCK_INITIALIZER_BY_WORDS

Affected Test Application(s) / Utilities:
 o tx_samples, tx_benchmark, xcv_benchmark

Impact Version(s):
 o libsidekiq v4.7.0 through v4.16.2

Resolution:
 Libsidekiq v4.17.0 will page align memory when allocating for Tx blocks.
 By default, a page size of 4K is used, but can be overridden by #defining
 SKIQ_TX_BLOCK_MEMORY_ALIGN.  If an application uses any of the affected
 API functions, it *must* be updated to libsidekiq SDK v4.17.0 or there is
 a risk of incorrect data being transmitted by the FPGA.

Errata SW5
==========

Issue Description:
 When transmitting on timestamps, four samples are erroneously sent when the
 packet is received, regardless of the specified timestamp.  This can be four
 samples from a previous packet, or samples from the current packet.

Affected Product(s):
 o Sidekiq X2 PDK (Thunderbolt 3 Chassis)
 o Sidekiq X4 PDK (Thunderbolt 3 Chassis) and Sidekiq X4 PCIe Blade

Affected Test Application(s) / Utilities:
 o tx_samples, tx_samples_async, fdd_rx_tx_samples, tdd_rx_tx_samples

Impact Version(s):
 o FPGA bitstreams version v3.14.1 through v3.15.1

Resolution:
 If transmit on timestamp is utilized, please use a FPGA bitstream
 with a version prior to v3.14.1 when this defect was introduced, or
 version v3.16.1 and later when the issue was resolved.
