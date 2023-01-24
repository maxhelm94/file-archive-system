////////////////////////////////////////////////////////////////////////
// COMP1521 21T3 --- Assignment 2: `chicken', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/21T3/assignments/ass2/index.html>
//
// Written by Maximilian Bernhard Helm (z5314713) on 15.-22.11.2021
//
// 2021-11-08   v1.1    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "chicken.h"

#define MAX_PATHNAME 1024
#define MAX_CONTENTS 4194304 //8388608
// ADD ANY extra #defines HERE

// struct file node saves all details about one file
struct file_node {
    uint8_t egglet_format;
    uint8_t permissions[11];
    uint16_t pathname_length;
    char pathname[MAX_PATHNAME];
    uint64_t content_length;
    uint8_t contents[MAX_CONTENTS];
    uint8_t hash;
    uint8_t calculated_hash;
};

// ADD YOUR FUNCTION PROTOTYPES HERE
struct file_node get_file(int c, FILE *f);
uint16_t convert_permission(uint8_t* permissions);


// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)

void list_egg(char *egg_pathname, int long_listing) {

    // open the egg file; if that is not possible, raise an error
    FILE *f = fopen(egg_pathname, "rb");
    if (f == NULL) {
        perror(egg_pathname);
        return;
    }

    int c;
    // loop through all the files of the egg
    while ((c = fgetc(f)) != EOF) {
        // make sure the magic number is 'c'
        if (c != 'c') {
            exit(1);
        }

        // get all the details of one file of the egg
        struct file_node new_file = get_file(c, f);

        // print the details of the file
        if (long_listing) {
            printf("%s  %d  %5lu  %s\n", new_file.permissions, new_file.egglet_format,
                                         new_file.content_length, new_file.pathname);
        } else {
            printf("%s\n", new_file.pathname);
        }
    }

    // close the egg file
    fclose(f);
    return;
}


// check the files & directories stored in egg_pathname (subset 1)
//
// prints the files & directories stored in egg_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_egg(char *egg_pathname) {

    // open the egg file; if that is not possible, raise an error
    FILE *f = fopen(egg_pathname, "rb");
    if (f == NULL) {
        perror(egg_pathname);
        return;
    }

    int c;
    // loop through all the files of the egg
    while ((c = fgetc(f)) != EOF) {
        // make sure the magic number is 'c'
        if (c != 'c') {
            // if the first char is not 'c', print the char to stderr 
            char error_message[57];
            snprintf(error_message, sizeof(error_message), "error: incorrect first egglet byte: 0x%x should be 0x63\n", c);
            fprintf(stderr, "%s", error_message);
            return;
        }

        // get all the details of one file of the egg
        struct file_node new_file = get_file(c, f);

        // check whether the calculated hash is equal to the actual hash;
        // if not, name the hash that was calculated
        if (new_file.calculated_hash == new_file.hash) {
            printf("%s - correct hash\n", new_file.pathname);
        } else {
            printf("%s - incorrect hash 0x%x should be 0x%x\n", 
                   new_file.pathname, new_file.calculated_hash, new_file.hash);
        }
    }

    // close the egg file
    fclose(f);
    return;
}


// extract the files/directories stored in egg_pathname (subset 2 & 3)

void extract_egg(char *egg_pathname) {

    // open the egg file; if that is not possible, raise an error
    FILE *f = fopen(egg_pathname, "rb");
    if (f == NULL) {
        perror(egg_pathname);
        return;
    }

    int c;
    // loop through all the files of the egg
    while ((c = fgetc(f)) != EOF) {
        // make sure the magic number is 'c'
        if (c != 'c') {
            exit(1);
        }

        // get all the details of one file of the egg
        struct file_node new_file = get_file(c, f);

        // convert the permissions displayed as string "-rwxrwxrwx" to
        // a number 777
        mode_t mode = convert_permission(new_file.permissions);
        
        // create a new file with the details of that file in the egg
        FILE *g = fopen(new_file.pathname, "w");

        // copy all the content of the file from the egg into the newly
        // created file
        fwrite(new_file.contents, 1, new_file.content_length, g);
        
        // close that new file
        fclose(g);
        
        // change the permissions of the new file to the ones it had
        // inside the egg
        if (chmod(new_file.pathname, mode) != 0) {
            perror(new_file.pathname); // prints why the chmod failed
            return;
        }

        // print the extraction of the file
        printf("Extracting: %s\n", new_file.pathname);
    }

    // close the egg
    fclose(f);
    return;
}


// create egg_pathname containing the files or directories specified in pathnames (subset 3)
//
// if append is zero egg_pathname should be over-written if it exists
// if append is non-zero egglets should be instead appended to egg_pathname if it exists
//
// format specifies the egglet format to use, it must be one EGGLET_FMT_6,EGGLET_FMT_7 or EGGLET_FMT_8

