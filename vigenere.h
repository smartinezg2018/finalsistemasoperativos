#ifndef VIGENERE_H
#define VIGENERE_H

#include <string>

// Función para expandir la clave a la longitud necesaria
void expandKey(const std::string& key, std::string& expandedKey, size_t length);

// Función para encriptar un bloque de datos con Vigenère
void vigenereEncrypt(const unsigned char* input, unsigned char* output, 
                     const std::string& key, size_t length);

// Función para desencriptar un bloque de datos con Vigenère
void vigenereDecrypt(const unsigned char* input, unsigned char* output, 
                     const std::string& key, size_t length);

#endif // VIGENERE_H

