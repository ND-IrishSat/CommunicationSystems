/* skiq_helloworld.c
 * Irishsat
 * 10/08/24
 */

#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/types.h>
#include "sidekiq_types.h"
#include "sidekiq_params.h"
#include "sidekiq_xport_types.h"

#include "sidekiq_api.h"

#define SKIQ_MAX_NUM_CARDS (32)
// function prototypes


// main
int main(int argc, char *argv[]){
	
	uint8_t * p_cards = malloc(SKIQ_MAX_NUM_CARDS*sizeof(uint8_t));
	uint8_t p_num_cards;

	//get card number and array of cards 
	skiq_get_cards(skiq_xport_type_auto, &p_num_cards, p_cards);

	//initialize sidekiq - SUCCESS
	int32_t init_status = skiq_init(skiq_xport_type_auto, skiq_xport_init_level_basic, p_cards, p_num_cards);
	
	//enable cards - SUCCESS? - returns 0 for success but next command throws error (see below)
	int32_t enable_status = skiq_enable_cards(p_cards, p_num_cards, skiq_xport_init_level_full);
	
	//set tx sample rate and bandwidth - not working yet: "card not activated"
	int32_t rate_status = skiq_write_tx_sample_rate_and_bandwidth(*p_cards, skiq_tx_hdl_A1, 10, 10);
	
	//output exit statuses
	printf("init status: %u\n", init_status);
	printf("enable status: %u\n", enable_status);
	printf("rate status: %u\n", rate_status);
	
	printf("card number enabled: %u\n", p_cards[0]);
	free(p_cards);
		
	return 0;
}

// functions



