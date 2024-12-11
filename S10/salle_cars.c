#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "semaphore_v2.h"

#define printF(x) write(1, x, strlen(x))

typedef struct {
    int car_id;
    semaphore body_done;
    semaphore engine_done;
    semaphore wheels_done;
    semaphore paint_done;
    semaphore qc_done;
} CarProgress;

typedef struct {
    int total_cars;
    int *current_car;
    CarProgress *cars;
    pthread_mutex_t *station_mutex;
    semaphore *production_limit;
} StationData;

void print_message(const char *msg, pthread_mutex_t *screen_mutex) {
    pthread_mutex_lock(screen_mutex);
    write(1, msg, strlen(msg));
    pthread_mutex_unlock(screen_mutex);
}

void simulate_work() {
    unsigned int seed = time(NULL) ^ (unsigned int)pthread_self();
    int work_time = (rand_r(&seed) % 5) + 1;
    sleep(work_time);
}

void *body_station(void *arg) {
    StationData *data = (StationData *)arg;
    char *buffer = NULL;

    for (int i = 0; i < data->total_cars; i++) {
        // Wait for production slot
        SEM_wait(data->production_limit);

        // Lock station
        pthread_mutex_lock(&data->station_mutex[0]);

        asprintf(&buffer, "Body Station of car %d starting\n", i + 1);
        print_message(buffer, &data->station_mutex[5]); // screen mutex
        free(buffer);

        simulate_work();

        asprintf(&buffer, "Body Station of car %d assembled\n", i + 1);
        print_message(buffer, &data->station_mutex[5]);
        free(buffer);

        // Signal completion
        SEM_signal(&data->cars[i].body_done);
        pthread_mutex_unlock(&data->station_mutex[0]);
    }
    return NULL;
}

// Similar station functions for engine, wheels, paint, and QC will follow...

int main(int argc, char *argv[]) {
    if (argc != 2) {
        write(2, "Usage: ./S10.exe <number of cars>\n", 33);
        return 1;
    }

    int total_cars = atoi(argv[1]);
    if (total_cars < 1) {
        write(2, "Usage: ./S10.exe <number of cars>\n", 33);
        return 1;
    }

    // Continue with initialization...
    return 0;
}