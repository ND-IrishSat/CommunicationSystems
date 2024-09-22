/*! \file sidekiq_probe.c
 * \brief This file contains a basic application that reads and prints the
 * serial number and/or form factor for each available card.
 *
 * <pre>
 * Copyright 2017 - 2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

/**
    @todo   Should there be a format identifier for if specific transport
            types are available?
*/
/** @todo   Add format specifier for if Dropkiq is present. */


#include <sidekiq_api.h>
#include <stdio.h>

#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>

#include <arg_parser.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

/** @brief  The format specifier character; this should not be a backslash. */
#ifndef FORMAT_SPECIFIER
#   define FORMAT_SPECIFIER '%'
#endif
#if FORMAT_SPECIFIER == '\\'
#   error "Format specifier cannot be backslash!"
#endif

/** @brief  The default size to allocate (and grow with) for a StringBuffer */
#ifndef DEFAULT_STRING_SIZE
#   define DEFAULT_STRING_SIZE  (1024)
#endif

/** @brief  Static initializer for StringBuffers. */
#define DEFAULT_STRINGBUFFER_INITIALIZER    { NULL, 0, 0 }

/** @brief  The default formatting string to use if one is not specified. */
#ifndef DEFAULT_FMT_STRING
#   define DEFAULT_FMT_STRING "%s\n"
#endif
const char *defaultFmtString = DEFAULT_FMT_STRING;

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "- obtain card information";
static const char* p_help_long =
"Choose and select information about one or more Sidekiq cards.\n"
"Using the '--fmtstring' option, a format string is specified that defines\n"
"the output format. The following format specifiers can be used:\n"
"    %a     Card availability ('y' or 'n')\n"
"    %A     Card availability (0 if unused, else the PID of program using the\n"
"                              specified card)\n"
"    %c     Card ID\n"
"    %C     Accelerometer is present ('y' or 'n')\n"
"    %d     FPGA build date (YYMMDDHH)\n"
"    %f     Firmware version number (MAJOR.MINOR)\n"
"    %F     FPGA bitstream version number (MAJOR.MINOR.PATCH)\n"
"    %G     FPGA device\n"
"    %h     FPGA githash\n"
"    %H     Handle information\n"
"    %l     libsidekiq version number (MAJOR.MINOR.PATCH-LABEL)\n"
"    %m     The metadata values for every valid flash slot\n"
"    %m{N}  The metadata for a specified flash slot N\n"
"    %n     The number of flash slots available\n"
"    %M     FMC carrier (if applicable)\n"
"    %o     GPSDO support is present ('y' or 'n')\n"
"    %O     GPSDO support is present (descriptive string)\n"
"    %p     Part number (numeric)\n"
"    %P     Part name (string)\n"
"    %r     Revision\n"
"    %s     Serial number\n"
"    %v     Variant\n"
"    %t     Transport\n"
"\n"
"For example, \"%c %s %P\" might display the string \"0 12345 mPCIe\". Please\n"
"note that if a value cannot be read, most fields default to a zero value.\n"
"\n"
"If not specified, the default format string is " xstr(DEFAULT_FMT_STRING) ".\n"
"\n"
"Please note that the '-F' ('--form-factor') option provides backwards\n"
"compatibility and will ignore all other options if specified.\n"
"\n"
"If a flash slot number is not specified for the '%m' parameter, it will attempt\n"
"to read the metadata from all valid flash slots; the output format in this case\n"
"will take the format:\n"
"    <number of slots>,<metadata slot 0>, ... <metadata slot N>]\n"
"For example: \"3,12345,AEBCDEF2,2\" gives the metadata for each of the 3 available\n"
"flash slots.\n";

/* command line argument variables */
static uint8_t card = UINT8_MAX;
static char* p_serial = NULL;
static char* p_fmtString = NULL;
static bool display_serial_number = false;
static bool display_library_version = false;
static bool display_hardware_version = false;
static bool display_bitstream_version = false;
static bool display_firmware_version = false;
static bool display_form_factor_legacy = false;
static bool init_full = false;

/* Rx and Tx handle string names */
static const char *rxHandles[] =
{ "RxA1", "RxA2", "RxB1", "RxB2", "RxC1", "RxD1", "Unknown" };
static uint32_t rxHandlesLen = sizeof(rxHandles) / sizeof(rxHandles[0]);

static const char *txHandles[] =
    { "TxA1", "TxA2", "TxB1", "TxB2", "Unknown" };
static uint32_t txHandlesLen = sizeof(txHandles) / sizeof(txHandles[0]);

/**
    @brief  The PID of this program; this is used when checking if a card is
            available.
*/
static pid_t currentPid = 0;

