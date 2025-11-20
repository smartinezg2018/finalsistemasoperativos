#ifndef FILE_ENCRYPTING_H
#define FILE_ENCRYPTING_H

int encryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey);
int decryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey);

#endif // FILE_ENCRYPTING_H

