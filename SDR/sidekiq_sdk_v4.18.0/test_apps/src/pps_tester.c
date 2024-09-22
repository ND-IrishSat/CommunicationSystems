/**
    @file   pps_tester.c
    @brief  libsidekiq test utility that verifies the presence of a 1PPS signal
            on one or more cards.

    <pre>
    Copyright 2019 - 2020 Epiq Solutions, All Rights Reserved
    </pre>
*/

/** @todo   Allow user to specify card serial numbers as well as card numbers? */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "sidekiq_api.h"
#include "arg_parser.h"

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                         str(s)
#define str(s)                          #s

/* The default card(s) to used */
#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER  0
#endif

/* The default runtime for this test (in seconds) */
#ifndef DEFAULT_TEST_RUN_TIME_SEC
#   define DEFAULT_TEST_RUN_TIME_SEC        15
#endif

/* The default value to display PPS timestamps when detected ('false' means don't display) */
#ifndef DEFAULT_DISPLAY_TS_FLAG
#   define DEFAULT_DISPLAY_TS_FLAG          false
#endif

/*
    The default value to display a PPS timestamp table at the end of execution ('false' means
    don't display)
*/
#ifndef DEFAULT_DISPLAY_TS_TABLE_FLAG
#   define DEFAULT_DISPLAY_TS_TABLE_FLAG    false
#endif

/* The default 1PPS source (this can be either "external" or "host" */
#ifndef DEFAULT_PPS_SOURCE_STR
#   define DEFAULT_PPS_SOURCE_STR           external
#endif

/* these are used to provide help strings for the application when running it
   with either the "-h" or "--help" flags */
static const char* p_help_short = "Test if one or more Sidekiq cards are receiving PPS signals";
static const char* p_help_long = "\
Over a specified time period, counts the number of received PPS signals on\n\
one or more Sidekiq cards to verify that PPS signals are being received.\n\
\n\
By default, the success threshold is set to one less than the number of\n\
pulses expected for the specified runtime; this is one less just in case\n\
the first pulse is missed.\n\
\n\
Defaults:\n\
    --cards=" xstr(DEFAULT_CARD_NUMBER) "\n\
    --runtime=" xstr(DEFAULT_TEST_RUN_TIME_SEC) "\n\
    --displayts=" xstr(DEFAULT_DISPLAY_TS_FLAG) "\n\
    --displaytstable=" xstr(DEFAULT_DISPLAY_TS_TABLE_FLAG) "\n\
    --source=" xstr(DEFAULT_PPS_SOURCE_STR) "\n\
    --success=(one less than the number of pulses expected for the runtime)\n\
";

/* command line argument variables */
static bool displayTimestamps = DEFAULT_DISPLAY_TS_FLAG;
static bool displayTimestampsTable = DEFAULT_DISPLAY_TS_TABLE_FLAG;
static const char *p_cardListStr = NULL;
static const char *p_ppsSource = xstr(DEFAULT_PPS_SOURCE_STR);
static uint32_t testRunTimeSec = DEFAULT_TEST_RUN_TIME_SEC;
static uint32_t testSuccessThreshold = UINT32_MAX;

