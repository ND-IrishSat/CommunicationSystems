#ifndef IRISHSAT_SIGNAL_PROCESSING_H
#define IRISHSAT_SIGNAL_PROCESSING_H
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include "lib/irishsat_comms_lib.h" 
#include <time.h>

typedef struct {
    int data_length;
    double fs; // carrier frequency not 2.4e9!!!!
    int pulse_shape_length; // pulse shaping filter length (Is this the ideal length?)
    char* pulse_shape; // type of pulse shaping, also "rect"
    char* scheme; // Modulation scheme 'OOK', 'QPSK', or 'QAM'
    double alpha; // roll-off factor of the RRC pulse-shaping filter
    int sps; // samples per symbol, equal to oversampling factor
    double* preamble;
    int preamble_length;
    double* CRC_key;
    int CRC_length;
    bool exportArrays;
    bool generateRandomData;
    bool showOutputArrays;
    bool verboseTimers;
} SignalParameters;
typedef struct {
    double std_dev; // typically 1
    double phase_noise_stength; // typically 0.1
    double noise_power; // typically ~10
} NoiseParameters;
Complex_Array_Tuple Encode(SignalParameters params, double* data){
    clock_t start, end;
    if (params.verboseTimers) { start = clock();} // timer

    // *Preamble, Data, and Matched Filter Coefficient Generation--------------
    Array_Tuple preamble = defineArray(params.preamble, params.preamble_length);// optimal periodic binary code for N = 63 https://ntrs.nasa.gov/citations/19800017860
    if (params.exportArrays){ exportArray(preamble, "preamble.txt"); }
    
    
    Array_Tuple _data;
    if (params.generateRandomData){
        _data.array = randomArray(2,params.data_length);
        _data.length = params.data_length; // this points to the random array generated;  creates 256 random bits
        printArray("Random Data", _data);
    }else{
        _data = defineArray((double*)data, params.data_length);
    }

    if (params.exportArrays) {exportArray(_data, "data.txt"); }
    Array_Tuple CRC_key = defineArray(params.CRC_key, params.CRC_length);// Best CRC polynomials: https://users.ece.cmu.edu/~koopman/crc/
    Array_Tuple data_encoded = CRC_encodeData(_data, CRC_key); // length of data + key - 1
    Array_Tuple bits = append_array(preamble, data_encoded);
    if (params.exportArrays) { exportArray(bits, "bits.txt"); }
    Array_Tuple matched_filter_coef = flip(preamble);

    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Format Data", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer

    // *Generation of Pulse Train
    if (params.verboseTimers) { start = clock();} // timer
    Array_Tuple pulse_train = pulsetrain(bits, params.sps);
    if (params.exportArrays){ exportArray(pulse_train, "pulsetrain.txt"); }

    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Pulse Train", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer
    


    // *Pulse Shaping
    if (params.verboseTimers) { start = clock();} // timer
    Complex_Array_Tuple complexTestpacket = pulse_Shaping_Main(pulse_train, params.sps, params.fs, params.pulse_shape, params.alpha, params.pulse_shape_length);
    if (params.exportArrays){ exportComplexArray(complexTestpacket, "pulseshaping.txt"); }
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Pulse Shape", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer

    freeArrayMemory(preamble);
    freeArrayMemory(_data);
    freeArrayMemory(CRC_key);
    freeArrayMemory(data_encoded);
    freeArrayMemory(bits);
    freeArrayMemory(matched_filter_coef);
    freeArrayMemory(pulse_train);
    return complexTestpacket;
}
Complex_Array_Tuple Noise(SignalParameters params, NoiseParameters noise_params, Complex_Array_Tuple data){
    Complex_Array_Tuple noise = generateComplexNoise(data, noise_params.std_dev, noise_params.phase_noise_stength, noise_params.noise_power);
    if (params.exportArrays){ exportComplexArray(noise, "noise.txt"); }
    return noise;
    
}
Array_Tuple Decode(SignalParameters params, Complex_Array_Tuple data){
    clock_t start, end;

    // *Simulation of Channel
    if (params.verboseTimers) { start = clock();} // timer
    Complex_Array_Tuple testpacket_freq_shift = fractional_delay_frequency_offset(data, params.fs, 1.0 / params.fs);
    if (params.exportArrays){ exportComplexArray(testpacket_freq_shift, "testpacketfreqshift.txt");}
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Fractional Delay", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer


    // *Clock Recovery
    if (params.verboseTimers) { start = clock();} // timer
    Complex_Array_Tuple testpacket = clock_Recovery(testpacket_freq_shift, params.sps, params.exportArrays);
    if (params.exportArrays){ exportComplexArray(testpacket, "clockRecovery.txt"); }
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Clock Recovery", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer
    freeComplexArrayMemory(testpacket_freq_shift);


    // *Coarse Frequency Correction
    if (params.verboseTimers) { start = clock();} // timer
    Complex_Array_Tuple new_testpacket = coarse_Frequency_Correction(testpacket, params.fs);
    if (params.exportArrays){ exportComplexArray(new_testpacket, "coarseFrequencyCorrection.txt"); }
    freeComplexArrayMemory(testpacket);


    // *Fine Frequency Correction
    Complex_Array_Tuple costas_out = fine_Frequency_Correction(new_testpacket, params.fs, params.exportArrays);
    if (params.exportArrays){ exportComplexArray(costas_out, "costasout.txt"); }
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Frequency Correction", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer
    freeComplexArrayMemory(new_testpacket);


    // *IQ Imbalance Correction
    if (params.verboseTimers) { start = clock();} // timer
    int mean_period = 100;
    Complex_Array_Tuple testpacket_IQ_Imbalance_Correct = IQImbalanceCorrect(costas_out, mean_period); //TODO clean function
    if (params.exportArrays){ exportComplexArray(testpacket_IQ_Imbalance_Correct, "iqimbalanceout.txt"); }
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","IQ Correction", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer
    freeComplexArrayMemory(costas_out);

    // Some Constant stuff
    if (params.verboseTimers) { start = clock();} // timer
    Array_Tuple preamble = defineArray(params.preamble, params.preamble_length);
    Array_Tuple matched_filter_coef = flip(preamble);
    Array_Tuple CRC_key = defineArray(params.CRC_key, params.CRC_length);// Best CRC polynomials: https://users.ece.cmu.edu/~koopman/crc/
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Generate Constants", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer
    
    // *Frame Sync
    if (params.verboseTimers) { start = clock();} // timer
    Complex_Array_Tuple recoveredData = frame_Sync(testpacket_IQ_Imbalance_Correct, matched_filter_coef, preamble, params.data_length+CRC_key.length-1, params.exportArrays); //TODO clean function
    freeComplexArrayMemory(testpacket_IQ_Imbalance_Correct);
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Frame Sync", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer


    // *Demodulation
    if (params.verboseTimers) { start = clock();} // timer
    Array_Tuple demod_bits = demodulation(recoveredData, params.scheme, preamble); //TODO clean function
    if (params.exportArrays){ exportArray(demod_bits, "demodbits.txt"); }
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Demodulation", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer
    freeComplexArrayMemory(recoveredData);

    // Format output
    if (params.verboseTimers) { start = clock();} // timer
    double *temp = malloc(sizeof(double) * demod_bits.length);
    int output_length = demod_bits.length - params.CRC_length+1;
    for (size_t i = 0; i < output_length; i++)
    {
        temp[i] = demod_bits.array[i];
    }
    
    Array_Tuple demod_bits_without_CRC_key = defineArray(temp, output_length);
    if (params.verboseTimers) { end = clock(); printf("   - %-21s:  %.4f s\n","Format Output", ((double)(end - start)) / CLOCKS_PER_SEC);}  // timer

    freeArrayMemory(preamble);
    freeArrayMemory(matched_filter_coef);
    freeArrayMemory(CRC_key);
    freeArrayMemory(demod_bits);
    free(temp);
    return demod_bits_without_CRC_key;
}


