#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "AES/aes_encrypt.h"
#include "AES/aes_decrypt.h"

using namespace std;

void EncryptBuffer(const unsigned char *input,
                   unsigned char *output,
                   size_t size,
                   unsigned char *expandedKey)
{
    for (size_t i = 0; i < size; i += 16)
    {
        AESEncrypt((unsigned char *)(input + i),
                   expandedKey,
                   output + i);
    }
}

void DecryptBuffer(const unsigned char *input,
                   unsigned char *output,
                   size_t size,
                   unsigned char *expandedKey)
{
    for (size_t i = 0; i < size; i += 16)
    {
        AES_decrypt((unsigned char *)(input + i),
                    output + i,
                    expandedKey,
                    10);
    }
}