/*
main.c
IrishSAT CLOVER SDR
Rylan Paul — Communication Systems Team
*/
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <time.h>
// Our libraries --------------------------------
#include "lib/irishsat_comms_lib.h" 
#include "signals.h"
//-----------------------------------------------
int main(){
    // *DECLARE VARIABLES --------------
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

    const NoiseParameters noise_params = {
        .std_dev=1, // typically 1
        .phase_noise_stength=0.1, // typically 0.1
        .noise_power=10 // typically ~10
    };

    clock_t total, start, end; // timers

    // Define Data
    char words[13]="Hello World!";
    size_t numbits; // number of bits used for word
    int *binaryData = stringToBinaryArray(words, &numbits);
    params.data_length = numbits;
    double *data = calloc(params.data_length, sizeof(double));
    printf("ASCII: ");
    for (size_t i = 0; i < numbits / 8; i++)
    {
        printf("%d ", binaryData[i]);
    }
    
    for (int i = 0; i < params.data_length; i++) {
        for (int bit = 0; bit < 7; bit++) {
            data[(i*8)+(7-bit)] = ((unsigned char)binaryData[i] >> bit) & 1;
        }
    }
    printf("\n\n");
    free(binaryData);


    // Encode Data
    total = clock();
    start = clock();
    Complex_Array_Tuple encoded = Encode(params, data);
    end = clock();
    printf("%-25s :  %.4f s\n", "Encode", ((double)(end - start)) / CLOCKS_PER_SEC);


    //#############################################################################################
    // *----TRANSMISSION and Noise----
    start = clock();
    Complex_Array_Tuple noise = Noise(params, noise_params, encoded);
    end = clock();
    printf("%-25s :  %.4f s\n", "Noise", ((double)(end - start)) / CLOCKS_PER_SEC);
    //#############################################################################################

    // Decode Data on reception
    start = clock();
    Array_Tuple output = Decode(params, noise);
    end = clock();
    printf("%-25s :  %.4f s\n", "Decode", ((double)(end - start)) / CLOCKS_PER_SEC);
    printf("%-26s:  %.4f s\n\n", "----------Total-----------", ((double)(end - start)) / CLOCKS_PER_SEC);

    
    // *Display Output
    Array_Tuple _data = defineArray((double*)data, params.data_length);
    DisplayOutput(_data, output, params.showOutputArrays);


    freeComplexArrayMemory(encoded);
    freeComplexArrayMemory(noise);
    freeArrayMemory(output);
    freeArrayMemory(_data);
    free(data);

    return 0;
}