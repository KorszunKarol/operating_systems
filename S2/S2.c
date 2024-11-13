// name: Karol
// surname: Korszun
// login: karol.korszun

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define STRINGS 0
#define WINDS 1
#define PERCUSSION 2

#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define RESET "\e[0m"

volatile sig_atomic_t section_start = 0;
volatile sig_atomic_t current_section = 0;

void signal_handler(int signum) {
    switch (signum) {
        case SIGUSR1:
            section_start = 1;
            break;
        case SIGINT:
            write(STDOUT_FILENO, RED "\nConcert interrupted. Cleaning up...\n" RESET, 47);
            kill(0, SIGTERM);
            while (wait(NULL) > 0);
            exit(EXIT_SUCCESS);
        default:
            write(STDOUT_FILENO, YEL "Received unexpected signal\n" RESET, 38);
            break;
    }
}

void play_instrument(char* section, char* instrument, char* notes, char* color) {
    char message[150];
    int len = snprintf(message, sizeof(message), "%s%s %s is playing: %s%s\n", color, section, instrument, notes, RESET);
    write(STDOUT_FILENO, message, len);
    sleep(2);
    exit(EXIT_SUCCESS);
}

void string_section() {
    pid_t viola, violin1, violin2;

    viola = fork();
    if (viola == 0) {
        play_instrument("Strings", "Viola", "Do", GRN);
    } else if (viola < 0) {
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    violin1 = fork();
    if (violin1 == 0) {
        play_instrument("Strings", "Violin 1", "Re", GRN);
    } else if (violin1 < 0) {
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    violin2 = fork();
    if (violin2 == 0) {
        play_instrument("Strings", "Violin 2", "Re", GRN);
    } else if (violin2 < 0) {
        
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 3; i++) {
        if (wait(NULL) == -1) {
            write(STDERR_FILENO, RED "Wait failed in string section\n" RESET, 41);
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}

void wind_section() {
    pid_t flute, clarinet;

    flute = fork();
    if (flute == 0) {
        play_instrument("Wind", "Flute", "Do Do", BLU);
    } else if (flute < 0) {
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    clarinet = fork();
    if (clarinet == 0) {
        play_instrument("Wind", "Clarinet 1", "Re Re", BLU);
    } else if (clarinet < 0) {
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 2; i++) {
        if (wait(NULL) == -1) {
            write(STDERR_FILENO, RED "Wait failed in wind section\n" RESET, 39);
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}

void percussion_section() {
    pid_t triangle, vibraphone;

    triangle = fork();
    if (triangle == 0) {
        play_instrument("Percussion", "Triangle", "Do Re Mi", MAG);
    } else if (triangle < 0) {
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    vibraphone = fork();
    if (vibraphone == 0) {
        play_instrument("Percussion", "Vibraphone", "Do Re Re Mi", MAG);
    } else if (vibraphone < 0) {
        write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 2; i++) {
        if (wait(NULL) == -1) {
            write(STDERR_FILENO, RED "Wait failed in percussion section\n" RESET, 44);
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    (void)argv;
    if (argc != 1) {
        write(STDERR_FILENO, RED "Usage: ./S2\n" RESET, 23);
        exit(EXIT_FAILURE);
    }

    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        write(STDERR_FILENO, RED "Failed to set SIGUSR1 handler\n" RESET, 42);
        exit(EXIT_FAILURE);
    }
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        write(STDERR_FILENO, RED "Failed to set SIGINT handler\n" RESET, 41);
        exit(EXIT_FAILURE);
    }

    char message[150];
    int len = snprintf(message, sizeof(message), YEL "Director (PID %d) starting the concert. Use 'kill -SIGUSR1 %d' to start sections\n" RESET, getpid(), getpid());
    write(STDOUT_FILENO, message, len);

    pid_t sections[3];

    for (int i = 0; i < 3; i++) {
        while (!section_start) {
            pause();
        }
        section_start = 0;

        char* section_name;
        if (i == STRINGS) {
            section_name = "Strings";
        } else if (i == WINDS) {
            section_name = "Wind";
        } else {
            section_name = "Percussion";
        }
        write(STDOUT_FILENO, YEL "Section is ready to start\n" RESET, 37);

        sections[i] = fork();
        if (sections[i] == 0) {
            if (i == STRINGS){
                string_section();
            }
            else if (i == WINDS) {
                wind_section();
            }
            else {
                percussion_section();
            }
        } else if (sections[i] < 0) {
            write(STDERR_FILENO, RED "Fork failed\n" RESET, 23);
            exit(EXIT_FAILURE);
        }

        if (wait(NULL) == -1) {
            write(STDERR_FILENO, RED "Wait failed in main\n" RESET, 32);
            exit(EXIT_FAILURE);
        }

        len = snprintf(message, sizeof(message), RESET "Director: %s section completed.\n" RESET, section_name);
        write(STDOUT_FILENO, message, len);

        if (i < 2) {
            char* next_section = (i == STRINGS) ? "Wind" : "Percussion";
            len = snprintf(message, sizeof(message), YEL "Waiting to start %s section\n" RESET, next_section);
            write(STDOUT_FILENO, message, len);
        }
    }

    char *final_message = RESET "Director: Concert finished successfully\n" RESET;
    write(STDOUT_FILENO, final_message, strlen(final_message));

    return 0;
}