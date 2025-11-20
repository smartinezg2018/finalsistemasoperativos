#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "AES/structures.h"

// Forward declarations de las funciones de encriptación/desencriptación
extern int encryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey);
extern int decryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey);

static void print_usage(const char *p) {
    fprintf(stderr,
        "Usage: %s [-c|-d|-e|-u combined] [--comp-alg name] [--enc-alg name] -i input -o output [-k key]\n"
        "Operations:\n"
        "  -e  encrypt\n"
        "  -u  decrypt\n"
        "  -i  input file/directory\n"
        "  -o  output file/directory\n"
        "  -k  key file or 32 hex characters\n", p);
}

static int parse_hex_key(const char *hex, unsigned char *out16) {
    size_t len = strlen(hex);
    if (len != 32) return -1;
    
    for (int i = 0; i < 16; ++i) {
        unsigned int v;
        if (sscanf(hex + 2*i, "%2x", &v) != 1) return -1;
        out16[i] = (unsigned char)v;
    }
    return 0;
}

static int read_key_from_file(const char *path, unsigned char *out16) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    ssize_t r = read(fd, out16, 16);
    close(fd);
    
    if (r != 16) return -1;
    return 0;
}

int main(int argc, char **argv) {
    int opt;
    char *input = NULL;
    char *output = NULL;
    char *keyopt = NULL;
    bool encrypt = false;
    bool decrypt = false;

    while ((opt = getopt(argc, argv, "eui:o:k:")) != -1) {
        switch (opt) {
            case 'e': encrypt = true; break;
            case 'u': decrypt = true; break;
            case 'i': input = optarg; break;
            case 'o': output = optarg; break;
            case 'k': keyopt = optarg; break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (!input || !output || !keyopt) {
        print_usage(argv[0]);
        return 1;
    }

    unsigned char key[16];
    unsigned char expandedKey[176];

    // Interpretar la clave
    if (strlen(keyopt) == 32 && parse_hex_key(keyopt, key) == 0) {
        // La clave se proporcionó como hex string
    } else if (read_key_from_file(keyopt, key) == 0) {
        // La clave se leyó del archivo
    } else {
        fprintf(stderr, "Invalid key format\n");
        return 1;
    }

    KeyExpansion(key, expandedKey);

    if (encrypt) {
        return encryptFile(input, output, expandedKey);
    } else if (decrypt) {
        return decryptFile(input, output, expandedKey);
    }

    print_usage(argv[0]);
    return 1;
}