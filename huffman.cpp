#include "huffman.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <map>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;
using namespace std;

// Comparador para la cola de prioridad
struct CompareNodes {
    bool operator()(HuffmanNode* a, HuffmanNode* b) {
        return a->freq > b->freq;
    }
};

// Función auxiliar para liberar memoria del árbol
void deleteTree(HuffmanNode* root) {
    if (!root) return;
    deleteTree(root->left);
    deleteTree(root->right);
    delete root;
}

// Función auxiliar para generar los códigos recursivamente
void Huffman::generateCodes(HuffmanNode* root, const string& code, map<unsigned char, string>& huffmanCodes) {
    if (!root) return;

    // Caso especial: si solo hay un símbolo en el árbol
    if (!root->left && !root->right) {
        huffmanCodes[root->data] = code.empty() ? "0" : code;
        return;
    }

    generateCodes(root->left, code + "0", huffmanCodes);
    generateCodes(root->right, code + "1", huffmanCodes);
}

// Construir el árbol de Huffman
HuffmanNode* Huffman::buildHuffmanTree(const map<unsigned char, unsigned int>& freqs) {
    priority_queue<HuffmanNode*, vector<HuffmanNode*>, CompareNodes> minHeap;
    for (auto pair : freqs) {
        minHeap.push(new HuffmanNode(pair.first, pair.second));
    }

    if (minHeap.empty()) return nullptr;

    while (minHeap.size() > 1) {
        HuffmanNode* left = minHeap.top(); minHeap.pop();
        HuffmanNode* right = minHeap.top(); minHeap.pop();
        HuffmanNode* parent = new HuffmanNode('$', left->freq + right->freq);
        parent->left = left;
        parent->right = right;
        minHeap.push(parent);
    }

    return minHeap.top();
}

// ------------------------ COMPRESIÓN ------------------------
void Huffman::compress(const std::string& inputFilename) {
    // 1. Verificar archivo de entrada
    std::ifstream inputFile(inputFilename, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de entrada " << inputFilename << std::endl;
        return;
    }

    // Obtener tamaño original
    std::uintmax_t originalSize = 0;
    try {
        originalSize = fs::file_size(inputFilename);
    } catch (...) {
        std::cerr << "Advertencia: No se pudo determinar el tamaño del archivo original." << std::endl;
    }

    // 2. Contar frecuencias
    std::map<unsigned char, unsigned int> freqs;
    char c;
    while (inputFile.get(c)) {
        freqs[(unsigned char)c]++;
    }

    if (freqs.empty()) {
        std::cerr << "Archivo vacío, nada que comprimir." << std::endl;
        inputFile.close();
        return;
    }

    // 3. Construir el árbol
    HuffmanNode* root = buildHuffmanTree(freqs);

    // 4. Generar códigos
    std::map<unsigned char, std::string> huffmanCodes;
    generateCodes(root, "", huffmanCodes);

    // 5. Crear archivo comprimido
    std::string outputFilename = inputFilename + ".huff";
    std::ofstream outputFile(outputFilename, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Error: No se pudo crear el archivo " << outputFilename << std::endl;
        deleteTree(root);
        inputFile.close();
        return;
    }

    // 6. Escribir cabecera (tabla de frecuencias)
    unsigned int tableSize = freqs.size();
    char padding = 0;

    outputFile.write(reinterpret_cast<const char*>(&tableSize), sizeof(tableSize));
    outputFile.write(&padding, sizeof(padding));

    for (auto const& pair : freqs) {
        unsigned char key = pair.first;
        unsigned int val = pair.second;
        outputFile.write(reinterpret_cast<const char*>(&key), sizeof(key));
        outputFile.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }

    // Reiniciar lectura del archivo original
    inputFile.clear();
    inputFile.seekg(0, std::ios::beg);

    // 7. Escribir datos comprimidos
    unsigned char buffer = 0;
    int bitCount = 0;
    long long totalBits = 0;

    while (inputFile.get(c)) {
        std::string code = huffmanCodes[(unsigned char)c];
        for (char bit : code) {
            buffer = (buffer << 1) | (bit - '0');
            bitCount++;
            totalBits++;
            if (bitCount == 8) {
                outputFile.write(reinterpret_cast<const char*>(&buffer), 1);
                buffer = 0;
                bitCount = 0;
            }
        }
    }

    // Bits restantes + padding
    padding = (8 - (totalBits % 8)) % 8;
    if (bitCount > 0) {
        buffer <<= (8 - bitCount);
        outputFile.write(reinterpret_cast<const char*>(&buffer), 1);
    }

    // Reescribir padding en la cabecera
    outputFile.seekp(sizeof(tableSize));
    outputFile.write(&padding, sizeof(padding));

    // 8. Cerrar archivos
    inputFile.close();
    outputFile.close();
    deleteTree(root);

    // Obtener tamaño comprimido
    std::uintmax_t compressedSize = 0;
    try {
        compressedSize = fs::file_size(outputFilename);
    } catch (...) {
        std::cerr << "Advertencia: No se pudo determinar el tamaño del archivo comprimido." << std::endl;
    }

    // 9. Mostrar resultados
    std::cout << "-----------------------------------\n";
    std::cout << "Archivo original:    " << inputFilename << "\n";
    std::cout << "Tamaño original:     " << originalSize << " bytes\n";
    std::cout << "Archivo comprimido:  " << outputFilename << "\n";
    std::cout << "Tamaño comprimido:   " << compressedSize << " bytes\n";
    if (originalSize > 0) {
        double ratio = (1.0 - (double)compressedSize / (double)originalSize) * 100.0;
        std::cout << "Reducción:           " << ratio << "%\n";
    }
    std::cout << "-----------------------------------\n";

    std::cout << "Archivo comprimido exitosamente como " << outputFilename << std::endl;
}

