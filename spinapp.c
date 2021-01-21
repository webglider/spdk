// Simple spinning throughput-bound app
// gcc -o build/spinapp -O3 spinapp.c

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, const char **argv) {

    if(argc < 2) {
        printf("Usage: ./spinapp <duration-in-secs>\n");
        return -1;
    }

    int duration = atoi(argv[1]);
    // clock_t duration_clks = duration * CLOCKS_PER_SEC;

    uint64_t count = 0;

    time_t start_tim = time(NULL);

    while(1) 
    {
        if(count % 1000000 == 0 && time(NULL) - start_tim >= duration) {
            break;
        }
        count += 1;
    }

    printf("Spins/sec: %lf\n", (double)count/(double)duration);

    return 0;
}