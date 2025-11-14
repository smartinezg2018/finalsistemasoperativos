#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "structures.h"

using namespace std;

void InvShiftRows(unsigned char* state) {
    unsigned char tmp[16];
    
    tmp[0] = state[0];
    tmp[1] = state[13];
    tmp[2] = state[10];
    tmp[3] = state[7];
    
    tmp[4] = state[4];
    tmp[5] = state[1];
    tmp[6] = state[14];
    tmp[7] = state[11];
    
    tmp[8] = state[8];
    tmp[9] = state[5];
    tmp[10] = state[2];
    tmp[11] = state[15];
    
    tmp[12] = state[12];
    tmp[13] = state[9];
    tmp[14] = state[6];
    tmp[15] = state[3];
    
    for (int i = 0; i < 16; i++) {
        state[i] = tmp[i];
    }
}

void InvSubBytes(unsigned char* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = inv_s[state[i]];
    }
}

void InvMixColumns(unsigned char* state) {
    unsigned char tmp[16];
    
    tmp[0] = (unsigned char)(mul14[state[0]] ^ mul11[state[1]] ^ mul13[state[2]] ^ mul9[state[3]]);
    tmp[1] = (unsigned char)(mul9[state[0]] ^ mul14[state[1]] ^ mul11[state[2]] ^ mul13[state[3]]);
    tmp[2] = (unsigned char)(mul13[state[0]] ^ mul9[state[1]] ^ mul14[state[2]] ^ mul11[state[3]]);
    tmp[3] = (unsigned char)(mul11[state[0]] ^ mul13[state[1]] ^ mul9[state[2]] ^ mul14[state[3]]);
    
    tmp[4] = (unsigned char)(mul14[state[4]] ^ mul11[state[5]] ^ mul13[state[6]] ^ mul9[state[7]]);
    tmp[5] = (unsigned char)(mul9[state[4]] ^ mul14[state[5]] ^ mul11[state[6]] ^ mul13[state[7]]);
    tmp[6] = (unsigned char)(mul13[state[4]] ^ mul9[state[5]] ^ mul14[state[6]] ^ mul11[state[7]]);
    tmp[7] = (unsigned char)(mul11[state[4]] ^ mul13[state[5]] ^ mul9[state[6]] ^ mul14[state[7]]);
    
    tmp[8] = (unsigned char)(mul14[state[8]] ^ mul11[state[9]] ^ mul13[state[10]] ^ mul9[state[11]]);
    tmp[9] = (unsigned char)(mul9[state[8]] ^ mul14[state[9]] ^ mul11[state[10]] ^ mul13[state[11]]);
    tmp[10] = (unsigned char)(mul13[state[8]] ^ mul9[state[9]] ^ mul14[state[10]] ^ mul11[state[11]]);
    tmp[11] = (unsigned char)(mul11[state[8]] ^ mul13[state[9]] ^ mul9[state[10]] ^ mul14[state[11]]);
    
    tmp[12] = (unsigned char)(mul14[state[12]] ^ mul11[state[13]] ^ mul13[state[14]] ^ mul9[state[15]]);
    tmp[13] = (unsigned char)(mul9[state[12]] ^ mul14[state[13]] ^ mul11[state[14]] ^ mul13[state[15]]);
    tmp[14] = (unsigned char)(mul13[state[12]] ^ mul9[state[13]] ^ mul14[state[14]] ^ mul11[state[15]]);
    tmp[15] = (unsigned char)(mul11[state[12]] ^ mul13[state[13]] ^ mul9[state[14]] ^ mul14[state[15]]);
    
    for (int i = 0; i < 16; i++) {
        state[i] = tmp[i];
    }
}

void AES_decrypt(unsigned char* input, unsigned char* output, unsigned char* roundKeys, int Nr) {
    unsigned char state[16];
    
    for (int i = 0; i < 16; i++) {
        state[i] = input[i];
    }
    
    AddRoundKey(state, roundKeys + (Nr * 16));
    
    for (int round = Nr - 1; round >= 1; round--) {
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(state, roundKeys + (round * 16));
        InvMixColumns(state);
    }
    
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(state, roundKeys);
    
    for (int i = 0; i < 16; i++) {
        output[i] = state[i];
    }
}

int decryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey) {
    // Abrir archivo encriptado
    int fd_in = open(inputFile, O_RDONLY);
    if (fd_in < 0) {
        const char* err = "Error opening encrypted file\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        return 1;
    }

    // Obtener tamaño del archivo
    struct stat st;
    if (fstat(fd_in, &st) < 0) {
        const char* err = "Error getting file size\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        close(fd_in);
        return 1;
    }

    // Verificar tamaño mínimo (16 bytes del header)
    size_t fileSize = st.st_size;
    if (fileSize < 16 || (fileSize - 16) % 16 != 0) {
        const char* err = "Invalid encrypted file size\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        close(fd_in);
        return 1;
    }

    // Leer y desencriptar header
    unsigned char header[16];
    if (read(fd_in, header, 16) != 16) {
        const char* err = "Error reading header\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        close(fd_in);
        return 1;
    }

    unsigned char decryptedHeader[16];
    AES_decrypt(header, decryptedHeader, expandedKey, 10);
    
    // Obtener tamaño original del archivo
    size_t originalSize;
    memcpy(&originalSize, decryptedHeader, sizeof(originalSize));

    // Crear buffers para el contenido
    size_t encryptedSize = fileSize - 16;
    unsigned char* buffer = new unsigned char[encryptedSize];
    unsigned char* decryptedBuffer = new unsigned char[encryptedSize];

    // Leer contenido encriptado
    if (read(fd_in, buffer, encryptedSize) != (ssize_t)encryptedSize) {
        const char* err = "Error reading encrypted content\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        delete[] buffer;
        delete[] decryptedBuffer;
        close(fd_in);
        return 1;
    }
    close(fd_in);

    // Desencriptar por bloques
    for (size_t i = 0; i < encryptedSize; i += 16) {
        AES_decrypt(buffer + i, decryptedBuffer + i, expandedKey, 10);
    }

    // Escribir archivo desencriptado
    int fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        const char* err = "Error creating output file\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        delete[] buffer;
        delete[] decryptedBuffer;
        return 1;
    }

    // Escribir solo el contenido original (sin padding)
    if (write(fd_out, decryptedBuffer, originalSize) < 0) {
        const char* err = "Error writing decrypted content\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        delete[] buffer;
        delete[] decryptedBuffer;
        close(fd_out);
        return 1;
    }

    close(fd_out);
    delete[] buffer;
    delete[] decryptedBuffer;
    return 0;
}