// ------------------------ DESCOMPRESIÓN ------------------------
void Huffman::decompress(const string& inputFilename) {
    ifstream inputFile(inputFilename, ios::binary);
    if (!inputFile.is_open()) {
        cerr << "Error: No se pudo abrir el archivo " << inputFilename << endl;
        return;
    }

    // 1. Leer cabecera
    unsigned int tableSize;
    inputFile.read(reinterpret_cast<char*>(&tableSize), sizeof(tableSize));
    char padding;
    inputFile.read(&padding, sizeof(padding));

    map<unsigned char, unsigned int> freqs;
    long long totalChars = 0;
    for (unsigned int i = 0; i < tableSize; ++i) {
        unsigned char key;
        unsigned int val;
        inputFile.read(reinterpret_cast<char*>(&key), sizeof(key));
        inputFile.read(reinterpret_cast<char*>(&val), sizeof(val));
        freqs[key] = val;
        totalChars += val;
    }

    HuffmanNode* root = buildHuffmanTree(freqs);
    if (!root) {
        cerr << "Archivo vacío o corrupto." << endl;
        inputFile.close();
        return;
    }

    string outputFilename = inputFilename.substr(0, inputFilename.find_last_of("."));
    ofstream outputFile(outputFilename, ios::binary);
    if (!outputFile.is_open()) {
        cerr << "Error al crear archivo de salida." << endl;
        inputFile.close();
        deleteTree(root);
        return;
    }

    HuffmanNode* currentNode = root;
    char byte;
    long long charsWritten = 0;

    // Leer byte por byte y procesar bits
    while (inputFile.get(byte) && charsWritten < totalChars) {
        bool isLastByte = (inputFile.peek() == EOF);
        int bitsToRead = isLastByte ? 8 - padding : 8;

        for (int j = 7; j >= 8 - bitsToRead; --j) {
            int bit = (byte >> j) & 1;
            currentNode = bit ? currentNode->right : currentNode->left;
            if (!currentNode->left && !currentNode->right) {
                outputFile.put(currentNode->data);
                charsWritten++;
                currentNode = root;
                if (charsWritten == totalChars) break;
            }
        }
    }

    inputFile.close();
    outputFile.close();
    deleteTree(root);

    cout << "Archivo descomprimido exitosamente como " << outputFilename << endl;
}
