#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <features.h>
#include "semaphore_v2.h"

#define printF(x) write(1, x, strlen(x))
#define NUM_STATIONS 5

typedef struct {
    int car_id;
    semaphore body_done;
    semaphore engine_done;
    semaphore wheels_done;
    semaphore paint_done;
} CarProgress;

typedef struct {
    int total_cars;
    CarProgress *cars;
    pthread_mutex_t *station_mutex;
    semaphore *production_limit;
    pthread_mutex_t *screen_mutex;
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

int check_quality(int car_id, const char *station_name, pthread_mutex_t *screen_mutex) {
    unsigned int seed = time(NULL) ^ (unsigned int)pthread_self();
    int quality_score = rand_r(&seed) % 10;
    char *buffer;

    asprintf(&buffer, "Quality control of car %d at %s %s\n",
             car_id, station_name, quality_score > 5 ? "passed" : "failed");
    print_message(buffer, screen_mutex);
    free(buffer);

    return quality_score > 5;
}

void *body_station(void *arg) {
    StationData *data = (StationData *)arg;
    char *buffer;

    for (int i = 0; i < data->total_cars; i++) {
        SEM_wait(data->production_limit);
        pthread_mutex_lock(&data->station_mutex[0]);

        asprintf(&buffer, "Body Station of car %d starting\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        simulate_work();

        asprintf(&buffer, "Body Station of car %d assembled\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        SEM_signal(&data->cars[i].body_done);
        pthread_mutex_unlock(&data->station_mutex[0]);
    }
    return NULL;
}

void *engine_station(void *arg) {
    StationData *data = (StationData *)arg;
    char *buffer;

    for (int i = 0; i < data->total_cars; i++) {
        SEM_wait(&data->cars[i].body_done);
        pthread_mutex_lock(&data->station_mutex[1]);

        asprintf(&buffer, "Engine Station of car %d starting\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        simulate_work();

        asprintf(&buffer, "Engine Station of car %d assembled\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        SEM_signal(&data->cars[i].engine_done);
        pthread_mutex_unlock(&data->station_mutex[1]);
    }
    return NULL;
}

void *wheels_station(void *arg) {
    StationData *data = (StationData *)arg;
    char *buffer;

    for (int i = 0; i < data->total_cars; i++) {
        SEM_wait(&data->cars[i].engine_done);
        pthread_mutex_lock(&data->station_mutex[2]);

        asprintf(&buffer, "Wheel Station of car %d starting\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        simulate_work();

        asprintf(&buffer, "Wheel Station of car %d assembled\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        SEM_signal(&data->cars[i].wheels_done);
        pthread_mutex_unlock(&data->station_mutex[2]);
    }
    return NULL;
}

void *paint_station(void *arg) {
    StationData *data = (StationData *)arg;
    char *buffer;

    for (int i = 0; i < data->total_cars; i++) {
        SEM_wait(&data->cars[i].wheels_done);
        pthread_mutex_lock(&data->station_mutex[3]);

        asprintf(&buffer, "Paint Station of car %d starting\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        simulate_work();

        asprintf(&buffer, "Paint Station of car %d assembled\n", i + 1);
        print_message(buffer, data->screen_mutex);
        free(buffer);

        SEM_signal(&data->cars[i].paint_done);
        pthread_mutex_unlock(&data->station_mutex[3]);
    }
    return NULL;
}

void *quality_control(void *arg) {
    StationData *data = (StationData *)arg;
    char *buffer;

    for (int i = 0; i < data->total_cars; i++) {
        SEM_wait(&data->cars[i].paint_done);
        pthread_mutex_lock(&data->station_mutex[4]);

        while (1) {
            int passed = 1;
            passed &= check_quality(i + 1, "Body Station", data->screen_mutex);
            passed &= check_quality(i + 1, "Engine Station", data->screen_mutex);
            passed &= check_quality(i + 1, "Wheel Station", data->screen_mutex);
            passed &= check_quality(i + 1, "Paint Station", data->screen_mutex);

            if (passed) {
                asprintf(&buffer, "Quality control of car %d passed\n", i + 1);
                print_message(buffer, data->screen_mutex);
                free(buffer);
                break;
            }
            sleep(1);
        }

        SEM_signal(data->production_limit);
        pthread_mutex_unlock(&data->station_mutex[4]);
    }
    return NULL;
}

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

    CarProgress *cars = malloc(sizeof(CarProgress) * total_cars);
    pthread_mutex_t *station_mutex = malloc(sizeof(pthread_mutex_t) * (NUM_STATIONS + 1));
    semaphore production_limit;
    pthread_t threads[NUM_STATIONS];
    StationData thread_data;

    for (int i = 0; i <= NUM_STATIONS; i++) {
        pthread_mutex_init(&station_mutex[i], NULL);
    }

    SEM_constructor(&production_limit);
    SEM_init(&production_limit, 2);

    for (int i = 0; i < total_cars; i++) {
        cars[i].car_id = i + 1;
        SEM_constructor(&cars[i].body_done);
        SEM_constructor(&cars[i].engine_done);
        SEM_constructor(&cars[i].wheels_done);
        SEM_constructor(&cars[i].paint_done);

        SEM_init(&cars[i].body_done, 0);
        SEM_init(&cars[i].engine_done, 0);
        SEM_init(&cars[i].wheels_done, 0);
        SEM_init(&cars[i].paint_done, 0);
    }

    thread_data.total_cars = total_cars;
    thread_data.cars = cars;
    thread_data.station_mutex = station_mutex;
    thread_data.production_limit = &production_limit;
    thread_data.screen_mutex = &station_mutex[NUM_STATIONS];

    pthread_create(&threads[0], NULL, body_station, &thread_data);
    pthread_create(&threads[1], NULL, engine_station, &thread_data);
    pthread_create(&threads[2], NULL, wheels_station, &thread_data);
    pthread_create(&threads[3], NULL, paint_station, &thread_data);
    pthread_create(&threads[4], NULL, quality_control, &thread_data);

    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < total_cars; i++) {
        SEM_destructor(&cars[i].body_done);
        SEM_destructor(&cars[i].engine_done);
        SEM_destructor(&cars[i].wheels_done);
        SEM_destructor(&cars[i].paint_done);
    }

    SEM_destructor(&production_limit);

    for (int i = 0; i <= NUM_STATIONS; i++) {
        pthread_mutex_destroy(&station_mutex[i]);
    }

    free(cars);
    free(station_mutex);

    return 0;
}
