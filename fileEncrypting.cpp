#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <errno.h>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define usleep(x) Sleep((x)/1000)
#ifndef O_BINARY
#define O_BINARY 0
#endif
#else
#include <unistd.h>
#define O_BINARY 0
#endif
#include "vigenere.h"

// Función auxiliar para leer completamente un buffer
static bool read_all(int fd, unsigned char* buf, size_t to_read) {
    size_t off = 0;
    while (off < to_read) {
        ssize_t r = read(fd, buf + off, to_read - off);
        if (r <= 0) return false;
        off += r;
    }
    return true;
}

// Función auxiliar para escribir completamente un buffer
static bool write_all(int fd, const unsigned char* buf, size_t to_write, size_t* total_written) {
    size_t off = 0;
    while (off < to_write) {
        ssize_t w = write(fd, buf + off, to_write - off);
        if (w <= 0) return false;
        off += w;
    }
    if (total_written) *total_written = off;
    return true;
}

int encryptFile(const char* inputFile, const char* outputFile, const std::string& key) {
    int fd_in = open(inputFile, O_RDONLY | O_BINARY);
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
    if (fileSize == 0) {
        write(STDERR_FILENO, "Error: File is empty\n", 21);
        close(fd_in);
        return 1;
    }

    // HEADER: 8 bytes para el tamaño original
    unsigned char header[8] = {0};
    memcpy(header, &fileSize, sizeof(fileSize));

    // Leer todo el archivo
    unsigned char* buffer = new unsigned char[fileSize];
    if (!read_all(fd_in, buffer, fileSize)) {
        char errMsg[200];
        int errLen = snprintf(errMsg, sizeof(errMsg), 
            "Error reading file: %s (size: %zu bytes)\n", inputFile, fileSize);
        write(STDERR_FILENO, errMsg, errLen);
        delete[] buffer;
        close(fd_in);
        return 1;
    }
    close(fd_in);

    // Encriptar header y contenido con Vigenère
    unsigned char encryptedHeader[8];
    unsigned char* encryptedBuffer = new unsigned char[fileSize];
    
    vigenereEncrypt(header, encryptedHeader, key, 8);
    vigenereEncrypt(buffer, encryptedBuffer, key, fileSize);

    // IMPORTANTE: Eliminar archivo destino PRIMERO para evitar problemas en Windows
    for (int i = 0; i < 10; i++) {
        if (unlink(outputFile) == 0 || errno == ENOENT) {
            break; // Archivo eliminado o no existe
        }
        #ifdef _WIN32
        Sleep(50); // 50ms delay en Windows
        #endif
    }
    
    // Crear archivo temporal con nombre único para evitar conflictos en Windows
    char tempFile[512];
    static int counter = 0;
    counter++;
    snprintf(tempFile, sizeof(tempFile), "%s.tmp.%d.%d", outputFile, (int)getpid(), counter);
    
    // Eliminar temporal si existe
    unlink(tempFile);
    
    // Crear archivo nuevo con O_EXCL para asegurar que no existe
    int fd_out = open(tempFile, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0644);
    if (fd_out < 0 && errno == EEXIST) {
        unlink(tempFile);
        fd_out = open(tempFile, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0644);
    }
    if (fd_out < 0) {
        write(STDERR_FILENO, "Error creating output file\n", 27);
        delete[] buffer;
        delete[] encryptedBuffer;
        return 1;
    }

    size_t totalToWrite = 8 + fileSize;
    
    // Escribir header encriptado
    size_t header_written = 0;
    if (!write_all(fd_out, encryptedHeader, 8, &header_written)) {
        write(STDERR_FILENO, "Error writing header\n", 22);
        delete[] buffer;
        delete[] encryptedBuffer;
        close(fd_out);
        unlink(tempFile);
        return 1;
    }
    
    // Escribir contenido encriptado
    size_t content_written = 0;
    if (!write_all(fd_out, encryptedBuffer, fileSize, &content_written)) {
        write(STDERR_FILENO, "Error writing encrypted content\n", 33);
        delete[] buffer;
        delete[] encryptedBuffer;
        close(fd_out);
        unlink(tempFile);
        return 1;
    }
    
    close(fd_out);
    
    // Verificar que escribimos exactamente lo que queríamos
    if (header_written != 8 || content_written != fileSize) {
        char errMsg[200];
        int errLen = snprintf(errMsg, sizeof(errMsg), 
            "ERROR: Write mismatch - header: %zu/8, content: %zu/%zu\n", 
            header_written, content_written, fileSize);
        write(STDERR_FILENO, errMsg, errLen);
        unlink(tempFile);
        delete[] buffer;
        delete[] encryptedBuffer;
        return 1;
    }
    
    // En Windows, rename() puede no reemplazar archivos existentes
    // Eliminar archivo destino si existe (múltiples intentos)
    for (int i = 0; i < 5; i++) {
        if (unlink(outputFile) == 0 || errno == ENOENT) {
            break;
        }
        #ifdef _WIN32
        usleep(10000); // 10ms
        #endif
    }
    
    // Intentar renombrar primero (más eficiente)
    if (rename(tempFile, outputFile) == 0) {
        // Rename exitoso
    } else {
        // Si rename falla (común en Windows), copiar el archivo
        int fd_src = open(tempFile, O_RDONLY);
        if (fd_src < 0) {
            char errMsg[200];
            int errLen = snprintf(errMsg, sizeof(errMsg), 
                "ERROR: Failed to open temporary file for copy\n");
            write(STDERR_FILENO, errMsg, errLen);
            unlink(tempFile);
            delete[] buffer;
            delete[] encryptedBuffer;
            return 1;
        }
        
        int fd_dst = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
        if (fd_dst < 0) {
            char errMsg[200];
            int errLen = snprintf(errMsg, sizeof(errMsg), 
                "ERROR: Failed to create output file for copy\n");
            write(STDERR_FILENO, errMsg, errLen);
            close(fd_src);
            unlink(tempFile);
            delete[] buffer;
            delete[] encryptedBuffer;
            return 1;
        }
        
        // Copiar contenido
        unsigned char copyBuf[65536];
        size_t remaining = totalToWrite;
        while (remaining > 0) {
            size_t toRead = (remaining > sizeof(copyBuf)) ? sizeof(copyBuf) : remaining;
            ssize_t r = read(fd_src, copyBuf, toRead);
            if (r <= 0) break;
            if (!write_all(fd_dst, copyBuf, r, NULL)) {
                close(fd_src);
                close(fd_dst);
                unlink(outputFile);
                unlink(tempFile);
                delete[] buffer;
                delete[] encryptedBuffer;
                return 1;
            }
            remaining -= r;
        }
        
        close(fd_src);
        close(fd_dst);
        unlink(tempFile);
    }
    
    delete[] buffer;
    delete[] encryptedBuffer;
    return 0;
}

