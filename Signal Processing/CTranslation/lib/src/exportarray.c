#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>

void exportArray(double* input, int length, char filename[]){
    FILE *fpt;
    char export_name[1000] = ""; // can add an export path here
    int start = strlen(export_name);
    for (int i = 0; i < strlen(filename); i++) // fix export name
    {
        export_name[i+start] = filename[i];
    }
    export_name[strlen(export_name)] = '\0';
    fpt = fopen(export_name, "w+");
    for (int i = 0; i < length; i++)
    {
        fprintf(fpt, "%lf", input[i]);
        if (i != length-1){
            fprintf(fpt, "\n");
        }
    }
    fclose(fpt);
}