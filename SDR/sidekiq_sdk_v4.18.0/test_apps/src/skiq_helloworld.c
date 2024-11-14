/* skiq_helloworld.c
 * IrishSat
 * 10/08/24
 */

#include <stdio.h>

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/types.h>
#include "sidekiq_types.h"
#include "sidekiq_params.h"
#include "sidekiq_xport_types.h"
#include "sidekiq_api.h"
#include <math.h>


#define SKIQ_MAX_NUM_CARDS (32)
skiq_tx_block_t **p_tx_blocks = NULL;
// function prototypes
int32_t init_tx_buffer(void);


// main
int main(int argc, char *argv[]){
	uint8_t card = 0;
	skiq_xport_type_t type = skiq_xport_type_auto;
	skiq_xport_init_level_t level = skiq_xport_init_level_full;
	skiq_tx_hdl_t hdl = skiq_tx_hdl_A1;
	uint64_t lo_freq = 418274940;

	//initialize sidekiq - SUCCESS
	int32_t init_status = skiq_init(type, level, &card, 1);

	//set tx sample rate and bandwidth - not working yet: "card not activated"
	int32_t rate_status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl, 10000000, 10000000);

	//set tx lo frequency
	int32_t lo_status = skiq_write_tx_LO_freq(card, hdl, lo_freq);

	//start streaming
	int32_t stream_status = skiq_start_tx_streaming(card, hdl);

	int32_t buffer_status = init_tx_buffer();
	int32_t transmit_status = 0;
	for (size_t i = 0; i < 100000; i++)
	{
		transmit_status = skiq_transmit(card, hdl, p_tx_blocks[0], NULL);
		if (transmit_status != 0){
			break;
		}
		// double sec = 0.001;
		// usleep( sec * 1000000);
	}
	

	//output exit statuses
	printf("init status: %u\n", init_status);
	printf("rate status: %u\n", rate_status);
	printf("lo status: %u\n", lo_status);
	printf("stream status: %u\n", stream_status);
	printf("transmit status: %u\n", transmit_status);

	skiq_exit();
		
	return 0;
}

// functions

int32_t init_tx_buffer() {
	char words[13] = "Hello World!";
	double frequency = 2000000;  // 2 MHz
	double sample_rate = 10000000; // 10 MHz
    int block_size_in_words = 13*4096;  // Block size in words (samples)
    int num_blocks = 1;
    int32_t status = 0;

    // Allocate the transmit block
	if (p_tx_blocks != NULL){
		free(p_tx_blocks);
	}
    p_tx_blocks = calloc(num_blocks, sizeof(skiq_tx_block_t*));
    if (p_tx_blocks == NULL) {
        fprintf(stderr, "Error: unable to allocate memory for transmit blocks\n");
        return -1;
    }

    p_tx_blocks[0] = skiq_tx_block_allocate(block_size_in_words);
    if (p_tx_blocks[0] == NULL) {
        fprintf(stderr, "Error: unable to allocate transmit block data\n");
        free(p_tx_blocks);
        return -2;
    }

    // Generate the cosine wave samples
    for (int i = 0; i < block_size_in_words; i++) {
        // double t = i / sample_rate;  // Calculate time for each sample
		// double pi = 3.14159265358979323846;
        // double cos_wave = cos(2 * pi * frequency * t);  // Cosine wave sample

        // // Store the sample in the block (assuming int32_t format for IQ samples)
        // // Here, I and Q components are interleaved as [I, Q, I, Q, ...]
        // p_tx_blocks[0]->data[i * 2] = (int32_t)(cos_wave * INT32_MAX);   // I component
        // p_tx_blocks[0]->data[i * 2 + 1] = 0;                             // Q component as zero
		//p_tx_blocks[0]->data[i * 2] = (int32_t) words[(i / 4096)];   // I component
		if (i % 2 == 0) {
			p_tx_blocks[0]->data[i * 2] = INT32_MAX;   // I component
		} else {
			p_tx_blocks[0]->data[i * 2] = INT32_MIN; 
		}
        p_tx_blocks[0]->data[i * 2 + 1] = 0;                             // Q component as zero
    }

	if (0 != status)
    {
        if (NULL != p_tx_blocks)
        {
            skiq_tx_block_free(p_tx_blocks[0]);

            free(p_tx_blocks);
            p_tx_blocks = NULL;
		}
    }

    return status;
}
