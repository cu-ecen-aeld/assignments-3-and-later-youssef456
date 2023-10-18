#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <writestr>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    const char *writestr = argv[2];

    // Open the file for both reading and appending (creates the file if it doesn't exist)
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", file_path);
        return 1;
    }

    // Write the specified string to the file
    if (fprintf(file, "%s\n", writestr) < 0) {
        fprintf(stderr, "Error: Could not write to file %s\n", file_path);
        fclose(file);
        return 1;
    }

    // Close the file
    fclose(file);

    printf("Content written to %s\n", file_path);

    return 0;
}