void printBinaryData(int *binaryData) {
    // Print binary data
    size_t i =0;
    while (1)
    {
        bool nullchar = true;
        for (int bit = 7; bit >= 0; bit--) {
            printf("%d", ((unsigned char)binaryData[i] >> bit) & 1);
            if (binaryData[i] != 0){
                nullchar = false;
            }
        }
        if (nullchar) break;
        printf(" ");
        i++;
    }
    printf("\n");
    
}
int *stringToBinaryArray(char *str, size_t *numbits) {
    if (!str) return NULL;

    size_t len = strlen(str)+1; // include null char
    *(numbits) = len*8;

    int *binaryData = malloc(len * sizeof(int));
    if (!binaryData) {
        perror("Memory allocation failed");
        return NULL;
    }

    for (size_t i = 0; i < len; i++) {
        binaryData[i] = (int)str[i]; // Directly store each character's ASCII value
    }

    return binaryData;
}
char* binaryToString(int *binaryData) {
    int MAX_STRING_LENGTH = 4095;
    char *reconstructedString = malloc(1);

    if (!reconstructedString) {
        perror("Memory allocation failed");
        return NULL;
    }
    size_t i = 0;
    while (1)
    {
        if (i >= MAX_STRING_LENGTH) {
            reconstructedString[i - 1] = '\0'; // Null-terminate the string
            break;
        }
        char *temp = realloc(reconstructedString, (i + 2));
        if (!temp) {
            perror("Memory reallocation failed");
            free(reconstructedString); // Free the previously allocated memory
            return NULL;
        }
        reconstructedString = temp;
        reconstructedString[i] = (char)binaryData[i]; // Convert back to characters
        if ((char)binaryData[i] == '\0'){
            break;
        }
        i++;
    }
    return reconstructedString;
}


#endif