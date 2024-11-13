// login: karol.korszun@students.salle.url.edu
// first_name: Karol
// last_name: Korszun


#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#define SOLAR_STORM_DURATION 10
#define MAX_MSG_LEN 256

volatile sig_atomic_t last_signal = 0;
volatile sig_atomic_t last_signal_time = 0;
volatile sig_atomic_t last_state = 0;
volatile sig_atomic_t solar_storm_active = 0;
volatile sig_atomic_t rescue_successful = 0;

void write_msg(int fd, const char *msg) {
    write(fd, msg, strlen(msg));
}

void write_status(int fd, const char *time_str, const char *state) {
    if (time_str) {
        write(fd, time_str, strlen(time_str));
    }
    write(fd, state, strlen(state));
}

void ksigusr1(int signum) {
    (void)signum;
    time_t current_time = time(NULL);

    if (last_signal == SIGUSR2 && current_time - last_signal_time <= 5) {
        last_state = 1;
        write_msg(STDOUT_FILENO, "Critical oxygen state detected.\n");
    } else {
        last_state = 0;
        write_msg(STDOUT_FILENO, "Oxygen state stabilized.\n");
    }
}

void ksigusr2(int signum) {
    (void)signum;
    if (last_signal == SIGUSR2) {
        last_state = 2;
        write_msg(STDOUT_FILENO, "Energy failure detected.\n");
    }
}

void ksigalrm(int signum) {
    (void)signum;
    if (!solar_storm_active) {
        solar_storm_active = 1;
        write_msg(STDOUT_FILENO, "Solar storm detected. All systems paused.\n");
        alarm(SOLAR_STORM_DURATION);
    } else {
        solar_storm_active = 0;
        write_msg(STDOUT_FILENO, "End of solar storm. All systems operational. Signals unblocked.\n");
    }
}

void ksighup(int signum) {
    (void)signum;
    time_t current_time = time(NULL);
    char *time_str = ctime(&current_time);
    const char *state_msg;

    switch (last_state) {
        case 1:
            state_msg = "Report: Critical oxygen level.\n";
            break;
        case 2:
            state_msg = "Report: Energy failure.\n";
            break;
        default:
            state_msg = "Report: Normal state.\n";
    }

    int fd = open("drone_state.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        write_status(fd, time_str, state_msg);
        close(fd);
    }
    write_msg(STDOUT_FILENO, state_msg);
}

void ksigint(int signum) {
    (void)signum;
    rescue_successful = 1;
    write_msg(STDOUT_FILENO, "Rescue mission successful.\n");
}

void ksighandler(int signum) {
    if (solar_storm_active && signum != SIGALRM) {
        write_msg(STDOUT_FILENO, "Solar storm detected.\n");
        return;
    }

    switch (signum) {
        case SIGUSR1:
            ksigusr1(signum);
            break;
        case SIGUSR2:
            ksigusr2(signum);
            break;
        case SIGALRM:
            ksigalrm(signum);
            break;
        case SIGHUP:
            ksighup(signum);
            break;
        case SIGINT:
            ksigint(signum);
            break;
    }
    last_signal = signum;
    last_signal_time = time(NULL);
}

int main() {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = ksighandler;

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    pid_t pid = getpid();
    char pid_str[16];
    int pid_len = snprintf(pid_str, sizeof(pid_str), "%d", pid);

    write_msg(STDOUT_FILENO, "Process ID: ");
    write(STDOUT_FILENO, pid_str, pid_len);
    write_msg(STDOUT_FILENO, "\n");

    write_msg(STDOUT_FILENO, "Rescue Drone AION initialized, waiting for signals...\n");

    while (!rescue_successful) {
        pause();
    }

    return 0;
}