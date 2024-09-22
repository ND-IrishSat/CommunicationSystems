/**
 * @file   elapsed.h
 * @author  <jeremy@epiq-solutions.com>
 * @date   Mon Jun  5 16:09:36 2017
 *
 * @brief
 *
 * <pre>
 * Copyright 2017-2020 Epiq Solutions, All Rights Reserved
 *
 *
 * </pre>
 *
 */

#ifndef __ELAPSED_H__
#define __ELAPSED_H__

/**
    @todo   `clock_gettime()` seems to take different amounts of time on different platforms -
            on x86 it generally uses a specialized high-resolution timer instruction that is
            fast (generally tens of nanoseconds), but ARM hosts have slower implementations that
            vary in time (generally single to tens of microseconds). It would be very nice to
            find a good high-resolution timer that is available on all platforms and use it
            here...
*/

/***** INCLUDES *****/

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

/***** DEFINES *****/

#define ELAPSED(_x)         struct elapsed _x = ELAPSED_INITIALIZER
#define ELAPSED_INITIALIZER                                             \
    (struct elapsed){                                                   \
        .total = { 0, 0 },                                              \
        .max = { 0, 0 },                                                \
        .min = { LONG_MAX, LONG_MAX },                                  \
        .num_samples = 0,                                               \
        .mean = 0.0,                                                    \
        .std = 0.0                                                      \
    }

/***** TYPEDEFS *****/

struct elapsed
{
    struct timespec start, stop, total;
    struct timespec max, min;
    double mean, std;
    uint64_t num_samples;
};

/***** INLINE FUNCTIONS  *****/


/* diff = b - a */
static struct timespec timespec_sub( struct timespec *a,
                                     struct timespec *b )
{
    struct timespec diff;

    if (b->tv_nsec < a->tv_nsec)
    {
        diff.tv_sec = b->tv_sec - a->tv_sec - 1;
        diff.tv_nsec = b->tv_nsec + (long)1e9 - a->tv_nsec;
    }
    else
    {
        diff.tv_sec = b->tv_sec - a->tv_sec;
        diff.tv_nsec = b->tv_nsec - a->tv_nsec;
    }

    return diff;
}

/* sum = a + b */
static struct timespec timespec_add( struct timespec *a,
                                     struct timespec *b )
{
    struct timespec sum;

    sum.tv_sec = a->tv_sec + b->tv_sec;
    sum.tv_nsec = a->tv_nsec + b->tv_nsec;
    if ( sum.tv_nsec > (long)1e9 )
    {
        sum.tv_sec += sum.tv_nsec / (long)1e9;
        sum.tv_nsec = sum.tv_nsec % (long)1e9;
    }

    return sum;
}

/* true if a > b */
static int timespec_gt( struct timespec *a,
                        struct timespec *b )
{
    if ( a->tv_sec == b->tv_sec )
    {
        return ( a->tv_nsec > b->tv_nsec );
    }
    else
    {
        return ( a->tv_sec > b->tv_sec );
    }
}

static inline void elapsed_start(struct elapsed *e)
{
    clock_gettime(CLOCK_MONOTONIC, &(e->start));
}

/* subtract e->start from e->stop and increment e->total by that difference */
static inline void elapsed_end(struct elapsed *e)
{
    struct timespec diff;
    clock_gettime(CLOCK_MONOTONIC, &(e->stop));

    diff = timespec_sub( &(e->start), &(e->stop) );
    e->total = timespec_add( &(e->total), &diff );
    if ( timespec_gt( &diff, &(e->max) ) )
    {
        e->max = diff;
    }
    if ( timespec_gt( &(e->min), &diff ) )
    {
        e->min = diff;
    }

    e->num_samples++;

    {
        int64_t value_ns = diff.tv_sec * (int64_t)1e9 + diff.tv_nsec;
        double temp_mean = e->mean;

        e->mean = e->mean + (value_ns - temp_mean) / e->num_samples;
        e->std = e->std + (value_ns - temp_mean) * ( value_ns - e->mean );
    }
}

#define elapsed_print(_x)                                               \
    do {                                                                \
        printf("%30s: %4ld.%09ld\n",                                    \
               #_x,                                                     \
               (long)(_x)->total.tv_sec,                                \
               (_x)->total.tv_nsec);                                    \
    } while (0)

#define _elapsed_print_str(_x, _e)       \
    do {                                 \
        printf("%45s: %4ld.%09ld\n",     \
               _x,                       \
               (long)(_e)->total.tv_sec, \
               (_e)->total.tv_nsec);     \
    } while (0)

#define elapsed_call(_x)                        \
    do {                                        \
        ELAPSED(_my_elapsed);                   \
        elapsed_start(&_my_elapsed);            \
        _x;                                     \
        elapsed_end(&_my_elapsed);              \
        _elapsed_print_str(#_x, &_my_elapsed);  \
    } while(0)

#define print_minimum(_e)               _print_us(&((_e)->min))
#define print_maximum(_e)               _print_us(&((_e)->max))

static inline
void print_stddev( struct elapsed *e )
{
    if ( e->num_samples > 1 )
    {
        printf("%13.3f uS\n", sqrt(e->std / (e->num_samples - 1)) / 1000.0);
    }
    else
    {
        printf("%13s uS\n", "NaN");
    }
}

static inline
void print_average( struct elapsed *e )
{
    printf("%13.3f uS\n", e->mean / 1000.0);
}

static inline
void print_average_and_error( struct elapsed *e, uint64_t nr_nanoseconds )
{
    printf("%13.3f uS (err %+.3f uS = %.1f%%)\n", e->mean / 1000.0,
           (e->mean - (double)nr_nanoseconds) / 1000.0,
           (e->mean - (double)nr_nanoseconds) / (double)nr_nanoseconds * 100.0);
}

static inline
void _print_us( struct timespec *ts )
{
    uint64_t num_ns = (uint64_t)(ts->tv_sec) * (uint64_t)1e9 + (uint64_t)(ts->tv_nsec);

    printf("%9" PRIu64 ".%03" PRIu64 " uS\n", num_ns / (uint64_t)1000, num_ns % (uint64_t)1000);
}

static inline
void print_nr_calls( struct elapsed *e )
{
    printf("%13" PRIu64 "\n", e->num_samples);
}

static inline
void print_total( struct elapsed *e )
{
    printf("%3" PRId64 ".%09lu seconds\n", (int64_t)e->total.tv_sec, e->total.tv_nsec);
}

#endif  /* __ELAPSED_H__ */
