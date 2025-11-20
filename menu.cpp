#include "menu.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "filePartitioner.h"
#include "huffman.h"
#include "AES/structures.h"
#include "fileEncrypting.h"

using namespace std;

// Función auxiliar para leer la clave AES
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

// Función para obtener y expandir la clave AES
bool getAESKey(unsigned char* expandedKey) {
    string keyInput;
    cout << "\nIngrese la ruta al archivo de clave (AES/keyfile.txt) o una clave hexadecimal de 32 caracteres: ";
    cin.ignore();
    getline(cin, keyInput);
    
    unsigned char key[16];
    
    // Intentar leer como archivo primero
    if (read_key_from_file(keyInput.c_str(), key) == 0) {
        KeyExpansion(key, expandedKey);
        return true;
    }
    // Intentar como hex string
    else if (keyInput.length() == 32 && parse_hex_key(keyInput.c_str(), key) == 0) {
        KeyExpansion(key, expandedKey);
        return true;
    }
    else {
        cout << "Error: Formato de clave inválido. Debe ser un archivo con 16 bytes o 32 caracteres hexadecimales." << endl;
        return false;
    }
}

// Función para encriptar un archivo
void encryptFileMenu() {
    string inputFile, outputFile;
    cout << "\n=== ENCRIPTAR ARCHIVO ===" << endl;
    cout << "Ingrese el archivo de entrada: ";
    cin >> inputFile;
    cout << "Ingrese el archivo de salida: ";
    cin >> outputFile;
    
    unsigned char expandedKey[176];
    if (!getAESKey(expandedKey)) {
        return;
    }
    
    if (encryptFile(inputFile.c_str(), outputFile.c_str(), expandedKey) == 0) {
        cout << "\n✓ Archivo encriptado exitosamente: " << outputFile << endl;
    } else {
        cout << "\n✗ Error al encriptar el archivo." << endl;
    }
}

// Función para desencriptar un archivo
void decryptFileMenu() {
    string inputFile, outputFile;
    cout << "\n=== DESENCRIPTAR ARCHIVO ===" << endl;
    cout << "Ingrese el archivo encriptado: ";
    cin >> inputFile;
    cout << "Ingrese el archivo de salida: ";
    cin >> outputFile;
    
    unsigned char expandedKey[176];
    if (!getAESKey(expandedKey)) {
        return;
    }
    
    if (decryptFile(inputFile.c_str(), outputFile.c_str(), expandedKey) == 0) {
        cout << "\n✓ Archivo desencriptado exitosamente: " << outputFile << endl;
    } else {
        cout << "\n✗ Error al desencriptar el archivo." << endl;
    }
}

// Función para comprimir un archivo
void compressFile() {
    string inputFile;
    int algorithm;
    
    cout << "\n=== COMPRIMIR ARCHIVO ===" << endl;
    cout << "Seleccione el algoritmo de compresión:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opción: ";
    cin >> algorithm;
    
    cout << "Ingrese el archivo a comprimir: ";
    cin >> inputFile;
    
    if (algorithm == 1) {
        // LZW
        filePartitioner partitioner;
        partitioner.compressLZW(inputFile);
        cout << "\n✓ Archivo comprimido con LZW: " << inputFile << ".lzw" << endl;
    } else if (algorithm == 2) {
        // Huffman
        Huffman huffman;
        huffman.compress(inputFile);
        cout << "\n✓ Archivo comprimido con Huffman: " << inputFile << ".huff" << endl;
    } else {
        cout << "\n✗ Opción inválida." << endl;
    }
}

// Función para descomprimir un archivo
void decompressFile() {
    string inputFile;
    int algorithm;
    
    cout << "\n=== DESCOMPRIMIR ARCHIVO ===" << endl;
    cout << "Seleccione el algoritmo de compresión:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opción: ";
    cin >> algorithm;
    
    cout << "Ingrese el archivo comprimido: ";
    cin >> inputFile;
    
    if (algorithm == 1) {
        // LZW
        filePartitioner partitioner;
        partitioner.decompressLZW(inputFile);
        cout << "\n✓ Archivo descomprimido con LZW." << endl;
    } else if (algorithm == 2) {
        // Huffman
        Huffman huffman;
        huffman.decompress(inputFile);
        cout << "\n✓ Archivo descomprimido con Huffman." << endl;
    } else {
        cout << "\n✗ Opción inválida." << endl;
    }
}