int decryptFile(const char* inputFile, const char* outputFile, const std::string& key) {
    int fd_in = open(inputFile, O_RDONLY | O_BINARY);
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

    if (fileSize < 8) {
        char errMsg[200];
        int errLen = snprintf(errMsg, sizeof(errMsg), 
            "Invalid encrypted file size: %zu bytes (must be >= 8)\n", fileSize);
        write(STDERR_FILENO, errMsg, errLen);
        close(fd_in);
        return 1;
    }

    // Leer header encriptado (8 bytes)
    unsigned char encryptedHeader[8];
    if (read(fd_in, encryptedHeader, 8) != 8) {
        write(STDERR_FILENO, "Error reading header\n", 21);
        close(fd_in);
        return 1;
    }

    // Desencriptar header
    unsigned char header[8];
    vigenereDecrypt(encryptedHeader, header, key, 8);

    // Extraer tamaño original
    size_t originalSize;
    memcpy(&originalSize, header, sizeof(originalSize));

    // Verificar que el tamaño tiene sentido
    if (originalSize > fileSize - 8 || originalSize == 0) {
        char errMsg[200];
        int errLen = snprintf(errMsg, sizeof(errMsg), 
            "Invalid original file size in header: %zu bytes\n", originalSize);
        write(STDERR_FILENO, errMsg, errLen);
        close(fd_in);
        return 1;
    }

    size_t encryptedSize = fileSize - 8;

    // Leer contenido encriptado
    unsigned char* buffer = new unsigned char[encryptedSize];
    if (!read_all(fd_in, buffer, encryptedSize)) {
        write(STDERR_FILENO, "Error reading encrypted content\n", 32);
        delete[] buffer;
        close(fd_in);
        return 1;
    }
    close(fd_in);

    // Desencriptar contenido
    unsigned char* decryptedBuffer = new unsigned char[encryptedSize];
    vigenereDecrypt(buffer, decryptedBuffer, key, encryptedSize);

    // Escribir archivo desencriptado
    int fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd_out < 0) {
        write(STDERR_FILENO, "Error creating output file\n", 27);
        delete[] buffer;
        delete[] decryptedBuffer;
        return 1;
    }

    if (!write_all(fd_out, decryptedBuffer, originalSize, NULL)) {
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