void create_egg(char *egg_pathname, int append, int format,
                int n_pathnames, char *pathnames[n_pathnames]) {

    // create a new egg file or append to one, depending on the
    // "append" variable
    FILE *f = NULL;
    if (append) {
        f = fopen(egg_pathname, "a");
    } else {
        f = fopen(egg_pathname, "wb");
    }

    // loop through all the files that shall be included in the egg
    for (int i = 0; i < n_pathnames; ++i) {
        
        // declare a variable that progressively calculates the hash value
        // of the file
        int hash;
        
        // add magic number
        fputc(0x63, f);
        hash = egglet_hash(0x00, 0x63);
        
        // add egglet format
        fputc(format, f);
        hash = egglet_hash(hash, format);
        
        // add first byte of the permissions; will by '-', as directories
        // are not implemented
        fputc('-', f);
        hash = egglet_hash(hash, '-');

        // save the details of the file in the stat struct s
        struct stat s;
        stat(pathnames[i], &s);

        // copy the permission of st_mode
        uint16_t permission = s.st_mode;
        uint16_t divisor = 256;
        
        // loop 9 times; for each permission bit once
        for (int j = 0; j < 9; ++j) {
            // the divisor is 0b1,0000,0000 and therefore the & operator checks
            // whether the 9th bit of permission is one; by dividing the divisor
            // by 2 every loop, we check every bit consecutively
            if ((permission & divisor) != 0) {
                if (j == 0 || j == 3 || j == 6) {
                    fputc('r', f);
                    hash = egglet_hash(hash, 'r');
                } else if (j == 1 || j == 4 || j == 7) {
                    fputc('w', f);
                    hash = egglet_hash(hash, 'w');
                } else if (j == 2 || j == 5 || j == 8) {
                    fputc('x', f);
                    hash = egglet_hash(hash, 'x');
                }
            } else {
                fputc('-', f);
                hash = egglet_hash(hash, '-');
            }
            divisor /= 2;
        }

        // get the pathname_length by using strlen
        uint16_t pathname_length = strlen(pathnames[i]);
        
        // as the pathname_length is saved in little-endian, we first save
        // the value of the latter 8 bits in x
        uint8_t x = pathname_length & 127u;
        
        // then we shift the pathname_length by 8 bits to the right to save
        // the value of the left 8 bits in y
        uint8_t y = pathname_length >> 8u;
        
        // save these two digits, indicating the length of the pathname
        fputc(x, f);
        hash = egglet_hash(hash, x);
        fputc(y, f);
        hash = egglet_hash(hash, y);

        // save all bytes of the pathename in the egg
        for (int j = 0; j < pathname_length; ++j) {
            fputc(pathnames[i][j], f);
            hash = egglet_hash(hash, pathnames[i][j]);
        }

        // get the size of the file by using the st_size variable
        uint64_t content_length = s.st_size;

        // consecutively save 8 bits as byte in the egg, starting
        // form the most right ones as we save the information in
        // little endian
        for (int j = 0; j < 48; j += 8) {
            uint8_t byte = ((content_length >> j) & 255u);
            fputc(byte, f);
            hash = egglet_hash(hash, byte);
        }

        // open the file and read all bytes consecutively
        FILE *g = fopen(pathnames[i], "rb");
        int c;
        for (int j = 0; j < content_length; j++) {
            // copy all bytes into the egglet
            c = fgetc(g);
            hash = egglet_hash(hash, c);
            fputc(c, f);
        }

        // close the file
        fclose(g);

        // print the add message to stdout
        printf("Adding: %s\n", pathnames[i]);

        // add the calculated hash to the egglet
        fputc(hash, f);


    }

    // close the egg
    fclose(f);
    
}


// ADD YOUR EXTRA FUNCTIONS HERE


// function that saves all the details of a file in a struct that 
// can then be accessed in the function that called it 
struct file_node get_file(int c, FILE *f) {
    
    struct file_node n_f;
    
    // add the foramt to the file struct
    n_f.calculated_hash = egglet_hash(0x00, c);
    n_f.egglet_format = fgetc(f);
    
    // evaluate whether the egglet format is 0x36, 0x37 or 0x38
    // if (n_f.egglet_format != 0x36 && n_f.egglet_format != 0x37 &&
    //     n_f.egglet_format != 0x38) {
    //     exit(1);
    // }

    // add the egg format to the hash calculation
    n_f.calculated_hash = egglet_hash(n_f.calculated_hash, n_f.egglet_format);
    
    n_f.egglet_format = n_f.egglet_format - '0';

    // evaluate the permissions of the file
    for (int i = 0; i < 10; ++i) {
        n_f.permissions[i] = fgetc(f);
        n_f.calculated_hash = egglet_hash(n_f.calculated_hash, n_f.permissions[i]);
    }
    n_f.permissions[10] = '\0';

    // get the length of the file 
    c = fgetc(f);
    int d = fgetc(f);
    n_f.pathname_length = d * 256 + c;
    n_f.calculated_hash = egglet_hash(n_f.calculated_hash, c);
    n_f.calculated_hash = egglet_hash(n_f.calculated_hash, d); 

    // get the pathname of the file
    for (int i = 0; i < n_f.pathname_length; ++i) {
        n_f.pathname[i] = fgetc(f);
        n_f.calculated_hash = egglet_hash(n_f.calculated_hash, n_f.pathname[i]);
    } 
    n_f.pathname[n_f.pathname_length] = '\0';

    // determine the content's length 
    n_f.content_length = 0;
    for (int i = 0; i < 6; ++i) {
        c = fgetc(f);
        n_f.calculated_hash = egglet_hash(n_f.calculated_hash, c);
        int shift = 8 * i;
        uint64_t tmp = c;
        tmp = tmp << shift;
        n_f.content_length |= tmp;
    }
    
    // get the files content
    for (int i = 0; i < n_f.content_length; ++i) {
        n_f.contents[i] = fgetc(f);
        n_f.calculated_hash = egglet_hash(n_f.calculated_hash, n_f.contents[i]);
    }
    n_f.contents[n_f.content_length] = '\0';
    
    // get the file's hash
    n_f.hash = fgetc(f);
    
    return n_f;
}


// function that converts the permission passed in as string
// "-rwxrwxrwx" to a three digit octal number and returns it
uint16_t convert_permission(uint8_t* permissions) {
    uint16_t result = 0;
    for (int i = 0; i < 9; i += 3) {
        for (int j = 0; j < 3; ++j) {
            if (permissions[9 - (i + j)] != '-') {
                uint16_t tmp = 1 << (i + j);
                result |= tmp;
            } 
        }
    }
    return result;
}