// Función para encriptar y comprimir
void encryptAndCompress() {
    string inputFile, encryptedFile, compressedFile;
    int algorithm;
    
    cout << "\n=== ENCRIPTAR Y COMPRIMIR ARCHIVO ===" << endl;
    cout << "Ingrese el archivo de entrada: ";
    cin >> inputFile;
    
    cout << "Seleccione el algoritmo de compresión:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opción: ";
    cin >> algorithm;
    
    // Primero encriptar
    encryptedFile = inputFile + ".enc";
    unsigned char expandedKey[176];
    if (!getAESKey(expandedKey)) {
        return;
    }
    
    cout << "\nEncriptando archivo..." << endl;
    if (::encryptFile(inputFile.c_str(), encryptedFile.c_str(), expandedKey) != 0) {
        cout << "\n✗ Error al encriptar el archivo." << endl;
        return;
    }
    cout << "✓ Archivo encriptado: " << encryptedFile << endl;
    
    // Luego comprimir
    cout << "\nComprimiendo archivo encriptado..." << endl;
    if (algorithm == 1) {
        filePartitioner partitioner;
        partitioner.compressLZW(encryptedFile);
        compressedFile = encryptedFile + ".lzw";
        cout << "✓ Archivo comprimido con LZW: " << compressedFile << endl;
    } else if (algorithm == 2) {
        Huffman huffman;
        huffman.compress(encryptedFile);
        compressedFile = encryptedFile + ".huff";
        cout << "✓ Archivo comprimido con Huffman: " << compressedFile << endl;
    } else {
        cout << "\n✗ Opción de compresión inválida." << endl;
        return;
    }
    
    cout << "\n✓ Proceso completado: " << compressedFile << endl;
}

// Función para desencriptar y descomprimir
void decryptAndDecompress() {
    string inputFile, decompressedFile, decryptedFile;
    int algorithm;
    
    cout << "\n=== DESENCRIPTAR Y DESCOMPRIMIR ARCHIVO ===" << endl;
    cout << "Ingrese el archivo comprimido y encriptado: ";
    cin >> inputFile;
    
    cout << "Seleccione el algoritmo de compresión:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opción: ";
    cin >> algorithm;
    
    // Primero descomprimir
    cout << "\nDescomprimiendo archivo..." << endl;
    if (algorithm == 1) {
        filePartitioner partitioner;
        partitioner.decompressLZW(inputFile);
        // El archivo descomprimido será inputFile sin .lzw
        if (inputFile.size() > 4 && inputFile.substr(inputFile.size() - 4) == ".lzw") {
            decompressedFile = inputFile.substr(0, inputFile.size() - 4);
        } else {
            decompressedFile = inputFile + ".out";
        }
    } else if (algorithm == 2) {
        Huffman huffman;
        huffman.decompress(inputFile);
        // El archivo descomprimido será inputFile sin .huff
        if (inputFile.size() > 5 && inputFile.substr(inputFile.size() - 5) == ".huff") {
            decompressedFile = inputFile.substr(0, inputFile.size() - 5);
        } else {
            decompressedFile = inputFile + ".out";
        }
    } else {
        cout << "\n✗ Opción de compresión inválida." << endl;
        return;
    }
    
    cout << "✓ Archivo descomprimido: " << decompressedFile << endl;
    
    // Luego desencriptar
    decryptedFile = decompressedFile + ".dec";
    unsigned char expandedKey[176];
    if (!getAESKey(expandedKey)) {
        return;
    }
    
    cout << "\nDesencriptando archivo..." << endl;
    if (::decryptFile(decompressedFile.c_str(), decryptedFile.c_str(), expandedKey) == 0) {
        cout << "✓ Archivo desencriptado: " << decryptedFile << endl;
        cout << "\n✓ Proceso completado: " << decryptedFile << endl;
    } else {
        cout << "\n✗ Error al desencriptar el archivo." << endl;
    }
}

// Función principal del menú
void showMenu() {
    int option;
    
    while (true) {
        cout << "\n";
        cout << "╔════════════════════════════════════════╗" << endl;
        cout << "║     MENÚ PRINCIPAL - SISTEMA DE       ║" << endl;
        cout << "║   ENCRIPTACIÓN Y COMPRESIÓN            ║" << endl;
        cout << "╠════════════════════════════════════════╣" << endl;
        cout << "║  1. Encriptar                          ║" << endl;
        cout << "║  2. Desencriptar                       ║" << endl;
        cout << "║  3. Comprimir                          ║" << endl;
        cout << "║  4. Descomprimir                       ║" << endl;
        cout << "║  5. Encriptar y comprimir              ║" << endl;
        cout << "║  6. Desencriptar y descomprimir        ║" << endl;
        cout << "║  0. Salir                              ║" << endl;
        cout << "╚════════════════════════════════════════╝" << endl;
        cout << "\nSeleccione una opción: ";
        
        cin >> option;
        
        switch (option) {
            case 1:
                encryptFileMenu();
                break;
            case 2:
                decryptFileMenu();
                break;
            case 3:
                compressFile();
                break;
            case 4:
                decompressFile();
                break;
            case 5:
                encryptAndCompress();
                break;
            case 6:
                decryptAndDecompress();
                break;
            case 0:
                cout << "\n¡Hasta luego!" << endl;
                return;
            default:
                cout << "\n✗ Opción inválida. Por favor, seleccione una opción del 0 al 6." << endl;
                break;
        }
        
        cout << "\nPresione Enter para continuar...";
        cin.ignore();
        cin.get();
    }
}

