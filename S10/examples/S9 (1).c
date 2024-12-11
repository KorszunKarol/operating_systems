/*
 * Simulation of MotoGP Qualifying Session S9
 * Course 2024-25
 *
 * @author: Ferran Castañé
 *
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#include <errno.h>
#include <stdint.h>
#include "semaphore_v2.h"

#define printF(x) write(1, x, strlen(x))

/**
 * Structure representing a MotoGP rider.
 */
typedef struct {
    char *firstName;
    char *lastName;
    int number;
    char *motorcycleBrand;
} MotoGpRider;

/**
 * Structure representing a rider's lap time.
 */
typedef struct {
    int riderId;
    float totalTime;
    float sector1;
    float sector2;
    float sector3;
} MotoGpRiderTime;

/**
 * Structure to pass data to the thread.
 */
typedef struct {
    MotoGpRider *rider;
    MotoGpRiderTime *bestTimes;
    int numRiders;
} ThreadData;

/** Mutex for screen access */
pthread_mutex_t mutexScreen;
/** Mutex for accessing best times */
pthread_mutex_t mutexBestTimes;
/** Mutex and condition variable for session over */
pthread_mutex_t mutexSessionOver;
/** Flag indicating if the session is over */
int sessionOver = 0;

/** Number of riders on track */
int numRidersOnTrack = 0;

semaphore semaphoreMotoGp;

/**
 * Initializes the screen mutex.
 */
void createScreenMutex() {
    pthread_mutex_init(&mutexScreen, NULL);
}

/**
 * Destroys the screen mutex.
 */
void destroyScreenMutex() {
    pthread_mutex_destroy(&mutexScreen);
}

/**
 * Thread-safe function to print a message to the screen.
 *
 * @param message The message to print.
 */
void printMessage(char *message) {
    pthread_mutex_lock(&mutexScreen);
    write(1, message, strlen(message));
    pthread_mutex_unlock(&mutexScreen);
}

/**
 * Reads from the file descriptor until the specified character is found.
 *
 * @param fd File descriptor to read from.
 * @param cEnd Character indicating the end of reading.
 * @return A dynamically allocated string containing the read data.
 */
char *readUntil(int fd, char cEnd) {
    int i = 0;
    int charsRead = 0;
    char c = ' ';
    char *buffer = NULL;

    while (c != cEnd) {
        charsRead = (int) read(fd, &c, sizeof(char));
        if (charsRead <= 0) {
            break;
        }
        buffer = (char *) realloc(buffer, (sizeof(char)) * (i + 2));
        if (c != cEnd) {
            buffer[i] = c;
            i++;
        }
    }

    if (buffer != NULL) {
        buffer[i] = '\0';
    }
    return buffer;
}

/**
 * Reads riders from a file.
 *
 * @param filePath Path to the riders file.
 * @param numRiders Pointer to an integer where the number of riders will be stored.
 * @return An array of MotoGpRider structures.
 */
MotoGpRider *readRiders(const char *filePath, int *numRiders) {
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("Error opening riders file");
        exit(EXIT_FAILURE);
    }

    MotoGpRider *riders = NULL;
    char *line;
    int count = 0;

    while ((line = readUntil(fd, '\n')) != NULL && strlen(line) > 0) {
        riders = realloc(riders, sizeof(MotoGpRider) * (count + 1));

        // Split the fields
        char *token = strtok(line, ",");
        riders[count].firstName = strdup(token);

        token = strtok(NULL, ",");
        riders[count].lastName = strdup(token);

        token = strtok(NULL, ",");
        riders[count].number = atoi(token);

        token = strtok(NULL, ",");
        riders[count].motorcycleBrand = strdup(token);

        free(line);
        count++;
    }

    close(fd);
    *numRiders = count;
    return riders;
}

/**
 * Thread function that simulates laps for a rider.
 *
 * @param arg Pointer to ThreadData structure.
 * @return NULL.
 */
