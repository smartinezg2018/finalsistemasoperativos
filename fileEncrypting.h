#ifndef FILE_ENCRYPTING_H
#define FILE_ENCRYPTING_H

#include <string>

int encryptFile(const char* inputFile, const char* outputFile, const std::string& key);
int decryptFile(const char* inputFile, const char* outputFile, const std::string& key);

#endif // FILE_ENCRYPTING_H