/* the command line arguments available to this application */
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
                "SERIAL",
                &p_serial,
                STRING_VAR_TYPE),
    APP_ARG_OPT("fmtstring",
                0,
                "Specify the output format string; this overrides other"
                " display options (such as --display-serial)",
                "FMT",
                &p_fmtString,
                STRING_VAR_TYPE),
    APP_ARG_OPT("display-serial",
                0,
                "Display the Sidekiq's serial number",
                NULL,
                &display_serial_number,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("display-library",
                0,
                "Display the version of libsidekiq",
                NULL,
                &display_library_version,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("display-hwver",
                0,
                "Display the Sidekiq's hardware version",
                NULL,
                &display_hardware_version,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("display-bitstream",
                0,
                "Display the Sidekiq's bitstream version",
                NULL,
                &display_bitstream_version,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("display-fwver",
                0,
                "Display the Sidekiq's firmware version",
                NULL,
                &display_firmware_version,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("form-factor",
                'F',
                "Display form-factor of the card specified by serial number or all cards",
                NULL,
                &display_form_factor_legacy,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("full",
                0,
                "Fully initialize sidekiq prior to probe",
                NULL,
                &init_full,
                BOOL_VAR_TYPE),
    APP_ARG_TERMINATOR,
};


typedef struct StringBuffer
{
    /** @brief  Buffer to store data. */
    char *buffer;
    /** @brief  Length of buffer (in bytes). */
    uint32_t allocated;
    /** @brief  Next write index. */
    uint32_t writeIdx;
} StringBuffer;


/**************************************************************************************************/
static const char *
fpga_device_cstr( skiq_fpga_device_t fpga_device )
{
    const char* p_fpga_device_str =
        (fpga_device == skiq_fpga_device_xc6slx45t) ? "xc6slx54t" :
        (fpga_device == skiq_fpga_device_xc7a50t) ? "xc7a50t" :
        (fpga_device == skiq_fpga_device_xc7z010) ? "xc7z010" :
        (fpga_device == skiq_fpga_device_xcku060) ? "xcku060" :
        (fpga_device == skiq_fpga_device_xcku115) ? "xcku115" :
        (fpga_device == skiq_fpga_device_xczu3eg) ? "xczu3eg" :
        "unknown";

    return p_fpga_device_str;
}


/**************************************************************************************************/
static const char *
fmc_carrier_cstr( skiq_fmc_carrier_t fmc_carrier )
{
    const char* p_fmc_carrier_str =
        (fmc_carrier == skiq_fmc_carrier_not_applicable) ? "not_applicable" :
        (fmc_carrier == skiq_fmc_carrier_ams_wb3xzd) ? "ams_wb3xzd" :
        (fmc_carrier == skiq_fmc_carrier_ams_wb3xbm) ? "ams_wb3xbm" :
        (fmc_carrier == skiq_fmc_carrier_htg_k800) ? "htg_k800" :
        (fmc_carrier == skiq_fmc_carrier_htg_k810) ? "htg_k810" :
        "unknown";

    return p_fmc_carrier_str;
}

/**************************************************************************************************/
static const char *
xport_type_cstr( skiq_xport_type_t type )
{
    const char* p_xport_type_str =
        (type == skiq_xport_type_pcie) ? "pcie" :
        (type == skiq_xport_type_usb) ? "usb" :
        (type == skiq_xport_type_custom) ? "custom" :
        (type == skiq_xport_type_net) ? "network" :
        "unknown";

    return p_xport_type_str;
}

/**************************************************************************************************/
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

/**************************************************************************************************/
static const char *
rxHdlToString(skiq_rx_hdl_t hdl)
{
    return hdlToString((const char **) rxHandles, rxHandlesLen, (uint32_t) hdl);
}

/**************************************************************************************************/
static const char *
txHdlToString(skiq_tx_hdl_t hdl)
{
    return hdlToString((const char **) txHandles, txHandlesLen, (uint32_t) hdl);
}

/**
    @brief  Clean up a StringBuffer.

    @param[in]  pBuf        The StringBuffer to clean up.
*/
static void
stringBuffer_done(StringBuffer *pBuf)
{
    if (NULL != pBuf)
    {
        free(pBuf->buffer);
        pBuf->buffer = NULL;
        pBuf->allocated = 0;
        pBuf->writeIdx = 0;
    }
}

/**
    @brief  Initialize a StringBuffer.

    @param[in]  pBuf                The StringBuffer to initialize; this should
                                    not be NULL.
    @param[in]  initialAllocate     The number of bytes to initially allocate;
                                    if 0, no bytes are allocated.

    @note   This function assumes that pBuf hasn't been already used; if an
            already used StringBuffer is provided, the original memory buffer
            will leak.

    @return true if the initialization was successful, else false.
*/
static bool
stringBuffer_init(StringBuffer *pBuf, uint32_t initialAllocate)
{
    if (NULL == pBuf)
    {
        return true;
    }

    if (0 != initialAllocate)
    {
        pBuf->buffer = calloc((size_t) initialAllocate, sizeof(char));
        if (NULL == pBuf->buffer)
        {
            return false;
        }
        pBuf->allocated = initialAllocate;
    }
    else
    {
        pBuf->buffer = NULL;
        pBuf->allocated = 0;
    }

    pBuf->writeIdx = 0;

    return true;
}

/**
    @brief  Increase the buffer space available in a StringBuffer.

    @param[in]  pBuf        The StringBuffer to increase; this should not be
                            NULL.
    @param[in]  addSize     The number of bytes to increase the size by.

    @return false if the memory couldn't be allocated or the new size exceeds
            the storage space of the 'allocated' field, else true.
*/
static bool
stringBuffer_grow(StringBuffer *pBuf, uint32_t addSize)
{
    char *tmpBuffer = NULL;
    uint64_t calcSize = 0;

    if (NULL == pBuf)
    {
        return true;
    }

    /* Verify that the new size won't overflow the 'allocated' field. */
    calcSize = pBuf->allocated + addSize;
    if (UINT32_MAX < calcSize)
    {
        return false;
    }

    tmpBuffer = realloc((void *) pBuf->buffer,
                    (size_t) (pBuf->allocated + addSize));
    if (NULL == tmpBuffer)
    {
        return false;
    }
    pBuf->buffer = tmpBuffer;
    pBuf->allocated += addSize;

    return true;
}

/**
    @brief  Append text to a StringBuffer.

    @param[in]  pBuf        The StringBuffer to append text to; this should
                            not be NULL.
    @param[in]  appendText  The text to append to the StringBuffer.
    @param[in]  appendLen   The length of @ref appendText in bytes.

    @note   This function will initialize the StringBuffer if it hasn't
            already been.
    @note   This function will grow the StringBuffer by DEFAULT_STRING_SIZE
            if needed.

    @return The number of bytes appended to the StringBuffer, else an error
            code.
    @retval 0       if the StringBuffer was NULL.
    @retval -1      if the StringBuffer couldn't be allocated and/or grown.
*/
static int32_t
stringBuffer_append(StringBuffer *pBuf, const char *appendText,
        uint32_t appendLen)
{
    bool success = false;

    if (NULL == pBuf)
    {
        return 0;
    }

    if (NULL == pBuf->buffer)
    {
        success = stringBuffer_init(pBuf, DEFAULT_STRING_SIZE);
        if (!success)
        {
            return -1;
        }
    }

    if (pBuf->allocated < (pBuf->writeIdx + appendLen + 1))
    {
        success = stringBuffer_grow(pBuf, DEFAULT_STRING_SIZE);
        if (!success)
        {
            return -1;
        }
    }

    memcpy((void *) &(pBuf->buffer[pBuf->writeIdx]),
           (const void *) appendText,
           (size_t) appendLen);

    pBuf->writeIdx += appendLen;

    return appendLen;
}

/**************************************************************************************************/
static int32_t
get_handle_info(char *string, size_t string_size,
                skiq_param_t *p_param, uint8_t cardId)
{
// Length of the string of a single handle: i.e. RxB1,
#define HDL_STR_LEN 5
    int32_t j, num_char_written;
    uint32_t str_idx = 0;

    for( j = 0; j < p_param->rf_param.num_rx_channels; j++ )
    {
        skiq_rx_hdl_t rx_hdl = p_param->rf_param.rx_handles[j];
        num_char_written = snprintf(&string[str_idx], string_size - str_idx, "%s,", rxHdlToString(rx_hdl));
        if (num_char_written != HDL_STR_LEN)
        {
            return -1;
        }
        str_idx += num_char_written;
    }

    for( j = 0; j < p_param->rf_param.num_tx_channels; j++ )
    {
        skiq_tx_hdl_t tx_hdl = p_param->rf_param.tx_handles[j];
        num_char_written = snprintf(&string[str_idx], string_size - str_idx, "%s,", txHdlToString(tx_hdl));
        if (num_char_written != HDL_STR_LEN)
        {
            return -1;
        }
        str_idx += num_char_written;
    }

    // Replace last comma with a null byte
    if (str_idx != 0)
    {
        string[str_idx-1] = '\0';
    }
    else
    {
        // If the string is empty
        string[0] = '\0';
    }

    return 0;
#undef HDL_STR_LEN
}


/**
    @brief  Process a format string to generate an output string.

    @param[in]  cardId      The card ID to use when gathering information.
    @param[in]  available   If the card is currently available (true) or in
                            use (false).
    @param[in]  fmtString   The format string used to generate the output
                            string. No output will be generated if NULL.
    @param[out] outStr      The buffer to copy the output string into. If NULL,
                            no output will be generated, though the return code
                            will indicate the number of bytes needed for the
                            output string.
    @param[in]  outStrLen   The length of @ref outStr in bytes.

    @note   Unrecognized format specifiers in the format string will be placed
            as is into the output string (e.g. if "%z" isn't a valid format
            specifier, it will be placed as "%z" into the output string).

    @return The length of the generated output string on success, else an
            error code.
    @retval -1      One of the internal functions produced an error.
    @retval -2      The parser got into an invalid state.
*/
static int32_t
formatString(uint8_t cardId, bool available, const char *fmtString,
        char *outStr, uint32_t outStrLen)
{
#define ARG_BUFFER_SIZE     (1024)
    bool success = true;
    enum ParseState
    {
        NORMAL,
        FOUND_BACKSLASH,
        FOUND_FORMAT_SPECIFIER,
    } state = NORMAL;
    size_t i = 0;
    size_t offset = 0;
    StringBuffer outBuffer = DEFAULT_STRINGBUFFER_INITIALIZER;
    size_t toCopy = 0;
    int32_t result = 0;
    char argBuffer[ARG_BUFFER_SIZE];

    pid_t owner = 0;
    uint8_t libskiq_major = 0;
    uint8_t libskiq_minor = 0;
    uint8_t libskiq_patch = 0;
    const char *libskiq_label;
    skiq_part_t part_type = skiq_part_invalid;
    skiq_param_t radioParams;
    int paramsResult = 0;
    bool readPartInfo = false;
    char part_num[SKIQ_PART_NUM_STRLEN] = { '\0' };
    char revision[SKIQ_REVISION_STRLEN] = { '\0' };
    char variant[SKIQ_VARIANT_STRLEN] = { '\0' };
    bool slotSpecified = false;
    uint8_t numFlashSlots = 0;
    uint8_t j = 0;
    uint8_t flashSlot = 0;
    uint64_t flashSlotMetadata = 0;
    char *endPtr;
    long value = 0;
    skiq_gpsdo_support_t gpsdo_support = skiq_gpsdo_support_unknown;

    if (NULL == fmtString)
    {
        return 0;
    }

    success = stringBuffer_init(&outBuffer, DEFAULT_STRING_SIZE);
    if (!success)
    {
        return -1;
    }

    if (NULL != outStr)
    {
        memset((void *) outStr, 0, (size_t) outStrLen);
    }

    memset((void *) &radioParams, 0, sizeof(radioParams));

    paramsResult = skiq_read_parameters(cardId, &radioParams);
    if (0 != paramsResult)
    {
        fprintf(stderr, "Error: failed to get radio parameters for card %"
                PRIu8 " (status code %" PRIi32 ") - filling in parameters"
                " with placeholder empty values.\n", cardId, paramsResult);

        /* Attempt to at least get the hardware information from the API. */
        result = skiq_read_part_info(cardId, part_num, revision, variant);
        if (0 != result)
        {
            fprintf(stderr, "Error: failed to read part information on card"
                    " %" PRIu8 " (status code %" PRIi32 ")\n", cardId, result);
        }
        else
        {
            readPartInfo = true;
        }
    }

    /**
        @todo   Should this parsing operation be two pass?
                First pass:
                    - Validate specifiers
                    - Gather the needed radio information

                Second pass:
                    - Format the output string, using the specifiers and
                      the radio information gathered above.

                This would be cleaner & easier to follow.
    */

    for (i = 0; i < strlen(fmtString) && success; i++)
    {
        switch(state)
        {
        case NORMAL:
            if (FORMAT_SPECIFIER == fmtString[i])
            {
                state = FOUND_FORMAT_SPECIFIER;
            }
            else if ('\\' == fmtString[i])
            {
                state = FOUND_BACKSLASH;
            }
            else
            {
                result = stringBuffer_append(&outBuffer, &(fmtString[i]), 1);
            }
            break;
        case FOUND_BACKSLASH:
            switch(fmtString[i])
            {
            case '\\':
                argBuffer[0] = '\\';
                break;
            case 'b':
                argBuffer[0] = '\b';
                break;
            case 'n':
                argBuffer[0] = '\n';
                break;
            case 'r':
                argBuffer[0] = '\r';
                break;
            case 't':
                argBuffer[0] = '\t';
                break;
            default:
                argBuffer[0] = fmtString[i];
                break;
            }
            result = stringBuffer_append(&outBuffer, argBuffer, 1);
            state = NORMAL;
            break;
        case FOUND_FORMAT_SPECIFIER:
            switch(fmtString[i])
            {
            case FORMAT_SPECIFIER:
                argBuffer[0] = FORMAT_SPECIFIER;
                result = stringBuffer_append(&outBuffer, argBuffer, 1);
                break;

            case 'a':
                /* Availability string */
                argBuffer[0] = 'y';
                if (!available)
                {
                    result = skiq_is_card_avail(cardId, &owner);
                    if (0 != result)
                    {
                        if (owner != currentPid)
                        {
                            argBuffer[0] = 'n';
                        }
                    }
                }
                result = stringBuffer_append(&outBuffer, argBuffer, 1);
                break;

            case 'A':
                /* Available - PID number. */
                if (available)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u", 0);
                }
                else
                {
                    if (0 != skiq_is_card_avail(cardId, &owner))
                    {
                        if (owner != currentPid)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "%u",
                                    (unsigned int) owner);
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "%u", 0);
                        }
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "%u", 0);
                    }
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'c':
                /* Card ID */
                snprintf(argBuffer, ARG_BUFFER_SIZE, "%d", cardId);
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'C':
                /* Accelerometer is present */
                if ((0 == paramsResult) &&
                    (radioParams.card_param.is_accelerometer_present))
                {
                    argBuffer[0] = 'y';
                }
                else
                {
                    argBuffer[0] = 'n';
                }
                result = stringBuffer_append(&outBuffer, argBuffer, 1);
                break;

            case 'd':
                /* FPGA build date */
                if (0 != paramsResult)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%08" PRIx32, 0);
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%08" PRIx32,
                            radioParams.fpga_param.build_date);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'f':
                /* Firmware version number */
                if (0 != paramsResult)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u.%u", 0, 0);
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u.%u",
                            radioParams.fw_param.version_major,
                            radioParams.fw_param.version_minor);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'F':
                /* FPGA version number */
                if (0 != paramsResult)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u.%u.%u", 0, 0, 0);
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u.%u.%u",
                        radioParams.fpga_param.version_major,
                        radioParams.fpga_param.version_minor,
                        radioParams.fpga_param.version_patch);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'G':
                /* FPGA device */
                if (0 != paramsResult)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                             fpga_device_cstr(radioParams.fpga_param.fpga_device));
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'h':
                /* FPGA githash */
                if (0 != paramsResult)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%08" PRIx32, 0);
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%08" PRIx32,
                            radioParams.fpga_param.git_hash);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'H':
                /* Available handles (Rx and Tx) requires --full initialization */
                if (0 != paramsResult)
                {
                    fprintf(stderr, "Error: failed to get parameters for card %d, errno %d\n",
                            cardId, paramsResult);
                }
                else
                {
                    result = get_handle_info(argBuffer, ARG_BUFFER_SIZE, &radioParams, cardId);
                    if (result != 0)
                    {
                        fprintf(stderr, "Error: failed to get handle info for card %d, errno %d\n",
                                cardId, result);
                    }
                    else
                    {
                        result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));
                    }
                }
                break;

            case 'l':
                /* Libsidekiq version number */
                result = skiq_read_libsidekiq_version(&libskiq_major,
                            &libskiq_minor, &libskiq_patch,
                            &libskiq_label);
                if (0 != result)
                {
                    fprintf(stderr, "Error: failed to get libsidekiq version"
                            " number (status code %" PRIi32 ")\n", result);
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u.%u.%u%s",
                            libskiq_major, libskiq_minor, libskiq_patch,
                            libskiq_label);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'm':
                /* The metadata for flash slot N */
                result = 0;

                if (strlen(fmtString) > (i + 1))
                {
                    /*
                        This format specifier should be just 'm' or 'm{N}'; if there is an opening
                        brace, then assume that the user is trying to specify a flash slot number
                    */
                    if ('{' == fmtString[i+1])
                    {
                        slotSpecified = true;
                    }
                }

                if (slotSpecified)
                {
                    /*
                        Ugly input checking - (i + 1) should be the index of the initial brace;
                        for the input to be valid, there must be at least 2 more characters -
                        at least one digit and the closing brace - hence (i + 1) + 2
                    */
                    if (strlen(fmtString) <= ((i + 1) + 2))
                    {
                        fprintf(stderr, "No flash slot number specified!\n");
                        result = -EFAULT;
                    }
                    else
                    {
                        errno = 0;
                        value = strtoul(&(fmtString[i+2]), &endPtr, 10);
                        if ('}' != *endPtr)
                        {
                            fprintf(stderr, "No closing brace for the flash slot number!\n");
                            result = -EFAULT;
                        }
                        else if (&(fmtString[i+2]) == endPtr)
                        {
                            fprintf(stderr, "No flash slot number specified!\n");
                            result = -EFAULT;
                        }
                        else if (((ULONG_MAX == value) && (ERANGE == errno)) ||
                                 (EINVAL == errno) || (UINT8_MAX < value))
                        {
                            fprintf(stderr, "Invalid flash slot number!\n");
                            result = -EINVAL;
                        }
                    }

                    if (0 == result)
                    {
                        flashSlot = (uint8_t) value;
                        result = skiq_read_fpga_config_flash_slot_metadata(cardId, flashSlot,
                                    &flashSlotMetadata);
                        if (0 != result)
                        {
                            if (-ESRCH == result)
                            {
                                fprintf(stderr, "Failed to get metadata for flash slot %" PRIu8
                                    " - is this a valid flash slot number? (status code %"
                                    PRIi32 ")\n", flashSlot, result);
                            }
                            else
                            {
                                fprintf(stderr, "Failed to get metadata for flash slot %" PRIu8
                                    " (status code %" PRIi32 ")\n", flashSlot, result);
                            }
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "%" PRIx64, flashSlotMetadata);
                        }
                    }

                    if (0 != result)
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "-1");
                    }

                    /*
                        Adjust i to read the next format specifier; (i + 1) is used as the
                        for loop will automatically increment i the next time around the loop.
                    */
                    while ((strlen(fmtString) > (i + 1)) && ('}' != fmtString[i]))
                    {
                        i++;
                    }
                }
                else if (0 == result)
                {
                    result = skiq_read_fpga_config_flash_slots_avail(cardId, &numFlashSlots);
                    if (0 != result)
                    {
                        fprintf(stderr, "Failed to read the number of available flash slots"
                            " (status code %" PRIi32 ")\n", result);
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "-1");
                    }
                    else
                    {
                        offset = 0;
                        offset = snprintf(&(argBuffer[offset]), ARG_BUFFER_SIZE, "%" PRIu8,
                                    numFlashSlots);
                        for (j = 0; j < numFlashSlots; j++)
                        {
                            result = skiq_read_fpga_config_flash_slot_metadata(cardId, j,
                                        &flashSlotMetadata);
                            if (0 != result)
                            {
                                fprintf(stderr, "Failed to get metadata for flash slot %" PRIu8
                                    " (status code %" PRIi32 ")\n", j, result);
                                offset += snprintf(&(argBuffer[offset]), ARG_BUFFER_SIZE, ",-1");
                            }
                            else
                            {
                                offset += snprintf(&(argBuffer[offset]), ARG_BUFFER_SIZE, ",%"
                                            PRIx64, flashSlotMetadata);
                            }
                        }
                    }
                }

                result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));

                break;

            case 'M':
                /* FMC carrier */
                if (0 != paramsResult)
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                             fmc_carrier_cstr(radioParams.card_param.part_fmc_carrier));
                }
                result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));
                break;

            case 'n':
                /* The number of flash slots available */
                result = skiq_read_fpga_config_flash_slots_avail(cardId, &numFlashSlots);
                if (0 != result)
                {
                    fprintf(stderr, "Failed to read the number of available flash slots"
                        " (status code %" PRIi32 ")\n", result);
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "-1");
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%u", numFlashSlots);
                }

                result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));
                break;

            case 'o':
            case 'O':
                /* GPSDO availability */
                result = skiq_is_gpsdo_supported(cardId, &gpsdo_support);
                if ('o' == fmtString[i])
                {
                    if ((0 == result) && (skiq_gpsdo_support_is_supported == gpsdo_support))
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "y");
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "n");
                    }
                }
                else
                {
                    if (0 != result)
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "ReadError");
                    }
                    else
                    {
                        switch (gpsdo_support)
                        {
                        case skiq_gpsdo_support_is_supported:
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Available");
                            break;
                        case skiq_gpsdo_support_card_not_supported:
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "CardNotSupported");
                            break;
                        case skiq_gpsdo_support_fpga_not_supported:
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "FpgaNotSupported");
                            break;
                        case skiq_gpsdo_support_not_supported:
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "NotAvailable");
                            break;
                        case skiq_gpsdo_support_unknown:
                        default:
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                            break;
                        }
                    }
                }

                result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));
                break;

            case 'p':
                /* Part number */
                if (0 != paramsResult)
                {
                    if (!readPartInfo)
                    {
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                        }
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "%s", part_num);
                    }
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                            radioParams.card_param.part_info.number_string);
                }
                result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));
                break;

            case 'P':
                /* Part name */
                result = 0;
                if (0 != paramsResult)
                {
                    if (!readPartInfo)
                    {
                        result = -1;
                    }
                    else
                    {
                        if ((0 == strcmp(part_num, SKIQ_PART_NUM_STRING_MPCIE_001)) ||
                            (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_MPCIE_002)))
                        {
                            part_type = skiq_mpcie;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_M2))
                        {
                            part_type = skiq_m2;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_X2))
                        {
                            part_type = skiq_x2;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_Z2))
                        {
                            part_type = skiq_z2;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_X4))
                        {
                            part_type = skiq_x4;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_M2_2280))
                        {
                            part_type = skiq_m2_2280;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_Z2P))
                        {
                            part_type = skiq_z2p;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_Z3U))
                        {
                            part_type = skiq_z3u;
                        }
                        else if (0 == strcmp(part_num, SKIQ_PART_NUM_STRING_NV100))
                        {
                            part_type = skiq_nv100;
                        }
                        else
                        {
                            result = -1;
                        }
                    }
                }
                else
                {
                    part_type = radioParams.card_param.part_type;
                }

                if (skiq_part_invalid != part_type)
                {
                    switch (part_type)
                    {
                    case skiq_mpcie:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "mpcie");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "mPCIe");
                        }
                        break;
                    case skiq_m2:
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "m.2");
                        break;
                    case skiq_x2:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "x2");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "X2");
                        }
                        break;
                    case skiq_z2:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "z2");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Z2");
                        }
                        break;
                    case skiq_x4:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "x4");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "X4");
                        }
                        break;
                    case skiq_m2_2280:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "m.2-2280");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "M.2-2280");
                        }
                        break;
                    case skiq_z2p:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "z2p");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Z2P");
                        }
                        break;
                    case skiq_z3u:
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "z3u");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Z3U");
                        }
                        break;
                    case skiq_nv100:
                        snprintf(argBuffer, ARG_BUFFER_SIZE, display_form_factor_legacy ?
                                 "nv100" : "NV100");
                        break;
                    default:
                        result = -1;
                        break;
                    }
                }
                else
                {
                    result = -1;
                }

                if (0 != result)
                {
                    if (display_form_factor_legacy)
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                    }
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'r':
                /* Revision */
                if (0 != paramsResult)
                {
                    if (!readPartInfo)
                    {
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                        }
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "%s", revision);
                    }
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                            radioParams.card_param.part_info.revision_string);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 's':
                /* Serial number */
                if (0 != paramsResult)
                {
                    /*
                        Couldn't get the serial number from the radio
                        parameters; fall back to the libsidekiq call.
                    */
                    char *p_card_serial_num = NULL;

                    result = skiq_read_serial_string(cardId,
                                &p_card_serial_num);
                    if (0 != result)
                    {
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                        }
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                                p_card_serial_num);
                    }
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                            radioParams.card_param.serial_string);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 'v':
                /* Variant */
                if (0 != paramsResult)
                {
                    if (!readPartInfo)
                    {
                        if (display_form_factor_legacy)
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "unk");
                        }
                        else
                        {
                            snprintf(argBuffer, ARG_BUFFER_SIZE, "Unknown");
                        }
                    }
                    else
                    {
                        snprintf(argBuffer, ARG_BUFFER_SIZE, "%s", variant);
                    }
                }
                else
                {
                    snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                            radioParams.card_param.part_info.variant_string);
                }
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;

            case 't':
                /* Transport */
                snprintf(argBuffer, ARG_BUFFER_SIZE, "%s",
                        xport_type_cstr(radioParams.card_param.xport));
                result = stringBuffer_append(&outBuffer, argBuffer, strlen(argBuffer));
                break;

            default:
                /* Just copy any unrecognized format specifiers. */
                snprintf(argBuffer, ARG_BUFFER_SIZE, "%%%c", fmtString[i]);
                result = stringBuffer_append(&outBuffer, argBuffer,
                            strlen(argBuffer));
                break;
            }
            state = NORMAL;
            break;
        default:
            result = -2;
            break;
        }

        if (0 > result)
        {
            success = false;
        }
    }

    if (NORMAL != state)
    {
        /* Dangling specifier at the end? */
        success = false;
        result = -3;
    }

    if (success)
    {
        outBuffer.buffer[outBuffer.writeIdx] = '\0';

        toCopy = outBuffer.writeIdx;
        if (NULL != outStr)
        {
            if (toCopy > outStrLen)
            {
                toCopy = outStrLen;
            }

            memcpy((void *) outStr, (const void *) outBuffer.buffer, toCopy);
            outStr[toCopy] = '\0';
        }

        result = (int32_t) toCopy;
    }

    stringBuffer_done(&outBuffer);

    return result;
