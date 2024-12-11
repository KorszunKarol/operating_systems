// name: Karol
// last name: Korszun
// login: karol.korszun

#include <sys/types.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "semaphore_v2.h"

#define MAX_PILOTS 50
#define MAX_LAPS 5
#define SESSION_DURATION 10
#define DISPLAY_INTERVAL 3
#define MAX_LINE 256

#define printF(x) write(1, x, strlen(x))

typedef struct {
    char firstName[50];
    char lastName[50];
    int number;
    char team[100];
    long bestTime;
    int lapsCompleted;
    int isOnTrack;
} Pilot;

typedef struct {
    Pilot* pilots;
    int numPilots;
    int maxOnTrack;
    semaphore* trackAccess;
    pthread_mutex_t standingsMutex;
    int sessionActive;
} SessionData;

SessionData session;

void nothing() {
    signal(SIGINT, nothing);
}

void setupSignals() {
    signal(SIGINT, nothing);
}

char* read_until(int fd, char end) {
    char* string = NULL;
    char c;
    int i = 0, size;

    while (read(fd, &c, 1) > 0 && c != end) {
        if (i % 10 == 0) {
            size = i + 10;
            string = realloc(string, size);
        }
        string[i++] = c;
    }

    if (i % 10 == 0) {
        size = i + 1;
        string = realloc(string, size);
    }
    string[i] = '\0';

    return string;
}

Pilot* readPilotsFile(char* filename, int* numPilots) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printF("Error opening pilots file\n");
        exit(1);
    }

    Pilot* pilots = malloc(MAX_PILOTS * sizeof(Pilot));
    *numPilots = 0;

    while (1) {
        char* firstName = read_until(fd, ',');
        if (strlen(firstName) == 0) {
            free(firstName);
            break;
        }

        strcpy(pilots[*numPilots].firstName, firstName);
        free(firstName);

        char* lastName = read_until(fd, ',');
        strcpy(pilots[*numPilots].lastName, lastName);
        free(lastName);

        char* number = read_until(fd, ',');
        pilots[*numPilots].number = atoi(number);
        free(number);

        char* team = read_until(fd, '\n');
        strcpy(pilots[*numPilots].team, team);
        free(team);

        pilots[*numPilots].bestTime = -1;
        pilots[*numPilots].lapsCompleted = 0;
        pilots[*numPilots].isOnTrack = 0;

        (*numPilots)++;
    }

    close(fd);
    return pilots;
}

long generateLapTime() {
    long sector1 = 25 + (rand() % 30);
    long sector2 = 28 + (rand() % 25);
    long sector3 = 22 + (rand() % 35);
    return sector1 + sector2 + sector3;
}

