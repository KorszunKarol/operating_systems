//
// login: karol.korszun
// name: Karol
// surname: Korszun


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>

struct Product {
    char product_name[100];
    char category[50];
    int max_stock;
    int stock_quantity;
    float unit_price;
    int supplier_id;
};

struct Supplier {
    int ID;
    char supplierName[100];
    char email[100];
    char city[50];
    char street[100];
};

struct Supplier* read_suppliers(const char* filename, int* num_suppliers, size_t max_line_length) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }

    struct Supplier* suppliers = NULL;
    *num_suppliers = 0;
    char* line = malloc(max_line_length);
    if (!line) {
        close(fd);
        return NULL;
    }

    size_t line_pos = 0;
    char c;

    while (1) {
        ssize_t bytes_read = read(fd, &c, 1);
        if (bytes_read == 0) break;
        if (bytes_read == -1) {
            const char* error_msg = "Error reading file\n";
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            free(line);
            if (suppliers) free(suppliers);
            close(fd);
            return NULL;
        }

        if (c == '\n' || line_pos == max_line_length - 1) {
            line[line_pos] = '\0';
            *num_suppliers += 1;
            struct Supplier* new_suppliers = realloc(suppliers, *num_suppliers * sizeof(struct Supplier));
            if (new_suppliers == NULL) {
                free(line);
                if (suppliers) free(suppliers);
                close(fd);
                return NULL;
            }
            suppliers = new_suppliers;

            struct Supplier* current = &suppliers[*num_suppliers - 1];
            char* token = strtok(line, "&");
            if (token) current->ID = atoi(token);
            token = strtok(NULL, "&");
            if (token) strncpy(current->supplierName, token, sizeof(current->supplierName) - 1);
            token = strtok(NULL, "&");
            if (token) strncpy(current->email, token, sizeof(current->email) - 1);
            token = strtok(NULL, "&");
            if (token) strncpy(current->city, token, sizeof(current->city) - 1);
            token = strtok(NULL, "&");
            if (token) strncpy(current->street, token, sizeof(current->street) - 1);

            line_pos = 0;
        } else {
            line[line_pos++] = c;
        }
    }

    free(line);
    close(fd);
    return suppliers;
}

struct Product* read_products(const char* filename, int* num_products) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    *num_products = file_size / sizeof(struct Product);

    struct Product* products = malloc(*num_products * sizeof(struct Product));
    if (products == NULL) {
        close(fd);
        return NULL;
    }

    ssize_t bytes_read = read(fd, products, file_size);
    if (bytes_read != file_size) {
        free(products);
        close(fd);
        return NULL;
    }

    close(fd);
    return products;
}

int needs_restock(struct Product* product, int threshold) {
    return product->stock_quantity < threshold;
}

struct Product* create_restock_list(struct Product* products, int num_products,
                                    int threshold, int* num_restock) {
    struct Product* restock_products = malloc(num_products * sizeof(struct Product));
    if (!restock_products) return NULL;

    *num_restock = 0;

    for (int i = 0; i < num_products; i++) {
        if (needs_restock(&products[i], threshold)) {
            restock_products[*num_restock] = products[i];
            (*num_restock)++;
        }
    }

    return restock_products;
}

int write_to_file_and_stdout(int fd, const char* buffer, size_t len) {
    if (write(fd, buffer, len) != (ssize_t)len) return -1;
    if (write(STDOUT_FILENO, buffer, len) != (ssize_t)len) return -1;
    return 0;
}

int write_formatted_to_file_and_stdout(int fd, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (len < 0 || len >= (int)sizeof(buffer)) return -1;
    return write_to_file_and_stdout(fd, buffer, len);
}

