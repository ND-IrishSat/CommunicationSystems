#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// ---------------- Overview of What This Program Does -----
/* 
1. Waits for binary packets over USB serial
2. Detects packet start
3. Reads sequence number
4. Reads payload
5. Verifies CRC
6. Stores valid packets in a queue
7. Processes them one-by-one 
*/

// ---------------- SPI (your starter code) ----------------
// defines which GPIO pins are used for SPI communication
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// ---------------- Packet + Queue ----------------
// example packet layout [A5][5A][seq][len][payload][crc]
// SOF = start-of-frame markers
// SEQ = sequence number
// LEN = payload length
// PAYLOAD = actual data
// CRC = error detection
#define SOF0 0xA5
#define SOF1 0x5A

#define MAX_PAYLOAD 256
#define QUEUE_CAP   32

// one complete packet definition
typedef struct {
    uint32_t seq;
    uint16_t len;
    uint8_t  payload[MAX_PAYLOAD];
} Packet;

// store packets in a circular buffer queue
typedef struct {
    Packet buf[QUEUE_CAP];
    uint16_t head, tail, count;
} PacketQueue;

static inline void pq_init(PacketQueue *q) {
    q->head = q->tail = q->count = 0;
}

// push a packet onto queue
static inline bool pq_push(PacketQueue *q, const Packet *p) {
    if (q->count == QUEUE_CAP) return false; // if the queue is full return false
    q->buf[q->head] = *p;
    q->head = (q->head + 1u) % QUEUE_CAP;
    q->count++;
    return true;
}

// removes the oldest packet
static inline bool pq_pop(PacketQueue *q, Packet *out) {
    if (q->count == 0) return false;
    *out = q->buf[q->tail];
    q->tail = (q->tail + 1u) % QUEUE_CAP;
    q->count--;
    return true;
}

// ---------------- CRC32 (same poly as most implementations) ----------------
static uint32_t crc32_table[256];

// error checking
static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
}

// crc calculation
static uint32_t crc32_calc(const uint8_t *data, size_t len) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static uint16_t rd_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

// ---------------- Parser state machine ----------------
// receive one byte at a time and build the state machine (allows the program to recover if bytes or lost or corrupted)
typedef enum {
    WAIT_SOF0,
    WAIT_SOF1,
    READ_SEQ,
    READ_LEN,
    READ_PAYLOAD,
    READ_CRC
} ParseState;

static ParseState st = WAIT_SOF0;
static uint8_t seq_buf[4];
static uint8_t len_buf[2];
static uint8_t crc_buf[4];
static uint16_t idx = 0;

static Packet current;
static PacketQueue q;

static inline void reset_parser(void) {
    st = WAIT_SOF0;
    idx = 0;
}

// processes one incoming byte at a time and updates the parser state
static void feed_byte(uint8_t b) {
    switch (st) {
        case WAIT_SOF0:
            if (b == SOF0) st = WAIT_SOF1;
            break;

        case WAIT_SOF1:
            if (b == SOF1) { st = READ_SEQ; idx = 0; }
            else st = WAIT_SOF0;
            break;

        case READ_SEQ:
            seq_buf[idx++] = b;
            if (idx == 4) {
                current.seq = rd_le32(seq_buf);
                st = READ_LEN;
                idx = 0;
            }
            break;

        case READ_LEN:
            len_buf[idx++] = b;
            if (idx == 2) {
                current.len = rd_le16(len_buf);
                if (current.len > MAX_PAYLOAD) {
                    // bad length => resync
                    reset_parser();
                } else {
                    st = READ_PAYLOAD;
                    idx = 0;
                }
            }
            break;

        case READ_PAYLOAD:
            current.payload[idx++] = b;
            if (idx == current.len) {
                st = READ_CRC;
                idx = 0;
            }
            break;

        case READ_CRC:
            crc_buf[idx++] = b;
            if (idx == 4) {
                uint32_t got = rd_le32(crc_buf);

                // compute CRC over seq||len||payload (not including SOF)
                uint8_t tmp[4 + 2 + MAX_PAYLOAD];
                memcpy(tmp, seq_buf, 4);
                memcpy(tmp + 4, len_buf, 2);
                memcpy(tmp + 6, current.payload, current.len);

                uint32_t want = crc32_calc(tmp, 6 + current.len);

                if (got == want) {
                    if (!pq_push(&q, &current)) {
                        printf("Queue full: dropped seq=%lu\n", (unsigned long)current.seq);
                    } else {
                        // Optional: acknowledge receipt
                        // printf("OK %lu\n", (unsigned long)current.seq);
                    }
                } else {
                    printf("CRC fail (seq=%lu)\n", (unsigned long)current.seq);
                }

                reset_parser();
            }
            break;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(500); // helps some serial monitors attach cleanly

    // SPI init (leave as-is or remove later)
    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    crc32_init();
    pq_init(&q);

    printf("RP2040 ready. Send framed packets over USB serial.\n");

    absolute_time_t last_heartbeat = get_absolute_time();

    while (true) {
        // Non-blocking read from USB serial
        int ch;
        while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
            feed_byte((uint8_t)ch);
        }

        // Example consumer: pop and print
        Packet p;
        if (pq_pop(&q, &p)) {
            printf("Dequeued seq=%lu len=%u\n", (unsigned long)p.seq, (unsigned)p.len);
            // Do real work with p.payload[0..p.len-1]
        }

        // Optional heartbeat so you know firmware is alive
        if (absolute_time_diff_us(last_heartbeat, get_absolute_time()) > 1000000) {
            last_heartbeat = get_absolute_time();
            // printf(".\n"); // uncomment if you want periodic output
        }

        tight_loop_contents();
    }
}