static struct application_argument p_args[] =
{
    APP_ARG_OPT("cards",
                0,
                "A comma separated list of card numbers to test",
                "",
                &p_cardListStr,
                STRING_VAR_TYPE),
    APP_ARG_OPT("displayts",
                0,
                "If set, show the system timestamps when PPS signals are detected",
                NULL,
                &displayTimestamps,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("displaytstable",
                0,
                "If set, show a table of received system timestamps when PPS signals were detected",
                NULL,
                &displayTimestampsTable,
                BOOL_VAR_TYPE),
    APP_ARG_OPT("runtime",
                0,
                "The test run time",
                "seconds",
                &testRunTimeSec,
                UINT32_VAR_TYPE),
    APP_ARG_OPT("source",
                0,
                "The PPS input source",
                "[\"external\",\"host\"]",
                &p_ppsSource,
                STRING_VAR_TYPE),
    APP_ARG_OPT("success",
                0,
                "The number of received PPS signals needed to declare a successful test",
                "pulses",
                &testSuccessThreshold,
                UINT32_VAR_TYPE),
    APP_ARG_TERMINATOR,
};

static volatile bool running = true;



/**
    @brief  Display the name of the PPS source

    @param[in]  source  The PPS source

    @return A string version of the PPS source constant
*/
static const char * ppsSourceName(skiq_1pps_source_t source)
{
    return \
        (skiq_1pps_source_unavailable == source) ? "unavailable" :
        (skiq_1pps_source_external == source) ? "external" :
        (skiq_1pps_source_host == source) ? "host" :
        "unknown";
}

/**
    @brief  Count the number of 1PPS pulses received over a given amount of time and list of
            cards.

    @param[in]  p_cardList      A list of libsidekiq card numbers (up to
                                SKIQ_MAX_NUM_CARDS) to test; this must not be
                                NULL.
    @param[in]  numCards        The number of cards specified in @p p_cardList.
    @param[in]  durationSec     The number of seconds to attempt to find a PPS signal
    @param[in]  displayTs       If true, show a message when a PPS signal is detected
    @param[in]  displayTsTable  If true, display a table of the cards & timestamps after
                                execution
    @param[out] p_recvPulses    If not NULL, the number of pulses received per card
                                number; e.g. index 0 in this array represents the number
                                of pulses received on card p_cardList[0]. This array
                                should be at least SKIQ_MAX_NUM_CARDS long.

    @note   This function will run for at least @p durationSec seconds.

    @return 0 on success
*/
static int32_t
countPpsPulses(uint8_t *p_cardList, uint8_t numCards, uint32_t durationSec, bool displayTs,
    bool displayTsTable, uint32_t *p_recvPulses)
{
    int32_t result = 0;
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t card = 0;
    uint64_t rfTs = 0;
    uint64_t sysTs = 0;
    uint64_t lastTimestamp[SKIQ_MAX_NUM_CARDS] = { [0 ... (SKIQ_MAX_NUM_CARDS-1)] = 0, };
    uint32_t pulseCount[SKIQ_MAX_NUM_CARDS] = { [0 ... (SKIQ_MAX_NUM_CARDS-1)] = 0, };
    /* Set the sleep time to 125ms (an eighth of a second). */
    uint32_t sleepTimeUs = 125000;
    /*
        The number of times to check the PPS timestamps; this is multiplied
        by the number of sleeps per second (1000000us/sec / sleepTimeUs) to calculate
        the total number of checks given the desired duration.
    */
    uint64_t checksLeft = (durationSec * (uint32_t) (1000000 / sleepTimeUs));

    /* A raw chunk of memory for timestamp history */
    uint64_t *rawTimestampBuffer = NULL;
    /* A historical list of timestamps per-card (using rawTimestampBuffer as a backing buffer) */
    uint64_t *timestampList[SKIQ_MAX_NUM_CARDS] = { [0 ... (SKIQ_MAX_NUM_CARDS-1)] = NULL, };
    /* The per-card write index for historical timestamp data */
    uint32_t timestampWriteIdx[SKIQ_MAX_NUM_CARDS] = { [0 ... (SKIQ_MAX_NUM_CARDS-1)] = 0, };

    /* Only worry about allocating timestamp historical memory if the "display table flag" is set */
    if (displayTsTable)
    {
        rawTimestampBuffer = calloc(SKIQ_MAX_NUM_CARDS * durationSec, sizeof(uint64_t));
        if (NULL == rawTimestampBuffer)
        {
            fprintf(stderr, "Error: failed to allocate memory for results\n");
            return -ENOMEM;
        }
        for (i = 0; i < numCards; i++)
        {
            timestampList[i] = &(rawTimestampBuffer[(i * durationSec)]);
            for (j = 0; j < durationSec; j++)
            {
                timestampList[i][j] = UINT64_MAX;
            }
        }
    }

    /* Get the initial PPS timestamp result for each card. */
    for (i = 0; (i < numCards) && (running); i++)
    {
        card = p_cardList[i];

        result = skiq_read_last_1pps_timestamp(card, &rfTs, &sysTs);
        if (0 != result)
        {
            fprintf(stderr, "Warning: failed to get initial timestamp for card %" PRIu8
                " (result = %" PRIi32 "); attempting to continue...\n", card, result);
        }

        lastTimestamp[i] = sysTs;
    }

    while ((checksLeft) && (running))
    {
        for (i = 0; i < numCards; i++)
        {
            card = p_cardList[i];

            result = skiq_read_last_1pps_timestamp(card, &rfTs, &sysTs);
            if (0 != result)
            {
                fprintf(stderr, "Warning: failed to get timestamp for card %" PRIu8
                    " (result = %" PRIi32 "); will try again\n", card, result);
            }
            else
            {
                if (sysTs != lastTimestamp[i])
                {
                    pulseCount[i]++;
                    lastTimestamp[i] = sysTs;

                    if (displayTs)
                    {
                        printf("Info: found PPS for card %" PRIu8 " at system"
                            " timestamp %" PRIu64 "\n", card, sysTs);
                    }

                    if (displayTsTable)
                    {
                        timestampList[i][timestampWriteIdx[i]] = sysTs;
                        timestampWriteIdx[i]++;
                    }
                }
            }
        }

        {
            errno = 0;
            int status = usleep((useconds_t) sleepTimeUs);
            if (0 != status)
            {
                fprintf(stderr, "Warning: failed to sleep (result = %" PRIi32 "); attempting"
                    " to continue...\n", status);
            }
        }
        checksLeft--;
    }

    if ((0 < checksLeft) && (!running))
    {
        fprintf(stderr, "Info: received shutdown signal\n");
    }

    /* If the "display table flag" is set, then show a table of the received PPS timestamps */
    if ((running) && (displayTsTable))
    {
        uint32_t k = 0;
        uint64_t ts = 0;

        printf("\nReceived timestamps\n");
        printf("-------------------\n");
        for (i = 0; i < numCards; i++)
        {
            printf("%11s %2" PRIu8, "Card", p_cardList[i]);
        }
        printf("\n\n");

        for (k = 0; k < durationSec; k++)
        {
            for (i = 0; i < numCards; i++)
            {
                ts = timestampList[i][k];

                if (UINT64_MAX == ts)
                {
                    printf("%14s", " ");
                }
                else
                {
                    printf("%14" PRIu64, ts);
                }
            }
            printf("\n");
        }
        printf("\n");
    }

    if (NULL != rawTimestampBuffer)
    {
        free(rawTimestampBuffer);
        rawTimestampBuffer = NULL;
        for (i = 0; i < SKIQ_MAX_NUM_CARDS; i++)
        {
            timestampList[i] = NULL;
        }
    }

    if (NULL != p_recvPulses)
    {
        memcpy(p_recvPulses, &(pulseCount[0]), sizeof(pulseCount));
    }

    return (result);
}

static int32_t
setPpsSources(uint8_t *p_cardList, uint8_t numCards, const char *p_ppsSourceStr)
{
    int32_t result = 0;
    uint8_t i = 0;
    const char *p_src = p_ppsSourceStr;
    skiq_1pps_source_t ppsSource = skiq_1pps_source_unavailable;
    skiq_1pps_source_t readPpsSource = skiq_1pps_source_unavailable;

    if ((0 != strcasecmp(p_ppsSourceStr, "external")) && (0 != strcasecmp(p_ppsSourceStr, "host")))
    {
        fprintf(stderr, "Warning: invalid PPS source '%s' (should be either 'external' or 'host');"
            " using default value '%s'\n", p_ppsSourceStr, xstr(DEFAULT_PPS_SOURCE_STR));
        p_src = xstr(DEFAULT_PPS_SOURCE_STR);
    }

    if (0 == strcasecmp(p_src, "external"))
    {
        fprintf(stderr, "Info: setting PPS source to 'external'...\n");
        ppsSource = skiq_1pps_source_external;
    }
    else if (0 == strcasecmp(p_src, "host"))
    {
        fprintf(stderr, "Info: setting PPS source to 'host'...\n");
        ppsSource = skiq_1pps_source_host;
    }

    if (skiq_1pps_source_unavailable == ppsSource)
    {
        fprintf(stderr, "Error: invalid PPS source type '%s'\n", p_src);
        return -EINVAL;
    }

    for (i = 0; (i < numCards) && (0 == result) && (running); i++)
    {
        result = skiq_write_1pps_source(p_cardList[i], ppsSource);
        if (0 != result)
        {
            fprintf(stderr, "Error: failed to set PPS source on card %" PRIu8 " (result = %"
                PRIi32 ")\n", p_cardList[i], result);
        }
        else
        {
            result = skiq_read_1pps_source(p_cardList[i], &readPpsSource);
            if (0 != result)
            {
                fprintf(stderr, "Warning: failed to read PPS source on card %" PRIu8 " (result = %"
                    PRIi32 "); attempting to continue...\n", p_cardList[i], result);
            }
            else
            {
                if (ppsSource != readPpsSource)
                {
                    fprintf(stderr, "Error: PPS source verification failed; attempted to set to"
                        "'%s' (%" PRIi32 "), read back '%s' (%" PRIi32 ")\n",
                        ppsSourceName(ppsSource), (int32_t) ppsSource,
                        ppsSourceName(readPpsSource), (int32_t) readPpsSource);
                }
            }
            /* Silently allow this function to continue without verification */
            result = 0;
        }
    }

    return result;
}

static void
sigHandler(int signal)
{
    (void) signal;
    running = false;
}

/**
    @brief  Convert a string to an unsigned 8-bit integer.

    This function assumes a base 10 input number.

    @param[in]  str         The string to convert; this should not be NULL
    @param[out] converted   If not NULL, the converted value

    @return true if the conversion suceeded, else false.
*/
bool
strToNumU8(const char *str, uint8_t *converted)
{
    char *endPtr;
    long long int value = 0;

    errno = 0;
    value = strtoll(str, &endPtr, 10);
    if ((0 != errno) || ('\0' != *endPtr))
    {
        return false;
    }

    if ((0 > value) || (UINT8_MAX < value))
    {
        return false;
    }

    *converted = (uint8_t) value;
    return true;
}


/**
    @brief  Parse a list of cards seperated by delimiter(s)

    @param[in]  p_cardListString    The string containing the card list; if NULL or empty,
                                    then the default card number is used
    @param[out] p_cardList          The list of parsed card numbers; this should be at least
                                    SKIQ_MAX_NUM_CARDS elements long and should not be NULL
    @param[out] p_numCards          The number of cards in @p p_cardList; this should not be NULL

    @return 0 on success, else an errno
    @retval ENOMEM  if memory allocation failed
    @retval EINVAL  if one or more of the card numbers in @p p_cardListString are invalid or
                    cannot be parsed
*/
static int32_t
parseCardList(const char *p_cardListString, uint8_t *p_cardList, uint8_t *p_numCards)
{
    int32_t result = 0;
    const char *p_delimiters = ",;";
    uint8_t i = 0;
    bool success = false;
    bool found = false;
    uint8_t cardNum = 0;
    uint8_t numCards = 0;

    char *dup = NULL;
    char *p_tok = NULL;

    if ((NULL == p_cardListString) || (0 == strlen(p_cardListString)))
    {
        p_cardList[0] = DEFAULT_CARD_NUMBER;
        *p_numCards = 1;
        return 0;
    }

    /*
        Parsing the string potentially (OK, likely) modifies the original string, so create
        a temporary copy
    */
    dup = strdup(p_cardListString);
    if (NULL == dup)
    {
        fprintf(stderr, "Error: failed to allocate memory for string\n");
        return ENOMEM;
    }

    p_tok = strtok(dup, p_delimiters);
    while ((NULL != p_tok) && (SKIQ_MAX_NUM_CARDS > numCards) && (0 == result) && (running))
    {
        success = strToNumU8(p_tok, &cardNum);
        if (!success)
        {
            fprintf(stderr, "Error: couldn't parse card number '%s'\n", p_tok);
            result = EINVAL;
            continue;
        }

        /* Only add the card number if it doesn't already exist in the list */
        found = false;
        for (i = 0; i < numCards; i++)
        {
            if (cardNum == p_cardList[i])
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            p_cardList[numCards++] = cardNum;
        }

        p_tok = strtok(NULL, p_delimiters);
    }

    if (0 == result)
    {
        *p_numCards = numCards;
    }

    if (NULL != dup)
    {
        free(dup);
        dup = NULL;
    }

    return result;
}

int
main(int argc, char *argv[])
{
    int32_t result = 0;
    uint8_t cardList[SKIQ_MAX_NUM_CARDS] = { 0 };
    uint8_t cardListLen = 0;
    uint8_t validCardList[SKIQ_MAX_NUM_CARDS] = { 0 };
    uint8_t validCardListLen = 0;
    uint32_t ppsSignalCount[SKIQ_MAX_NUM_CARDS] = { 0 };
    uint8_t i = 0;
    bool cardFailed = false;
    int32_t resultsList[SKIQ_MAX_NUM_CARDS] = { [0 ... (SKIQ_MAX_NUM_CARDS-1)] = 0, };

    if (0 != arg_parser(argc, argv, p_help_short, p_help_long, p_args))
    {
        perror("Command Line");
        arg_parser_print_help(argv[0], p_help_short, p_help_long, p_args);
        return -1;
    }

    result = parseCardList(p_cardListStr, &(cardList[0]), &cardListLen);
    if (0 != result)
    {
        fprintf(stderr, "Error: failed to parse list of card numbers (result = %" PRIi32 ")\n",
            result);
        return -2;
    }
    if (1 > cardListLen)
    {
        fprintf(stderr, "Error: no card numbers specified!\n");
        return -3;
    }

    /* Calculate testSuccessThreshold (if needed) */
    if (UINT32_MAX == testSuccessThreshold)
    {
        if (0 < testRunTimeSec)
        {
            testSuccessThreshold = (testRunTimeSec - 1);
        }
        else
        {
            testSuccessThreshold = 0;
        }

        fprintf(stderr, "Info: success threshold set to %" PRIu32 " pulses\n",
            testSuccessThreshold);
    }

    /*
        Verify that the success threshold isn't unobtainable - the duration should tell us how
        many PPS signals we should see (1 pulse/seconds * N seconds = N pulses) so we'll use
        that as a maximum.
    */
    if (testRunTimeSec < testSuccessThreshold)
    {
        fprintf(stderr, "Warning: specified success threshold is higher than the number of PPS"
            " signals that can be received; setting to maximum (%" PRIu32 ")\n", testRunTimeSec);
        testSuccessThreshold = testRunTimeSec;
    }

    fprintf(stderr, "Info: testing PPS signals on %" PRIu8 " card(s):\n", cardListLen);
    for (i = 0; i < cardListLen; i++)
    {
        fprintf(stderr, "\t%" PRIu8 "\n", cardList[i]);
    }
    fprintf(stderr, "\n");

#if (defined __MINGW32__)
    /* Unfortunately Mingw does not support sigaction, so just use signal() */
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
#else
    {
        struct sigaction signal_action;

        /* Install the new signal handlers. */
        signal_action.sa_handler = sigHandler;
        sigemptyset(&signal_action.sa_mask);
        signal_action.sa_flags = 0;
        sigaction(SIGINT, &signal_action, NULL);
        sigaction(SIGTERM, &signal_action, NULL);
    }
#endif

    result = skiq_init_without_cards();
    if (0 != result)
    {
        fprintf(stderr, "Error: failed to initialize libsidekiq (result = %" PRIi32 ")\n", result);
        return -4;
    }

    fprintf(stderr, "Info: testing if specified card(s) can be successfully opened...\n");
    {
        result = 0;
        for (i = 0; i < cardListLen; i++)
        {
            resultsList[i] = skiq_enable_cards(&(cardList[i]), 1, skiq_xport_init_level_basic);
            if (0 == resultsList[i])
            {
                result = skiq_disable_cards(&(cardList[i]), 1);
                if (0 != result)
                {
                    fprintf(stderr, "Warning: failed to close card %" PRIu8 " (result = %" PRIi32
                        "); attempting to continue...", cardList[i], result);
                }
            }
        }

        fprintf(stderr, "Card initialization results:\n");
        for (i = 0; i < cardListLen; i++)
        {
            if (0 == resultsList[i])
            {
                fprintf(stderr, "  Card %" PRIu8 ": CAN be opened\n", cardList[i]);
                validCardList[validCardListLen++] = cardList[i];
            }
            else
            {
                fprintf(stderr, "  Card %" PRIu8 ": CANNOT be opened (result %"
                    PRIi32 ")\n", cardList[i], resultsList[i]);
            }
        }
    }

    if (0 == validCardListLen)
    {
        fprintf(stderr, "Error: no open cards!\n");
        return -5;
    }

    fprintf(stderr, "Info: opening card(s)...\n");
    result = skiq_enable_cards(&(validCardList[0]), validCardListLen, skiq_xport_init_level_full);
    if (0 != result)
    {
        fprintf(stderr, "Error: failed to enable available cards (result = %" PRIi32 ")\n",
            result);
        goto finished;
    }

    if (running)
    {
        fprintf(stderr, "Info: attempting to set PPS source...\n");

        result = setPpsSources(&(validCardList[0]), validCardListLen, p_ppsSource);
        if (0 != result)
        {
            fprintf(stderr, "Error: failed to set PPS sources on specified card(s) (result = %"
                PRIi32 ")\n", result);
            goto finished;
        }
    }

    if (running)
    {
        fprintf(stderr, "Info: running PPS counter test for %" PRIu32 " seconds...\n",
                testRunTimeSec);

        result = countPpsPulses(&(validCardList[0]), validCardListLen, testRunTimeSec,
                    displayTimestamps, displayTimestampsTable, &(ppsSignalCount[0]));
        if (0 != result)
        {
            fprintf(stderr, "Error: failed to run PPS test (result = %" PRIi32 ")\n", result);
            goto finished;
        }
        else
        {
            printf("\nPPS counter test results (%" PRIu32 " needed for a successful test):\n",
                testSuccessThreshold);

            if (displayTimestampsTable)
            {
                printf("%16s %16s %16s %16s\n", "Card Number", "Status", "Pulses Received",
                    "Pulses Expected");
            }

            for (i = 0; i < validCardListLen; i++)
            {
                if (!cardFailed)
                {
                    cardFailed = (testSuccessThreshold > ppsSignalCount[i]);
                }

                if (displayTimestampsTable)
                {
                    printf("%16" PRIu8 " %16s %16" PRIu32 " %16" PRIu32 "\n",
                        validCardList[i],
                        (testSuccessThreshold <= ppsSignalCount[i]) ? "PASSED" : "FAILED",
                        ppsSignalCount[i], testRunTimeSec);
                }
                else
                {
                    printf("    Card %4" PRIu8 ": \t%" PRIu8 " of %" PRIu8 " expected PPS"
                        " signals received [%s]\n", validCardList[i], ppsSignalCount[i], testRunTimeSec,
                        (testSuccessThreshold <= ppsSignalCount[i]) ? "PASSED" : "FAILED");
                }
            }
            printf("\n");
        }
    }
    else
    {
        fprintf(stderr, "Info: received shutdown signal\n");
    }

finished:
    fprintf(stderr, "Info: shutting down...\n");

    fprintf(stderr, "Info: closing cards...\n");
    result = skiq_disable_cards(&(validCardList[0]), validCardListLen);
    if (0 != result)
    {
        fprintf(stderr, "Warning: failed to close all cards (result = %" PRIi32 ");"
            " possible resource leak", result);
    }

    result = skiq_exit();
    if (0 != result)
    {
        fprintf(stderr, "Warning: failed to close libsidekiq (result = %" PRIi32 ");"
            " possible resource leak", result);
    }

    return (cardFailed) ? -6 : 0;
}

