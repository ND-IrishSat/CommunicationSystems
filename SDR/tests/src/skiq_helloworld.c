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
#include "../lib/IrishsatSignalProcessing/lib/standardArray.h"
#include "../lib/IrishsatSignalProcessing/signals.h"



#define SKIQ_MAX_NUM_CARDS (32)
skiq_tx_block_t **p_tx_blocks = NULL;
// function prototypes
int32_t init_tx_buffer(SignalParameters params, int *bits);


// main
int main(int argc, char *argv[]){

    //define signal parameters
	 SignalParameters params = {
        .data_length=256, .fs=418274940, .pulse_shape_length=8, .pulse_shape="rrc", .scheme="BPSK",
        .alpha=0.5, .sps=8, 
        .preamble=(double[]){0,1,0,0,0,0,1,1,0,0,0,1,0,1,0,0,1,1,1,1,0,1,0,0,0,1,1,1,0,0,1,0,0,1,0,1,1,0,1,1,1,0,1,1,0,0,1,1,0,1,0,1,0,1,1,1,1,1,1,0},
        .preamble_length=60,
        .CRC_key=(double[]){1,0,0,1,1,0,0,0,0,1,1,1,0,0},
        .CRC_length=14,
        .exportArrays=false,
        .generateRandomData=false,
        .showOutputArrays=true,
        .verboseTimers=true
    };
    int numbits = 0;

	uint8_t card = 0;
	skiq_xport_type_t type = skiq_xport_type_auto;
	skiq_xport_init_level_t level = skiq_xport_init_level_full;
	skiq_tx_hdl_t hdl = skiq_tx_hdl_A1;

	//initialize sidekiq with parameters
	int32_t skiq_status = skiq_init(type, level, &card, 1);
    printf("init status: %u\n", skiq_status);
	skiq_status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl, 10000000, 10000000);
    printf("rate status: %u\n", skiq_status);
	skiq_status = skiq_write_tx_LO_freq(card, hdl, params.fs);
    printf("lo status: %u\n", skiq_status);
	skiq_status = skiq_start_tx_streaming(card, hdl);
    printf("stream status: %u\n", skiq_status);
	skiq_status = init_tx_buffer(params, &(numbits));
    params.data_length = numbits;
    
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

	skiq_exit();
		
	return 0;
}

// functions

int32_t init_tx_buffer(SignalParameters params, int* bits) {
	// Define Data
    char words[13] = "Hello World!";

    // Convert to binary
    size_t numbits = (strlen(words)+1)*8; // number of bits used for word
    *(bits) = numbits;

    double *data = calloc(numbits, sizeof(double));
    printf("ASCII: ");
    for (size_t i = 0; i < numbits / 8; i++)
    {
        printf("%d ", (int)words[i]);
    }

    // convert ascii int to array of binary 0s and 1s
    for (int i = 0; i < numbits / 8; i++) {
        for (int bit = 0; bit < 7; bit++) {
            int index = (i*8)+(7-bit);
            data[index] = ((unsigned char)words[i] >> bit) & 1;
        }
    }
	Complex_Array_Tuple encoded = Encode(params, data);

    int block_size_in_words = encoded.real.length;  // Block size in words (samples)
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

        // Store the sample in the block (assuming int32_t format for IQ samples)
        // Here, I and Q components are interleaved as [I, Q, I, Q, ...]
        // p_tx_blocks[0]->data[i * 2] = (int32_t)(cos_wave * INT32_MAX);   // I component
        // p_tx_blocks[0]->data[i * 2 + 1] = 0;                             // Q component as zero
		p_tx_blocks[0]->data[i * 2] = (int32_t) encoded.real.array[i];   // I component
		// if (i % 2 == 0) {
		// 	p_tx_blocks[0]->data[i * 2] = INT32_MAX;   // I component
		// } else {
		// 	p_tx_blocks[0]->data[i * 2] = INT32_MIN; 
		// }
        p_tx_blocks[0]->data[i * 2 + 1] = (int32_t) encoded.imaginary.array[i];    // Q component as zero
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
    freeComplexArrayMemory(encoded);

    return status;
}
