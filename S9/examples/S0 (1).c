/*
 * Solution S0 Operating Systems - File Descriptors
 * Curs 2024-25
 *
 * @author: Ferran Castañé
 *
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_NUM_ARGS "ERROR: Incorrect number of arguments\nPlease provide the input and output file names ./S0.exe products.bin suppliers.txt\n"
#define ERROR_MSG_FILE_NOT_FOUND "ERROR: File not found\n"

typedef struct{
    char productName[100];
    char category[50];
    int maxStock;
    int stockQuantity;
    float unitPrice;
    int supplierID;
} Product;

typedef struct{
    int supplierID;
    char* supplierName;
    char* email;
    char* city;
    char* street;
} Supplier;



/**
 * Reads product data from a binary file.
 *
 * @param file name of the binary file to read
 * @param numProducts pointer to an integer to store the number of products read
 * @return a dynamically allocated array of Product structures
 */
Product *readBinaryFile(char *file, int *numProducts){
    Product *products = NULL;

    int fd = open(file, O_RDONLY);
    if (fd < 0){
        printF(ERROR_MSG_FILE_NOT_FOUND);
    }else{
        products = (Product *)malloc(sizeof(Product));

        *numProducts = 0;

        while(read(fd, &(products[(*numProducts)]), sizeof(Product) ) != 0){
            (*numProducts)++;
            products = (Product *)realloc(products, ((*numProducts)+1) * sizeof(Product));
        }


        close(fd);

    }

    return products;
}

char *readUntil(int fd, char cEnd) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = NULL;

    while (1) {
        chars_read = read(fd, &c, sizeof(char));
        if (chars_read == 0) {
            if (i == 0) {
                return NULL;
            }
            break;
        } else if (chars_read < 0) {
            free(buffer);
            return NULL;
        }

        if (c == cEnd) {
            break;
        }
        buffer = (char *)realloc(buffer, i + 2);
        buffer[i++] = c;
    }

    buffer[i] = '\0';  // Null-terminate the string
    return buffer;
}

/**
 * Reads supplier data from a text file.
 *
 * @param file name of the text file to read
 * @param numSuppliers pointer to an integer to store the number of suppliers read
 * @return a dynamically allocated array of Supplier structures
 */
Supplier *readSupplierFile(char *file, int *numSuppliers){
    char* buffer;
    Supplier *suppliers;

    int fd = open(file, O_RDONLY);
    if (fd < 0){
        printF(ERROR_MSG_FILE_NOT_FOUND);
    }else{

        suppliers = (Supplier *)malloc(sizeof(Supplier));
        *numSuppliers = 0;

        buffer = readUntil(fd, '&');
        while(buffer != NULL){
            suppliers[(*numSuppliers)].supplierID = atoi(buffer);
            free(buffer);

            suppliers[(*numSuppliers)].supplierName = readUntil(fd, '&');
            suppliers[(*numSuppliers)].email = readUntil(fd, '&');
            suppliers[(*numSuppliers)].city = readUntil(fd, '&');
            suppliers[(*numSuppliers)].street = readUntil(fd, '\n');

            (*numSuppliers)++;


            buffer = readUntil(fd, '&');
            if(buffer!= NULL){
                suppliers = (Supplier *)realloc(suppliers, ((*numSuppliers)+1) * sizeof(Supplier));
            }
        }

        close(fd);
    }

    return suppliers;
}

/**
 * Finds a supplier by ID.
 *
 * @param suppliers array of Supplier structures
 * @param numSuppliers number of suppliers in the array
 * @param supplierID the ID of the supplier to find
 * @return a pointer to the found Supplier, or NULL if not found
 */
Supplier *findSupplier(Supplier *suppliers, int numSuppliers, int supplierID){
    for (int i = 0; i < numSuppliers; i++)
    {
        if (suppliers[i].supplierID == supplierID)
        {
            return &suppliers[i];
        }
    }
    return NULL;
}

/**
 * Generates the current stock report.
 *
 * @param filename the name of the output file
 * @param products array of Product structures
 * @param numProducts number of products in the array
 */
