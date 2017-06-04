/* keygen.c
 * Author: Jason Goldfine-Middleton
 * Course: CS 344
 */

#include <stdio.h>
#include <stdlib.h>

#define SP_KEY 26

int main(int argc, char *argv[])
{
    FILE *urandom;
    unsigned int seed;
    size_t keylen, r_ret_val;
    char *key;
    char ch;
    int i = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s keylength\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ch = argv[1][i];
    while (ch) {
        if (ch < '0' || ch > '9') {
            fprintf(stderr, "Usage: first parameter must be a non-negative integer\n");
            exit(EXIT_FAILURE);
        }
        ++i;
        ch = argv[1][i];
    }

    keylen = atoi(argv[1]);

    /* citation: http://stackoverflow.com/a/4930888 */
    urandom = fopen("/dev/urandom", "r");
    
    if (urandom == NULL) {
        fprintf(stderr, "Unable to open /dev/urandom for read\n");
        exit(EXIT_FAILURE);
    }

    /* citation: http://stackoverflow.com/a/4930888 */
    r_ret_val = fread(&seed, sizeof(seed), 1, urandom);
    if (r_ret_val < 1) {
        fprintf(stderr, "Unable to get a seed from /dev/urandom\n");
        fclose(urandom);
        exit(EXIT_FAILURE);
    }

    fclose(urandom);
    srand(seed);
    
    key = malloc((keylen + 1) * sizeof(char));
    if (key == NULL) {
        fprintf(stderr, "Unable to allocate heap memory\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i != keylen; ++i) {
        char rval = (char) (rand() % 27);
        key[i] = (rval == SP_KEY) ? ' ' : rval + 'A';
    }
    key[keylen] = '\0';

    puts(key);
    free(key);

    return EXIT_SUCCESS;
}