#undef ARG_BUFFER_SIZE
}

/**
    @brief  Build a formatting string from command-line arguments.

    @param[out] outStr              The output string to place the format
                                    string into; no output will be generated
                                    if this NULL.
    @param[in]  outStrLen           The length of @ref outStr in bytes.
    @param[in]  display_serial      If true, display the serial number.
    @param[in]  display_library     If true, display the library version number.
    @param[in]  display_hw          If true, display the hardware version
                                    number.
    @param[in]  display_bitstream   If true, display the FPGA bitstream version.
    @param[in]  display_fw          If true, display the firmware version.
    @param[in]  display_ff_legacy   This is a legacy parameter; if true, it
                                    overrides all other parameters and builds
                                    the format string used by the legacy version
                                    of this app.

    @note   If display_ff_legacy is not set, the card ID is always at the
            beginning of the string, each field will be preceded by a name
            (e.g. "card %c serial %s"), and the string will always end in a
            newline.
    @note   Setting display_ff_legacy will override all other parameters.

    @return The length of the string generated on success, else an error. Note
            that if outStr is NULL, the length of the generated string will be
            returned but no output will be generated.
    @retval -1  A function call failed.
*/
static int32_t
buildFmtStringFromArgs(char *outStr, uint32_t outStrLen,
        bool display_serial, bool display_library, bool display_hw,
        bool display_bitstream, bool display_fw,
        bool display_ff_legacy)
{
    StringBuffer customFmtString = DEFAULT_STRINGBUFFER_INITIALIZER;
    int32_t status = 0;
    bool result = 0;
    size_t toCopy = 0;

    result = stringBuffer_init(&customFmtString, DEFAULT_STRING_SIZE);
    if (!result)
    {
        fprintf(stderr, "Error: failed to allocate memory while"
                " creating formatting string.\n");
        status = -1;
        goto finished;
    }

    if (display_ff_legacy)
    {
        status = stringBuffer_append(&customFmtString, "%s,%P\n", 6);
        if (0 > status)
        {
            goto finished;
        }
    }
    else
    {
        status = stringBuffer_append(&customFmtString, "card %c ", 8);
        if (0 > status)
        {
            goto finished;
        }

        if (display_serial_number)
        {
            status = stringBuffer_append(&customFmtString, "serial %s ", 10);
            if (0 > status)
            {
                goto finished;
            }
        }
        if (display_library_version)
        {
            status = stringBuffer_append(&customFmtString, "library %l ", 11);
            if (0 > status)
            {
                goto finished;
            }
        }
        if (display_hardware_version)
        {
            status = stringBuffer_append(&customFmtString, "hardware %p ", 12);
            if (0 > status)
            {
                goto finished;
            }
        }
        if (display_bitstream_version)
        {
            status = stringBuffer_append(&customFmtString, "bitstream %F ", 13);
            if (0 > status)
            {
                goto finished;
            }
        }
        if (display_firmware_version)
        {
            status = stringBuffer_append(&customFmtString, "firmware %f ", 12);
            if (0 > status)
            {
                goto finished;
            }
        }

        /* This should remove the last space character. */
        customFmtString.buffer[customFmtString.writeIdx] = '\0';

        if (0 != customFmtString.writeIdx)
        {
            status = stringBuffer_append(&customFmtString, "\n", 1);
            if (0 > status)
            {
                goto finished;
            }
        }
    }

    if (NULL != outStr)
    {
        status = customFmtString.writeIdx - 1;
        if ((outStrLen) < customFmtString.writeIdx)
        {
            toCopy = (size_t) outStrLen;
        }
        else
        {
            toCopy = (size_t) customFmtString.writeIdx;
        }

        memcpy((void *) outStr, (const void *) customFmtString.buffer,
                toCopy);
    }

finished:
    stringBuffer_done(&customFmtString);

    return status;
}