void formatTime(long time, char* buffer) {
    int minutes = time / 6000;
    int seconds = (time % 6000) / 100;
    int milliseconds = (time % 100) * 10;

    buffer[0] = '0' + (minutes / 10);
    buffer[1] = '0' + (minutes % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (seconds / 10);
    buffer[4] = '0' + (seconds % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (milliseconds / 100);
    buffer[7] = '0' + ((milliseconds / 10) % 10);
    buffer[8] = '0' + (milliseconds % 10);
    buffer[9] = '\0';
}
char* formatPilotMessage(Pilot* pilot, const char* action) {
    char* buffer = malloc(256);
    char numStr[20];

    int num = pilot->number;
    int i = 0;
    do {
        numStr[i++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0);
    numStr[i] = '\0';

    for (int j = 0; j < i/2; j++) {
        char temp = numStr[j];
        numStr[j] = numStr[i-1-j];
        numStr[i-1-j] = temp;
    }

    strcpy(buffer, "(");
    strcat(buffer, numStr);
    strcat(buffer, ") ");
    strcat(buffer, pilot->firstName);
    strcat(buffer, " ");
    strcat(buffer, pilot->lastName);
    strcat(buffer, " ");
    strcat(buffer, action);
    strcat(buffer, "\n");

    return buffer;
}

char* formatStandingLine(int rank, Pilot* pilot, char* timeStr) {
    char* buffer = malloc(256);
    char rankStr[10];
    char numStr[10];

    int i = 0;
    int temp = rank;
    do {
        rankStr[i++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    rankStr[i] = '\0';
    for (int j = 0; j < i/2; j++) {
        char t = rankStr[j];
        rankStr[j] = rankStr[i-1-j];
        rankStr[i-1-j] = t;
    }

    i = 0;
    temp = pilot->number;
    do {
        numStr[i++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    numStr[i] = '\0';
    for (int j = 0; j < i/2; j++) {
        char t = numStr[j];
        numStr[j] = numStr[i-1-j];
        numStr[i-1-j] = t;
    }

    strcpy(buffer, rankStr);
    strcat(buffer, ". (");
    strcat(buffer, numStr);
    strcat(buffer, ") ");
    strcat(buffer, pilot->firstName);
    strcat(buffer, " ");
    strcat(buffer, pilot->lastName);
    strcat(buffer, ": ");
    strcat(buffer, timeStr);
    strcat(buffer, "\n");

    return buffer;
}

void displayStandings() {
    pthread_mutex_lock(&session.standingsMutex);

    Pilot** sortedPilots = malloc(session.numPilots * sizeof(Pilot*));
    for (int i = 0; i < session.numPilots; i++) {
        sortedPilots[i] = &session.pilots[i];
    }

    for (int i = 0; i < session.numPilots - 1; i++) {
        for (int j = 0; j < session.numPilots - i - 1; j++) {
            if ((sortedPilots[j]->bestTime == -1) ||
                (sortedPilots[j+1]->bestTime != -1 && sortedPilots[j]->bestTime > sortedPilots[j+1]->bestTime)) {
                Pilot* temp = sortedPilots[j];
                sortedPilots[j] = sortedPilots[j+1];
                sortedPilots[j+1] = temp;
            }
        }
    }

    int rank = 1;
    char* timeStr = malloc(20);
    char* output;

    for (int i = 0; i < session.numPilots; i++) {
        if (sortedPilots[i]->bestTime != -1) {
            formatTime(sortedPilots[i]->bestTime, timeStr);
            output = formatStandingLine(rank++, sortedPilots[i], timeStr);
            printF(output);
            free(output);
        }
    }

    free(timeStr);
    free(sortedPilots);
    pthread_mutex_unlock(&session.standingsMutex);
}

void* displayThread(void* arg) {
    (void)arg;
    while (session.sessionActive) {
        sleep(DISPLAY_INTERVAL);
        printF("\n=== Current Classification ===\n");
        displayStandings();
    }
    return NULL;
}

void* pilotThread(void* arg) {
    Pilot* pilot = (Pilot*)arg;
    char* buffer;

    if (session.sessionActive) {
        SEM_wait(session.trackAccess);

        if (!session.sessionActive) {
            SEM_signal(session.trackAccess);
            return NULL;
        }

        pilot->isOnTrack = 1;
        buffer = formatPilotMessage(pilot, "on track");
        printF(buffer);
        free(buffer);

        int completed_laps = 0;
        while (session.sessionActive && completed_laps < MAX_LAPS) {
            long lapTime = generateLapTime();

            pthread_mutex_lock(&session.standingsMutex);
            if (pilot->bestTime == -1 || lapTime < pilot->bestTime) {
                pilot->bestTime = lapTime;
            }
            completed_laps++;
            pilot->lapsCompleted = completed_laps;
            pthread_mutex_unlock(&session.standingsMutex);

            usleep(lapTime * 10000);
        }

        if (completed_laps == MAX_LAPS) {
            pilot->isOnTrack = 0;
            buffer = formatPilotMessage(pilot, "leaves the track");
            printF(buffer);
            free(buffer);
        }

        SEM_signal(session.trackAccess);
    }

    return NULL;
}

int allRidersCompleted() {
    for (int i = 0; i < session.numPilots; i++) {
        if (session.pilots[i].lapsCompleted < MAX_LAPS) {
            return 0;
        }
    }
    return 1;
}

void cleanup() {
    SEM_destructor(session.trackAccess);
    free(session.trackAccess);
    pthread_mutex_destroy(&session.standingsMutex);
    free(session.pilots);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printF("Usage: ./S9 <riders_file> <number_of_pilots_on_the_track>\n");
        exit(1);
    }

    srand(time(NULL));
    setupSignals();

    session.maxOnTrack = atoi(argv[2]);
    if (session.maxOnTrack <= 0) {
        printF("Invalid number of pilots on track\n");
        exit(1);
    }

    session.pilots = readPilotsFile(argv[1], &session.numPilots);

    session.trackAccess = malloc(sizeof(semaphore));
    key_t key = ftok("/tmp", 'S');
    if (SEM_constructor_with_name(session.trackAccess, key) < 0) {
        printF("Error creating semaphore\n");
        exit(1);
    }
    SEM_init(session.trackAccess, session.maxOnTrack);

    pthread_mutex_init(&session.standingsMutex, NULL);
    session.sessionActive = 1;

    pthread_t* pilotThreads = malloc(session.numPilots * sizeof(pthread_t));
    pthread_t displayThreadId;

    pthread_create(&displayThreadId, NULL, displayThread, NULL);

    for (int i = 0; i < session.numPilots; i++) {
        pthread_create(&pilotThreads[i], NULL, pilotThread, (void*)&session.pilots[i]);
    }

    time_t start_time = time(NULL);
    while (!allRidersCompleted() &&
           difftime(time(NULL), start_time) < SESSION_DURATION) {
        sleep(1);
    }
    session.sessionActive = 0;

    for (int i = 0; i < session.numPilots; i++) {
        pthread_join(pilotThreads[i], NULL);
    }
    pthread_join(displayThreadId, NULL);

    printF("\n=== Final Classification ===\n");
    displayStandings();
    printF("The session has ended.\n");

    cleanup();
    free(pilotThreads);

    return 0;
}
