#include <iostream>
#include <cstring>
#include <sstream>
#include <fcntl.h>      // open()
#include <unistd.h>     // read(), write(), close()
#include <sys/stat.h>   // permisos
#include "structures.h"

using namespace std;

/* AES core functions (no se modifican) */
void AddRoundKey(unsigned char * state, unsigned char * roundKey) {
    for (int i = 0; i < 16; i++) {
        state[i] ^= roundKey[i];
    }
}

void SubBytes(unsigned char * state) {
    for (int i = 0; i < 16; i++) {
        state[i] = s[state[i]];
    }
}

void ShiftRows(unsigned char * state) {
    unsigned char tmp[16];
    tmp[0] = state[0];  tmp[1] = state[5];  tmp[2] = state[10]; tmp[3] = state[15];
    tmp[4] = state[4];  tmp[5] = state[9];  tmp[6] = state[14]; tmp[7] = state[3];
    tmp[8] = state[8];  tmp[9] = state[13]; tmp[10] = state[2]; tmp[11] = state[7];
    tmp[12] = state[12]; tmp[13] = state[1]; tmp[14] = state[6]; tmp[15] = state[11];
    for (int i = 0; i < 16; i++) state[i] = tmp[i];
}

void MixColumns(unsigned char * state) {
    unsigned char tmp[16];
    tmp[0] = (unsigned char) mul2[state[0]] ^ mul3[state[1]] ^ state[2] ^ state[3];
    tmp[1] = (unsigned char) state[0] ^ mul2[state[1]] ^ mul3[state[2]] ^ state[3];
    tmp[2] = (unsigned char) state[0] ^ state[1] ^ mul2[state[2]] ^ mul3[state[3]];
    tmp[3] = (unsigned char) mul3[state[0]] ^ state[1] ^ state[2] ^ mul2[state[3]];

    tmp[4] = (unsigned char)mul2[state[4]] ^ mul3[state[5]] ^ state[6] ^ state[7];
    tmp[5] = (unsigned char)state[4] ^ mul2[state[5]] ^ mul3[state[6]] ^ state[7];
    tmp[6] = (unsigned char)state[4] ^ state[5] ^ mul2[state[6]] ^ mul3[state[7]];
    tmp[7] = (unsigned char)mul3[state[4]] ^ state[5] ^ state[6] ^ mul2[state[7]];

    tmp[8] = (unsigned char)mul2[state[8]] ^ mul3[state[9]] ^ state[10] ^ state[11];
    tmp[9] = (unsigned char)state[8] ^ mul2[state[9]] ^ mul3[state[10]] ^ state[11];
    tmp[10] = (unsigned char)state[8] ^ state[9] ^ mul2[state[10]] ^ mul3[state[11]];
    tmp[11] = (unsigned char)mul3[state[8]] ^ state[9] ^ state[10] ^ mul2[state[11]];

    tmp[12] = (unsigned char)mul2[state[12]] ^ mul3[state[13]] ^ state[14] ^ state[15];
    tmp[13] = (unsigned char)state[12] ^ mul2[state[13]] ^ mul3[state[14]] ^ state[15];
    tmp[14] = (unsigned char)state[12] ^ state[13] ^ mul2[state[14]] ^ mul3[state[15]];
    tmp[15] = (unsigned char)mul3[state[12]] ^ state[13] ^ state[14] ^ mul2[state[15]];

    for (int i = 0; i < 16; i++) state[i] = tmp[i];
}

void Round(unsigned char * state, unsigned char * key) {
    SubBytes(state);
    ShiftRows(state);
    MixColumns(state);
    AddRoundKey(state, key);
}

void FinalRound(unsigned char * state, unsigned char * key) {
    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(state, key);
}

void AESEncrypt(unsigned char * message, unsigned char * expandedKey, unsigned char * encryptedMessage) {
    unsigned char state[16];
    for (int i = 0; i < 16; i++) state[i] = message[i];
    int numberOfRounds = 9;

    AddRoundKey(state, expandedKey);

    for (int i = 0; i < numberOfRounds; i++) {
        Round(state, expandedKey + (16 * (i+1)));
    }

    FinalRound(state, expandedKey + 160);

    for (int i = 0; i < 16; i++) encryptedMessage[i] = state[i];
}

/* ==========================
   MAIN usando POSIX I/O
   ========================== */
int main() {
    const char *banner = "=============================\n 128-bit AES Encryption Tool \n=============================\n";
    write(STDOUT_FILENO, banner, strlen(banner));

    char message[1024];
    const char *prompt = "Enter the message to encrypt: ";
    write(STDOUT_FILENO, prompt, strlen(prompt));

    ssize_t bytesRead = read(STDIN_FILENO, message, sizeof(message) - 1);
    if (bytesRead <= 0) {
        const char *err = "Error reading message\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }
    message[bytesRead - 1] = '\0'; // eliminar salto de línea

    int originalLen = strlen((const char *)message);
    int paddedMessageLen = (originalLen % 16 == 0) ? originalLen : ((originalLen / 16) + 1) * 16;

    unsigned char *paddedMessage = new unsigned char[paddedMessageLen]();
    memcpy(paddedMessage, message, originalLen);
    unsigned char *encryptedMessage = new unsigned char[paddedMessageLen];

    // === Leer clave desde archivo "keyfile" usando POSIX ===
    int fd_key = open("keyfile", O_RDONLY);
    if (fd_key < 0) {
        const char *err = "Unable to open keyfile\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }

    char keyBuffer[256];
    ssize_t keyBytes = read(fd_key, keyBuffer, sizeof(keyBuffer) - 1);
    close(fd_key);

    if (keyBytes <= 0) {
        const char *err = "Error reading keyfile\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }
    keyBuffer[keyBytes] = '\0';

    // Parsear clave en formato hex
    istringstream hex_chars_stream(keyBuffer);
    unsigned char key[16];
    unsigned int c;
    int i = 0;
    while (hex_chars_stream >> hex >> c && i < 16) key[i++] = c;

    unsigned char expandedKey[176];
    KeyExpansion(key, expandedKey);

    for (int j = 0; j < paddedMessageLen; j += 16) {
        AESEncrypt(paddedMessage + j, expandedKey, encryptedMessage + j);
    }

    const char *msg = "Encrypted message in hex:\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    // Mostrar salida en hex
    char hexOut[8];
    for (int j = 0; j < paddedMessageLen; j++) {
        int len = snprintf(hexOut, sizeof(hexOut), "%02x ", encryptedMessage[j]);
        write(STDOUT_FILENO, hexOut, len);
    }
    write(STDOUT_FILENO, "\n", 1);

    // === Escribir archivo cifrado "message.aes" con POSIX ===
    int fd_out = open("message.aes", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        const char *err = "Unable to open output file\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }

    write(fd_out, encryptedMessage, paddedMessageLen);
    close(fd_out);

    const char *done = "Wrote encrypted message to file message.aes\n";
    write(STDOUT_FILENO, done, strlen(done));

    delete[] paddedMessage;
    delete[] encryptedMessage;
    return 0;
}
