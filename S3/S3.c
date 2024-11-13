// login: korszun.karol
// name: Karol
// last name: Korszun

#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 1
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#define printF(x) write(1, x, strlen(x))



typedef struct {
    double *data;
    int size;
    double result;
} ThreadData;

double* read_file(const char* filename, int* size) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        write(STDERR_FILENO, "Error: Unable to open the file\n", 31);
        return NULL;
    }

    double* data = NULL;
    *size = 0;
    char buffer[64];
    int bytes_read, index = 0;

    while ((bytes_read = read(fd, &buffer[index], 1)) > 0) {
        if (buffer[index] == '\n') {
            buffer[index] = '\0';
            data = realloc(data, (*size + 1) * sizeof(double));
            if (!data) {
                write(STDERR_FILENO, "Error: Memory allocation failed\n", 32);
                close(fd);
                return NULL;
            }
            data[(*size)++] = atof(buffer);
            index = 0;
        } else if (index < 63) {
            index++;
        }
    }

    close(fd);
    return data;
}

void* calculate_mean(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    double sum = 0;
    for (int i = 0; i < data->size; i++) {
        sum += data->data[i];
    }
    data->result = sum / data->size;
    return NULL;
}

void* calculate_median(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    double* temp = malloc(data->size * sizeof(double));
    memcpy(temp, data->data, data->size * sizeof(double));

    // Bubble sort algorithm
    for (int i = 0; i < data->size - 1; i++)
        for (int j = 0; j < data->size - i - 1; j++)
            if (temp[j] > temp[j + 1]) {
                double t = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = t;
            }

    // when the size is even, the median is the average of the two middle numbers
    // when the size is odd, the median is the middle number
    if (data->size % 2 == 0)
        data->result = (temp[data->size/2 - 1] + temp[data->size/2]) / 2;
    else
        data->result = temp[data->size/2];
    free(temp);
    return NULL;
}

void* calculate_maximum(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    double max = data->data[0];
    for (int i = 1; i < data->size; i++)
        if (data->data[i] > max) max = data->data[i];
    data->result = max;
    return NULL;
}

void* calculate_variance(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    double mean = 0, sum_squared_diff = 0;
    for (int i = 0; i < data->size; i++) mean += data->data[i];
    mean /= data->size;
    for (int i = 0; i < data->size; i++) {
        double diff = data->data[i] - mean;
        sum_squared_diff += diff * diff;
    }
    data->result = sum_squared_diff / data->size;
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        write(STDERR_FILENO, "Usage: ./S3 <filename>\n", 23);
        return EXIT_FAILURE;
    }

    signal(SIGINT, SIG_IGN);

    int size;
    double* data = read_file(argv[1], &size);
    if (!data) {
        return EXIT_FAILURE;
    }
    ThreadData thread_data[4] = {{data, size, 0}, {data, size, 0}, {data, size, 0}, {data, size, 0}};
    pthread_t t1, t2, t3, t4;
    int s;

    s = pthread_create(&t1, NULL, calculate_mean, &thread_data[0]);
    if (s != 0) {
        const char *error_msg = "Error: pthread_create (mean)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_create(&t2, NULL, calculate_median, &thread_data[1]);
    if (s != 0) {
        const char *error_msg = "Error: pthread_create (median)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_create(&t3, NULL, calculate_maximum, &thread_data[2]);
    if (s != 0) {
        const char *error_msg = "Error: pthread_create (maximum)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_create(&t4, NULL, calculate_variance, &thread_data[3]);
    if (s != 0) {
        const char *error_msg = "Error: pthread_create (variance)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_join(t1, NULL);
    if (s != 0) {
        const char *error_msg = "Error: pthread_join (mean)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_join(t2, NULL);
    if (s != 0) {
        const char *error_msg = "Error: pthread_join (median)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_join(t3, NULL);
    if (s != 0) {
        const char *error_msg = "Error: pthread_join (maximum)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }

    s = pthread_join(t4, NULL);
    if (s != 0) {
        const char *error_msg = "Error: pthread_join (variance)\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        free(data);
        return EXIT_FAILURE;
    }


    char *msg;
    asprintf(&msg, "Mean: %.6f\nMedian: %.6f\nMaximum value: %.6f\nVariance: %.6f\n",
        thread_data[0].result, thread_data[1].result, thread_data[2].result, thread_data[3].result);
    printF(msg);

    free(data);
    return EXIT_SUCCESS;
}