void *simulateLaps(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    MotoGpRider *rider = data->rider;
    MotoGpRiderTime *bestTimes = data->bestTimes;
    int numRiders = data->numRiders;
    char *buffer = NULL;
    int sessionFinished = 0;
    int numberOfLaps = 0;

    // Check if session is over before waiting on semaphore
    pthread_mutex_lock(&mutexSessionOver);
    sessionFinished = sessionOver;
    pthread_mutex_unlock(&mutexSessionOver);

    if (sessionFinished) {
        return NULL;
    }

    SEM_wait(&semaphoreMotoGp);

    // After acquiring the semaphore, check again if session is over
    pthread_mutex_lock(&mutexSessionOver);
    sessionFinished = sessionOver;
    pthread_mutex_unlock(&mutexSessionOver);

    if (sessionFinished) {
        SEM_signal(&semaphoreMotoGp);
        return NULL;
    }

    // Seed for rand_r
    unsigned int seed = time(NULL) ^ (unsigned int)pthread_self();
    asprintf(&buffer, "(%d) %s %s on track\n", rider->number, rider->firstName, rider->lastName);
    printMessage(buffer);
    free(buffer);

    while (sessionFinished == 0 && numberOfLaps < 5) {
        float sector1 = ((float)(rand_r(&seed) % 201) / 1000.0) + 0.3;
        float sector2 = ((float)(rand_r(&seed) % 201) / 1000.0) + 0.3;
        float sector3 = ((float)(rand_r(&seed) % 201) / 1000.0) + 0.3;
        float totalTime = sector1 + sector2 + sector3;

        // Update the best time
        pthread_mutex_lock(&mutexBestTimes);

        // Find the index of the rider
        int index = -1;
        for (int i = 0; i < numRiders; i++) {
            if (bestTimes[i].riderId == rider->number) {
                index = i;
                break;
            }
        }
        if (index != -1) {
            if (bestTimes[index].totalTime < 0 || totalTime < bestTimes[index].totalTime) {
                bestTimes[index].totalTime = totalTime;
                bestTimes[index].sector1 = sector1;
                bestTimes[index].sector2 = sector2;
                bestTimes[index].sector3 = sector3;
            }
        }

        pthread_mutex_unlock(&mutexBestTimes);

        // Sleep for the duration of the lap
        sleep(totalTime);  // Convert seconds to microseconds

        // Check if the session has ended
        pthread_mutex_lock(&mutexSessionOver);
        sessionFinished = sessionOver;
        pthread_mutex_unlock(&mutexSessionOver);
        numberOfLaps++;
    }
    asprintf(&buffer, "(%d) %s %s leaves the track\n", rider->number, rider->firstName, rider->lastName);
    printMessage(buffer);
    free(buffer);
    SEM_signal(&semaphoreMotoGp);
    return NULL;
}


/**
 * Displays the current classification of riders.
 *
 * @param bestTimes Array of MotoGpRiderTime structures with the best times.
 * @param riders Array of MotoGpRider structures.
 * @param numRiders Number of riders.
 */
void showClassification(MotoGpRiderTime *bestTimes, MotoGpRider *riders, int numRiders) {
    char *buffer = NULL;
    pthread_mutex_lock(&mutexBestTimes);
    // Create a copy of bestTimes to sort
    MotoGpRiderTime *sortedTimes = malloc(sizeof(MotoGpRiderTime) * numRiders);
    memcpy(sortedTimes, bestTimes, sizeof(MotoGpRiderTime) * numRiders);

    // Sort the times
    for (int i = 0; i < numRiders - 1; i++) {
        for (int j = 0; j < numRiders - i - 1; j++) {
            if (sortedTimes[j].totalTime < 0 ||
                (sortedTimes[j + 1].totalTime > 0 && sortedTimes[j].totalTime > sortedTimes[j + 1].totalTime)) {
                MotoGpRiderTime temp = sortedTimes[j];
                sortedTimes[j] = sortedTimes[j + 1];
                sortedTimes[j + 1] = temp;
            }
        }
    }
    pthread_mutex_lock(&mutexScreen);
    printF("\n\n=== Current Classification ===\n");

    for (int i = 0; i < numRiders; i++) {
        if (sortedTimes[i].totalTime > 0) {
            // Find the rider
            char *firstName = NULL;
            char *lastName = NULL;
            for (int j = 0; j < numRiders; j++) {
                if (sortedTimes[i].riderId == riders[j].number) {
                    firstName = riders[j].firstName;
                    lastName = riders[j].lastName;
                    break;
                }
            }
            double totalTime = sortedTimes[i].totalTime;
            int seconds = (int)totalTime;
            double fractionalPart = totalTime - seconds;
            int milliseconds = (int)(fractionalPart * 100) % 100; // Get two digits
            fractionalPart = fractionalPart * 100 - milliseconds;
            int nanoseconds = (int)(fractionalPart * 100) % 100; // Get two digits

            asprintf(&buffer, "%d. (%d) %s %s: %02d:%02d:%02d\n",
                     i + 1, sortedTimes[i].riderId, firstName, lastName, seconds, milliseconds, nanoseconds);

            printF(buffer);
            free(buffer);
        }
    }

    free(sortedTimes);
    pthread_mutex_unlock(&mutexBestTimes);
    pthread_mutex_unlock(&mutexScreen);
}

