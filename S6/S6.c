// name: Karol
// Last name: Korszun
// login: korszun.karol


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>

#define NUM_STATIONS 3
#define EVENTS_PER_STATION 10
#define ARRIVAL_REPORT 1
#define DEPARTURE_REPORT 2
#define ERROR_MSG_SHM "Error creating shared memory\n"
#define ERROR_MSG_PIPE "Error creating pipe\n"
#define ERROR_MSG_PROCESS "Error creating process\n"

#define printF(x) write(1, x, strlen(x))

struct station_data {
    int passengers;
};

struct shared_memory {
    int shmid;
    struct station_data *data;
};

void handle_station(int station_id, int write_fd, struct station_data *shared_mem);
void create_station_processes(int pipes[][2], int station_pids[], struct shared_memory *mem);
void process_station_event(int station_id, int *fd, struct station_data *shared_mem);
void monitor_stations(int pipes[][2], struct station_data *shared_mem);
void cleanup(struct shared_memory *mem, int pipes[][2]);

struct shared_memory create_shared_memory() {
    struct shared_memory mem = {0};

    mem.shmid = shmget(IPC_PRIVATE, sizeof(struct station_data) * NUM_STATIONS, IPC_CREAT | 0600);
    if (mem.shmid == -1) {
        printF(ERROR_MSG_SHM);
        exit(1);
    }

    mem.data = shmat(mem.shmid, NULL, 0);
    if (mem.data == (void *)-1) {
        shmctl(mem.shmid, IPC_RMID, NULL);
        printF(ERROR_MSG_SHM);
        exit(1);
    }

    memset(mem.data, 0, sizeof(struct station_data) * NUM_STATIONS);
    return mem;
}

void create_station_processes(int pipes[][2], int station_pids[], struct shared_memory *mem) {
    for (int i = 0; i < NUM_STATIONS; i++) {
        if (pipe(pipes[i]) == -1) {
            printF(ERROR_MSG_PIPE);
            exit(1);
        }

        station_pids[i] = fork();
        if (station_pids[i] == -1) {
            printF(ERROR_MSG_PROCESS);
            exit(1);
        }

        if (station_pids[i] == 0) {
            close(pipes[i][0]);
            handle_station(i, pipes[i][1], mem->data);
        }
        close(pipes[i][1]);
    }
}

void handle_station(int station_id, int write_fd, struct station_data *shared_mem) {
    for (int i = 3; i < 256; i++) {
        if (i != write_fd) {
            close(i);
        }
    }

    srand(time(NULL) + station_id);

    for (int i = 0; i < EVENTS_PER_STATION; i++) {
        sleep(rand() % 3 + 1);

        int event_type = (rand() % 2) + 1;
        int passenger_change = rand() % 41 + 10;

        if (event_type == ARRIVAL_REPORT) {
            shared_mem[station_id].passengers += passenger_change;
        } else {
            if (passenger_change > shared_mem[station_id].passengers) {
                passenger_change = shared_mem[station_id].passengers;
            }
            shared_mem[station_id].passengers -= passenger_change;
        }

        write(write_fd, &event_type, sizeof(int));
    }

    int end_signal = -1;
    write(write_fd, &end_signal, sizeof(int));
    close(write_fd);
    exit(0);
}

void process_station_event(int station_id, int *fd, struct station_data *shared_mem) {
    int event_type;
    int nbytes = read(*fd, &event_type, sizeof(int));

    if (nbytes <= 0 || event_type == -1) {
        close(*fd);
        *fd = -1;
        char *message;
        int len = asprintf(&message,
            "[Control Center] Station %d has completed all operations.\n",
            station_id + 1);
        write(1, message, len);
        free(message);
        return;
    }

    char *event_name;
    if (event_type == ARRIVAL_REPORT) {
        event_name = "Train arrival";
    } else {
        event_name = "Train departure";
    }

    char *message;
    int len = asprintf(&message,
        "[Control Center] Station %d - %s. Passengers at station: %d\n",
        station_id + 1,
        event_name,
        shared_mem[station_id].passengers);
    write(1, message, len);
    free(message);
}

void monitor_stations(int pipes[][2], struct station_data *shared_mem) {
    fd_set read_fds;
    int active_stations = NUM_STATIONS;
    int max_fd;

    while (active_stations > 0) {
        FD_ZERO(&read_fds);
        max_fd = -1;

        for (int i = 0; i < NUM_STATIONS; i++) {
            if (pipes[i][0] != -1) {
                FD_SET(pipes[i][0], &read_fds);
                if (pipes[i][0] > max_fd) max_fd = pipes[i][0];
            }
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) > 0) {
            for (int i = 0; i < NUM_STATIONS; i++) {
                if (pipes[i][0] != -1 && FD_ISSET(pipes[i][0], &read_fds)) {
                    process_station_event(i, &pipes[i][0], shared_mem);
                    if (pipes[i][0] == -1) {
                        active_stations--;
                    }
                }
            }
        }
    }

    char *final_message = "\n[Control Center] All stations have completed their operations. Shutting down system.\n\n";
    write(1, final_message, strlen(final_message));

    for (int i = 0; i < NUM_STATIONS; i++) {
        char *stats;
        int len = asprintf(&stats,
            "[Control Center] Final passenger count at Station %d: %d\n",
            i + 1,
            shared_mem[i].passengers);
        write(1, stats, len);
        free(stats);
    }
    write(1, "\n", 1);
}

void cleanup(struct shared_memory *mem, int pipes[][2]) {
    for (int i = 0; i < NUM_STATIONS; i++) {
        if (pipes[i][0] >= 0) {
            close(pipes[i][0]);
            pipes[i][0] = -1;
        }
    }

    for (int i = 0; i < NUM_STATIONS; i++) {
        wait(NULL);
    }

    if (mem->data != NULL) {
        shmdt(mem->data);
        mem->data = NULL;
    }

    if (mem->shmid != -1) {
        shmctl(mem->shmid, IPC_RMID, NULL);
        mem->shmid = -1;
    }
}

int main() {
    int pipes[NUM_STATIONS][2];
    int station_pids[NUM_STATIONS];

    struct shared_memory mem = create_shared_memory();
    create_station_processes(pipes, station_pids, &mem);
    monitor_stations(pipes, mem.data);
    cleanup(&mem, pipes);

    return 0;
}