void generateCurrentStockReport(char *filename, Product *products, int numProducts){
    char* buffer;
    int len;
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0){
        printF("Error opening current stock file");
    }else{
        for (int i = 0; i < numProducts; i++){
            if (products[i].stockQuantity >= 10){
                len = asprintf(&buffer, "%s – Stock: %d – Category: %s\n", products[i].productName, products[i].stockQuantity, products[i].category);
                write(fd, buffer, len);
                write(1, buffer, len);
                free(buffer);
            }
        }

        close(fd);
    }


}

int hasCategory(char** categories, int doneCategories, char* category){
    for (int i = 0; i < doneCategories; i++){
        if (strcmp(categories[i], category) == 0){
            return 1;
        }
    }
    return 0;
}

/**
 * Generates the demand report.
 *
 * @param filename the name of the output file
 * @param products array of Product structures
 * @param numProducts number of products in the array
 * @param suppliers array of Supplier structures
 * @param numSuppliers number of suppliers in the array
 */
void generateDemandReport(char *filename, Product *products, int numProducts, Supplier *suppliers, int numSuppliers) {
    char *buffer;
    int len;
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        printF("Error opening demand report file");
    } else {
        for (int i = 0; i < numSuppliers; i++) {
            int hasProducts = 0;
            char **categories = NULL;  // Dynamic array to store printed categories
            int doneCategories = 0;

            len = asprintf(&buffer, "%s (%s):\n", suppliers[i].supplierName, suppliers[i].email);
            write(fd, buffer, len);
            write(1, buffer, len);
            free(buffer);

            for (int j = 0; j < numProducts; j++) {
                if (products[j].supplierID == suppliers[i].supplierID && products[j].stockQuantity < 10) {

                    // Check if the category has already been printed
                    if (!hasCategory(categories, doneCategories, products[j].category)) {
                        // Add the new category to the list
                        categories = realloc(categories, (doneCategories + 1) * sizeof(char *));
                        categories[doneCategories] = strdup(products[j].category);
                        doneCategories++;

                        len = asprintf(&buffer, "\t%s:\n", products[j].category);
                        write(fd, buffer, len);
                        write(1, buffer, len);
                        free(buffer);
                    }

                    len = asprintf(&buffer, "\t\t- %s – Stock: %d – Restock: %d\n", products[j].productName, products[j].stockQuantity, products[j].maxStock - products[j].stockQuantity);
                    write(fd, buffer, len);
                    write(1, buffer, len);
                    free(buffer);

                    hasProducts = 1;
                }
            }

            if (hasProducts) {
                write(fd, "\n", 1);
                write(1, "\n", 1);
            }

            for (int k = 0; k < doneCategories; k++) {
                free(categories[k]);
            }
            free(categories);
        }

        close(fd);
    }
}

/**
 * Frees the dynamically allocated memory for the products and suppliers.
 *
 * @param products array of Product structures
 * @param suppliers array of Supplier structures
 */
void freeMemory(Product *products, Supplier *suppliers, int totalSuppliers){
    free(products);
    for (int i = 0; i < totalSuppliers; i++){
        free(suppliers[i].supplierName);
        free(suppliers[i].email);
        free(suppliers[i].city);
        free(suppliers[i].street);
    }
    free(suppliers);
}

/**
 * Main function that executes the program.
 *
 * @param argc number of command-line arguments
 * @param argv array of command-line arguments
 * @return exit status
 */
int main(int argc, char *argv[]){
    Product *products = NULL;
    Supplier *suppliers = NULL;
    int numProducts = 0, numSuppliers = 0;

    if (argc != 3)
    {
        printF(ERROR_MSG_NUM_ARGS);
        return -1;
    }

    products = readBinaryFile(argv[1], &numProducts);
    if (products == NULL)
    {
        return -2;
    }

    suppliers = readSupplierFile(argv[2], &numSuppliers);
    if (suppliers == NULL)
    {
        free(products);
        return -3;
    }

    // Generate the reports
    generateCurrentStockReport("current_stock.txt", products, numProducts);
    generateDemandReport("demand.txt", products, numProducts, suppliers, numSuppliers);

    // Free the dynamically allocated memory
    freeMemory(products, suppliers, numSuppliers);

    return 0;
}
