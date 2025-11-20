#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include "aes_encrypt.h"
#include "aes_decrypt.h"

int encryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey) {
    int fd_in = open(inputFile, O_RDONLY);
    if (fd_in < 0) {
        write(STDERR_FILENO, "Error opening input file\n", 25);
        return 1;
    }

    struct stat st;
    if (fstat(fd_in, &st) < 0) {
        write(STDERR_FILENO, "Error getting file size\n", 24);
        close(fd_in);
        return 1;
    }

    size_t fileSize = st.st_size;
    size_t paddedSize = (fileSize % 16 == 0) ? fileSize : ((fileSize / 16) + 1) * 16;

    // HEADER: 16 bytes, primeros 8 = tamaño original
    unsigned char header[16] = {0};
    memcpy(header, &fileSize, sizeof(fileSize));

    unsigned char* buffer = new unsigned char[paddedSize]();
    unsigned char* encryptedBuffer = new unsigned char[16 + paddedSize];

    // Encriptar header en los primeros 16 bytes
    AESEncrypt(header, expandedKey, encryptedBuffer, 10);

    // Leer archivo
    ssize_t bytesRead = read(fd_in, buffer, fileSize);
    if (bytesRead < 0) {
        write(STDERR_FILENO, "Error reading file\n", 19);
        delete[] buffer;
        delete[] encryptedBuffer;
        close(fd_in);
        return 1;
    }
    close(fd_in);

    // Encriptar contenido en bloques
    for (size_t i = 0; i < paddedSize; i += 16) {
        AESEncrypt(buffer + i, expandedKey, encryptedBuffer + 16 + i, 10);
    }

    // Escribir salida: header + contenido
    int fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        write(STDERR_FILENO, "Error creating output file\n", 27);
        delete[] buffer;
        delete[] encryptedBuffer;
        return 1;
    }

    if (write(fd_out, encryptedBuffer, paddedSize + 16) < 0) {
        write(STDERR_FILENO, "Error writing to output file\n", 29);
        delete[] buffer;
        delete[] encryptedBuffer;
        close(fd_out);
        return 1;
    }

    close(fd_out);
    delete[] buffer;
    delete[] encryptedBuffer;
    return 0;
}


int decryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey) {
    int fd_in = open(inputFile, O_RDONLY);
    if (fd_in < 0) {
        write(STDERR_FILENO, "Error opening encrypted file\n", 29);
        return 1;
    }

    struct stat st;
    if (fstat(fd_in, &st) < 0) {
        write(STDERR_FILENO, "Error getting file size\n", 24);
        close(fd_in);
        return 1;
    }

    size_t fileSize = st.st_size;

    if (fileSize < 16 || (fileSize - 16) % 16 != 0) {
        write(STDERR_FILENO, "Invalid encrypted file size\n", 29);
        close(fd_in);
        return 1;
    }

    unsigned char header[16];
    if (read(fd_in, header, 16) != 16) {
        write(STDERR_FILENO, "Error reading header\n", 21);
        close(fd_in);
        return 1;
    }

    unsigned char decryptedHeader[16];
    AES_decrypt(header, decryptedHeader, expandedKey, 10);

    size_t originalSize;
    memcpy(&originalSize, decryptedHeader, sizeof(originalSize));

    size_t encryptedSize = fileSize - 16;

    unsigned char* buffer = new unsigned char[encryptedSize];
    unsigned char* decryptedBuffer = new unsigned char[encryptedSize];

    if (read(fd_in, buffer, encryptedSize) != (ssize_t)encryptedSize) {
        write(STDERR_FILENO, "Error reading encrypted content\n", 32);
        delete[] buffer;
        delete[] decryptedBuffer;
        close(fd_in);
        return 1;
    }
    close(fd_in);

    for (size_t i = 0; i < encryptedSize; i += 16) {
        AES_decrypt(buffer + i, decryptedBuffer + i, expandedKey, 10);
    }

    int fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        write(STDERR_FILENO, "Error creating output file\n", 27);
        delete[] buffer;
        delete[] decryptedBuffer;
        return 1;
    }

    if (write(fd_out, decryptedBuffer, originalSize) < 0) {
        write(STDERR_FILENO, "Error writing decrypted content\n", 32);
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