int generate_restock_report(struct Product* restock_products, int num_restock,
                            struct Supplier* suppliers, int num_suppliers) {
    int fd = open("demand.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;

    for (int i = 0; i < num_suppliers; i++) {
        int supplier_has_products = 0;

        for (int j = 0; j < num_restock; j++) {
            if (restock_products[j].supplier_id == suppliers[i].ID) {
                supplier_has_products = 1;
                break;
            }
        }

        if (!supplier_has_products) continue;

        if (write_formatted_to_file_and_stdout(fd, "%s (%s):\n", suppliers[i].supplierName, suppliers[i].email) == -1) return -1;

        char current_category[50] = "";
        for (int j = 0; j < num_restock; j++) {
            if (restock_products[j].supplier_id == suppliers[i].ID) {
                if (strcmp(current_category, restock_products[j].category) != 0) {
                    strcpy(current_category, restock_products[j].category);
                    if (write_formatted_to_file_and_stdout(fd, "\t%s:\n", current_category) == -1) return -1;
                }

                int restock_amount = restock_products[j].max_stock - restock_products[j].stock_quantity;
                if (write_formatted_to_file_and_stdout(fd, "\t\t- %s - Stock: %d - Restock: %d\n",
                                    restock_products[j].product_name,
                                    restock_products[j].stock_quantity,
                                    restock_amount) == -1) return -1;
            }
        }

        if (write_to_file_and_stdout(fd, "\n", 1) == -1) return -1;
    }

    close(fd);
    return 0;
}

int generate_current_stock_report(struct Product* products, int num_products) {
    int fd = open("current_stock.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;

    for (int i = 0; i < num_products; i++) {
        if (products[i].stock_quantity >= 10) {
            if (write_formatted_to_file_and_stdout(fd, "%s - Stock: %d - Category: %s\n",
                                products[i].product_name,
                                products[i].stock_quantity,
                                products[i].category) == -1) {
                close(fd);
                return -1;
            }
        }
    }

    close(fd);
    return 0;
}

struct Product* no_restock_needed_products(struct Product* products, int num_products, int threshold, int* num_no_restock) {
    struct Product* no_restock_products = malloc(num_products * sizeof(struct Product));
    if (!no_restock_products) return NULL;

    *num_no_restock = 0;

    for (int i = 0; i < num_products; i++) {
        if (!needs_restock(&products[i], threshold)) {
            no_restock_products[*num_no_restock] = products[i];
            (*num_no_restock)++;
        }
    }

    return no_restock_products;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        const char* error_msg = "Usage: ./S0 <product_file> <supplier_file>\n";
        write(STDERR_FILENO, error_msg, 44);
        return 1;
    }

    int num_products;
    struct Product* products = read_products(argv[1], &num_products);
    if (products == NULL) {
        const char* error_msg = "Error reading products\n";
        write(STDERR_FILENO, error_msg, 23);
        return 1;
    }

    int num_suppliers;
    const size_t max_line_length = 256;
    struct Supplier* suppliers = read_suppliers(argv[2], &num_suppliers, max_line_length);
    if (suppliers == NULL) {
        const char* error_msg = "Error reading suppliers\n";
        write(STDERR_FILENO, error_msg, 24);
        free(products);
        return 1;
    }

    int num_restock;
    int threshold = 10;

    struct Product* restock_products = create_restock_list(products, num_products, threshold, &num_restock);
    if (restock_products == NULL) {
        const char* error_msg = "Error creating restock list\n";
        write(STDERR_FILENO, error_msg, 29);
        free(products);
        free(suppliers);
        return 1;
    }

    if (generate_current_stock_report(products, num_products) == -1) {
        const char* error_msg = "Error generating current stock report\n";
        write(STDERR_FILENO, error_msg, 38);
        free(products);
        free(suppliers);
        free(restock_products);
        return 1;
    }

    write(STDOUT_FILENO, "\n", 1);

    if (generate_restock_report(restock_products, num_restock, suppliers, num_suppliers) == -1) {
        const char* error_msg = "Error generating restock report\n";
        write(STDERR_FILENO, error_msg, 32);
        free(products);
        free(suppliers);
        free(restock_products);
        return 1;
    }

    free(products);
    free(suppliers);
    free(restock_products);

    return 0;
}