/**
 * Signal handler for session end.
 */
void handleSessionEnd() {
    // Indicate that the session has ended
    pthread_mutex_lock(&mutexSessionOver);
    sessionOver = 1;
    pthread_mutex_unlock(&mutexSessionOver);
}

/**
 * Main function.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status.
 */
int main(int argc, char *argv[]) {
    int sessionOverTemp = 0;
    int numRiders;

    createScreenMutex();
    if (argc != 3) {
        printMessage("Usage: ./S9 <riders_file> <number_of_riders_on_the_track>\n");
        destroyScreenMutex();
        return 1;
    }

    numRidersOnTrack = atoi(argv[2]);
    SEM_constructor(&semaphoreMotoGp);
    SEM_init(&semaphoreMotoGp, numRidersOnTrack);

    pthread_mutex_init(&mutexBestTimes, NULL);
    pthread_mutex_init(&mutexSessionOver, NULL);

    // Ignore Ctrl+C
    signal(SIGINT, SIG_IGN);

    // Read riders
    MotoGpRider *riders = readRiders(argv[1], &numRiders);

    // Initialize best times
    MotoGpRiderTime *bestTimes = malloc(sizeof(MotoGpRiderTime) * numRiders);
    for (int i = 0; i < numRiders; i++) {
        bestTimes[i].riderId = riders[i].number;
        bestTimes[i].totalTime = -1.0;
        bestTimes[i].sector1 = -1.0;
        bestTimes[i].sector2 = -1.0;
        bestTimes[i].sector3 = -1.0;
    }

    // Create threads for each rider
    pthread_t *riderThreads = malloc(sizeof(pthread_t) * numRiders);
    ThreadData *threadDataArray = malloc(sizeof(ThreadData) * numRiders);

    for (int i = 0; i < numRiders; i++) {
        threadDataArray[i].rider = &riders[i];
        threadDataArray[i].bestTimes = bestTimes;
        threadDataArray[i].numRiders = numRiders;

        if (pthread_create(&riderThreads[i], NULL, simulateLaps, (void *)&threadDataArray[i]) != 0) {
            perror("Error creating rider thread");
            return 1;
        }
    }

    // Set up signal handler for SIGALRM only for the main thread
    signal(SIGALRM, handleSessionEnd);

    // Set alarm for 10 seconds
    alarm(10);

    // Main loop
    while (sessionOverTemp == 0) {
        showClassification(bestTimes, riders, numRiders);

        // Check if the session has ended
        pthread_mutex_lock(&mutexSessionOver);
        sessionOverTemp = sessionOver;
        pthread_mutex_unlock(&mutexSessionOver);
        sleep(3);
    }

    // Wait for all threads to finish
    for (int i = 0; i < numRiders; i++) {
        pthread_join(riderThreads[i], NULL);
    }

    // Show final classification
    showClassification(bestTimes, riders, numRiders);
    printMessage("The session has ended.\n");

    // Free resources
    free(riderThreads);
    free(threadDataArray);
    for (int i = 0; i < numRiders; i++) {
        free(riders[i].firstName);
        free(riders[i].lastName);
        free(riders[i].motorcycleBrand);
    }
    free(riders);
    free(bestTimes);

    pthread_mutex_destroy(&mutexBestTimes);
    pthread_mutex_destroy(&mutexSessionOver);
    destroyScreenMutex();
    SEM_destructor(&semaphoreMotoGp);

    return 0;
}