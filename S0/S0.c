// login: karol.korszun
// name: Karol
// lastname: Korszun


#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Product {
    char name[100];
    char category[50];
    int max_stock;
    int stock;
    float price;
    int supplier_id;
};

struct Supplier {
    int id;
    char name[100];
    char email[100];
    char city[50];
    char street[100];
};

void* read_binary_file(const char* filename, int* count, size_t size) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return NULL;

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    *count = file_size / size;
    void* data = malloc(file_size);
    if (!data || read(fd, data, file_size) != file_size) {
        free(data);
        close(fd);
        return NULL;
    }
    close(fd);
    return data;
}

struct Supplier* read_suppliers(const char* filename, int* num_suppliers) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return NULL;

    struct Supplier* suppliers = NULL;
    *num_suppliers = 0;
    char buffer[1024];
    ssize_t bytes_read;
    size_t buffer_pos = 0;

    while ((bytes_read = read(fd, buffer + buffer_pos, sizeof(buffer) - buffer_pos)) > 0) {
        buffer_pos += bytes_read;
        char* line_start = buffer;
        char* line_end;

        while ((line_end = memchr(line_start, '\n', buffer_pos - (line_start - buffer)))) {
            *line_end = '\0';
            *num_suppliers += 1;
            struct Supplier* new_suppliers = realloc(suppliers, *num_suppliers * sizeof(struct Supplier));
            if (!new_suppliers) {
                free(suppliers);
                close(fd);
                return NULL;
            }
            suppliers = new_suppliers;

            struct Supplier* current = &suppliers[*num_suppliers - 1];
            sscanf(line_start, "%d&%[^&]&%[^&]&%[^&]&%[^\n]",
                   &current->id, current->name, current->email, current->city, current->street);

            line_start = line_end + 1;
        }

        memmove(buffer, line_start, buffer_pos - (line_start - buffer));
        buffer_pos -= (line_start - buffer);
    }

    close(fd);
    return suppliers;
}

size_t str_len(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void str_copy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

void int_to_str(int num, char* str) {
    int i = 0, is_negative = 0;
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    do {
        str[i++] = num % 10 + '0';
        num /= 10;
    } while (num > 0);
    if (is_negative) str[i++] = '-';
    str[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char temp = str[j];
        str[j] = str[i - 1 - j];
        str[i - 1 - j] = temp;
    }
}

void write_str(int fd, const char* str) {
    write(fd, str, str_len(str));
}

void format_product_line(char* buffer, size_t buffer_size, const struct Product* product) {
    str_copy(buffer, product->name, buffer_size);
    str_copy(buffer + str_len(buffer), " - Stock: ", buffer_size - str_len(buffer));
    char stock_str[20];
    int_to_str(product->stock, stock_str);
    str_copy(buffer + str_len(buffer), stock_str, buffer_size - str_len(buffer));
    str_copy(buffer + str_len(buffer), " - Category: ", buffer_size - str_len(buffer));
    str_copy(buffer + str_len(buffer), product->category, buffer_size - str_len(buffer));
    str_copy(buffer + str_len(buffer), "\n", buffer_size - str_len(buffer));
}

void format_supplier_line(char* buffer, size_t buffer_size, const struct Supplier* supplier) {
    str_copy(buffer, supplier->name, buffer_size);
    str_copy(buffer + str_len(buffer), " (", buffer_size - str_len(buffer));
    str_copy(buffer + str_len(buffer), supplier->email, buffer_size - str_len(buffer));
    str_copy(buffer + str_len(buffer), "):\n", buffer_size - str_len(buffer));
}

void generate_current_stock_report(struct Product* products, int product_count) {
    int fd_current = open("current_stock.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_current == -1) {
        write(STDERR_FILENO, "Error opening current_stock.txt\n", 30);
        return;
    }

    char buffer[256];

    for (int i = 0; i < product_count; i++) {
        if (products[i].stock >= 10) {
            format_product_line(buffer, sizeof(buffer), &products[i]);
            write_str(fd_current, buffer);
            write_str(STDOUT_FILENO, buffer);
        }
    }

    close(fd_current);
}

void generate_demand_report(struct Product* products, int product_count,
                            struct Supplier* suppliers, int supplier_count) {
    int fd_demand = open("demand.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_demand == -1) {
        write(STDERR_FILENO, "Error opening demand.txt\n", 24);
        return;
    }

    char buffer[256];

    for (int i = 0; i < supplier_count; i++) {
        int supplier_has_products = 0;
        for (int j = 0; j < product_count; j++) {
            if (products[j].supplier_id == suppliers[i].id && products[j].stock < 10) {
                supplier_has_products = 1;
                break;
            }
        }

        if (supplier_has_products) {
            format_supplier_line(buffer, sizeof(buffer), &suppliers[i]);
            write_str(fd_demand, buffer);
            write_str(STDOUT_FILENO, buffer);

            char current_category[50] = "";
            for (int j = 0; j < product_count; j++) {
                if (products[j].supplier_id == suppliers[i].id && products[j].stock < 10) {
                    if (strcmp(current_category, products[j].category) != 0) {
                        str_copy(current_category, products[j].category, sizeof(current_category));
                        str_copy(buffer, "\t", sizeof(buffer));
                        str_copy(buffer + str_len(buffer), current_category, sizeof(buffer) - str_len(buffer));
                        str_copy(buffer + str_len(buffer), ":\n", sizeof(buffer) - str_len(buffer));
                        write_str(fd_demand, buffer);
                        write_str(STDOUT_FILENO, buffer);
                    }
                    int restock = products[j].max_stock - products[j].stock;
                    str_copy(buffer, "\t\t- ", sizeof(buffer));
                    str_copy(buffer + str_len(buffer), products[j].name, sizeof(buffer) - str_len(buffer));
                    str_copy(buffer + str_len(buffer), " - Stock: ", sizeof(buffer) - str_len(buffer));
                    char stock_str[20];
                    int_to_str(products[j].stock, stock_str);
                    str_copy(buffer + str_len(buffer), stock_str, sizeof(buffer) - str_len(buffer));
                    str_copy(buffer + str_len(buffer), " - Restock: ", sizeof(buffer) - str_len(buffer));
                    int_to_str(restock, stock_str);
                    str_copy(buffer + str_len(buffer), stock_str, sizeof(buffer) - str_len(buffer));
                    str_copy(buffer + str_len(buffer), "\n", sizeof(buffer) - str_len(buffer));
                    write_str(fd_demand, buffer);
                    write_str(STDOUT_FILENO, buffer);
                }
            }
            str_copy(buffer, "\n", sizeof(buffer));
            write_str(fd_demand, buffer);
            write_str(STDOUT_FILENO, buffer);
        }
    }

    close(fd_demand);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write(STDERR_FILENO, "Usage: ./S0 <product_file> <supplier_file>\n", 44);
        return 1;
    }

    int product_count, supplier_count;
    struct Product* products = read_binary_file(argv[1], &product_count, sizeof(struct Product));
    struct Supplier* suppliers = read_suppliers(argv[2], &supplier_count);

    if (!products || !suppliers) {
        write(STDERR_FILENO, "Error reading files\n", 20);
        free(products);
        free(suppliers);
        return 1;
    }

    generate_current_stock_report(products, product_count);
    generate_demand_report(products, product_count, suppliers, supplier_count);

    free(products);
    free(suppliers);
    return 0;
}