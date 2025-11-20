#include "vigenere.h"
#include <string>
#include <algorithm>

// Función para expandir la clave a la longitud necesaria
void expandKey(const std::string& key, std::string& expandedKey, size_t length) {
    if (key.empty()) {
        expandedKey = std::string(length, 'A'); // Clave por defecto si está vacía
        return;
    }
    
    expandedKey.clear();
    expandedKey.reserve(length);
    
    for (size_t i = 0; i < length; i++) {
        expandedKey += key[i % key.length()];
    }
}

// Función para encriptar un bloque de datos con Vigenère
void vigenereEncrypt(const unsigned char* input, unsigned char* output, 
                     const std::string& key, size_t length) {
    std::string expandedKey;
    expandKey(key, expandedKey, length);
    
    for (size_t i = 0; i < length; i++) {
        unsigned char inputByte = input[i];
        unsigned char keyByte = expandedKey[i];
        
        // Vigenère: (input + key) mod 256
        output[i] = (inputByte + keyByte) % 256;
    }
}

// Función para desencriptar un bloque de datos con Vigenère
void vigenereDecrypt(const unsigned char* input, unsigned char* output, 
                     const std::string& key, size_t length) {
    std::string expandedKey;
    expandKey(key, expandedKey, length);
    
    for (size_t i = 0; i < length; i++) {
        unsigned char inputByte = input[i];
        unsigned char keyByte = expandedKey[i];
        
        // Vigenère inverso: (input - key + 256) mod 256
        output[i] = (inputByte - keyByte + 256) % 256;
    }
}