/******************************************************************************/
/*
    @param argc-the # of arguments from the cmd line
    @param argv-a vector of ascii string aruments from the cmd line
    @return int-indicating status
*/
int main( int argc, char *argv[] )
{
#define OUTPUT_STRING_LENGTH    (16384)
    int32_t status=0;

    bool skiq_initialized = false;
    pid_t owner = 0;
    uint8_t cards[SKIQ_MAX_NUM_CARDS] = { UINT8_MAX };
    uint8_t num_cards = 0;
    uint8_t availCards[SKIQ_MAX_NUM_CARDS] = { UINT8_MAX };
    uint8_t num_avail_cards = 0;
    uint8_t unavailCards[SKIQ_MAX_NUM_CARDS] = { UINT8_MAX };
    uint8_t num_unavail_cards = 0;
    uint8_t i = 0;

    bool formatting_flags_given = false;

    char outStr[OUTPUT_STRING_LENGTH];
    char customFmtString[OUTPUT_STRING_LENGTH];

    currentPid = getpid();

    status = arg_parser(argc, argv, p_help_short, p_help_long, p_args);
    if( 0 != status )
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return (-1);
    }

    /* disable noisy messages, we just want the info */
    skiq_register_logging(NULL);

    if ((UINT8_MAX != card) && (NULL != p_serial))
    {
        fprintf(stderr, "Error: Must specify EITHER card ID or serial number,"
                " not both\n");
        status = -1;
        goto finished;
    }
    else if (UINT8_MAX != card)
    {
        /** @todo   Handle multiple card IDs with commas? */
        cards[0] = card;
        num_cards = 1;
    }
    else if (NULL != p_serial)
    {
        /** @todo   Handle multiple serial numbers with commas? */
        status = skiq_get_card_from_serial_string( p_serial, &card );
        if ( 0 == status )
        {
            cards[0] = card;
            num_cards = 1;
        }
        else
        {
            fprintf(stderr, "Error: unable to find Sidekiq with serial number"
                    " %s\n", p_serial);
            status = -1;
            goto finished;
        }
    }
    else
    {
        /* determine how many Sidekiqs are there and their card IDs */
        skiq_get_cards( skiq_xport_type_auto, &num_cards, cards );
    }

    /* no harm, no foul, no cards detected */
    if ( 0 == num_cards )
    {
        return (0);
    }

    /*
        Triage all of the specified cards; reject the ones out of valid range,
        and attempt to sort out the available and unavailable cards.
    */
    for (i = 0; i < num_cards; i++)
    {
        if (SKIQ_MAX_NUM_CARDS < cards[i])
        {
            fprintf(stderr, "Error: invalid card ID %" PRIu8 "\n", cards[i]);
            status = -1;
            goto finished;
        }

        status = skiq_is_card_avail(cards[i], &owner);
        if (((0 != status) && (currentPid == owner)) || (0 == status))
        {
            availCards[num_avail_cards] = cards[i];
            num_avail_cards++;
        }
        else
        {
            unavailCards[num_unavail_cards] = cards[i];
            num_unavail_cards++;
        }
    }

    // set init level according to whether or not --full is given
    skiq_xport_init_level_t level = skiq_xport_init_level_basic;
    if (init_full)
    {
        level = skiq_xport_init_level_full;
    }


    /** @todo   Handle already locked cards. */

    if (0 < num_avail_cards)
    {
        /* bring up the USB and PCIe interfaces for all the cards */
        status = skiq_init( skiq_xport_type_auto, level,
                    availCards, num_avail_cards);
        if( status != 0 )
        {
            if (-EINVAL == status)
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

            if (0 < status)
            {
                status = -status;
            }

            goto finished;
        }
        skiq_initialized = true;
    }

    formatting_flags_given = \
        display_serial_number || display_library_version || \
        display_hardware_version || display_bitstream_version || \
        display_firmware_version || display_form_factor_legacy;
    if ((NULL != p_fmtString) && formatting_flags_given)
    {
        fprintf(stderr, "INFO: format string specified; ignoring other"
                " formatting options.\n");
    }

    if (NULL == p_fmtString)
    {
        if (formatting_flags_given)
        {
            status = buildFmtStringFromArgs(customFmtString,
                        OUTPUT_STRING_LENGTH, display_serial_number,
                        display_library_version, display_hardware_version,
                        display_bitstream_version, display_firmware_version,
                        display_form_factor_legacy);
            if (0 > status)
            {
                fprintf(stderr, "Error: failed to build formatting string"
                        " (status code %" PRIi32 ")\n", status);
                goto finished;
            }

            p_fmtString = (char *) &(customFmtString[0]);
        }
        else
        {
            p_fmtString = (char *) &(defaultFmtString[0]);
        }
    }

    /** @todo   Display all cards specified, in card ID order. */

    /* Loop through all of the available cards first. */
    for(i = 0; i < num_avail_cards; i++)
    {
        status = formatString(availCards[i], true, p_fmtString,
                    &(outStr[0]), OUTPUT_STRING_LENGTH);
        if (-1 == status)
        {
            fprintf(stderr, "Error: could not format entry for available"
                    " card %" PRIu8 " (status code %" PRIi32 ")\n",
                    availCards[i], status);
            goto finished;
        }
        else
        {
            printf("%s", outStr);
            status = 0;
        }
    }
    for (i = 0; i < num_unavail_cards; i++)
    {
        status = formatString(unavailCards[i], false, p_fmtString,
                    &(outStr[0]), OUTPUT_STRING_LENGTH);
        if (-1 == status)
        {
            fprintf(stderr, "Error: could not format entry for unavailable"
                    " card %" PRIu8 " (status code %" PRIi32 ")\n",
                    unavailCards[i], status);
            goto finished;
        }
        else
        {
            printf("%s", outStr);
            status = 0;
        }
    }

finished:
    if (skiq_initialized)
    {
        skiq_exit();
        skiq_initialized = false;
    }

    return status;
#undef OUTPUT_STRING_LENGTH
}

