#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);                 // give serial monitor time to attach
    setvbuf(stdout, NULL, _IONBF, 0);

    while (true) {
        printf("hello from rp2040\n");
        sleep_ms(1000);
    }
}