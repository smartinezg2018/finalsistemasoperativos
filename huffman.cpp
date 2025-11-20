#include "huffman.h"
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#include <io.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#else
#define O_BINARY 0
#endif

using namespace std;

// Magic number para identificar archivos Huffman con formato de chunks
static const unsigned char MAGIC[8] = {'H', 'U', 'F', 'F', 'C', 'H', 'U', 'N'};

// Funciones auxiliares para escribir tipos específicos
static void write_u32(int fd, uint32_t v) {
    write(fd, &v, sizeof(v));
}

static void write_u64(int fd, uint64_t v) {
    write(fd, &v, sizeof(v));
}

static uint32_t read_u32(int fd) {
    uint32_t v;
    if (read(fd, &v, sizeof(v)) != sizeof(v)) {
        return 0;
    }
    return v;
}

static uint64_t read_u64(int fd) {
    uint64_t v;
    if (read(fd, &v, sizeof(v)) != sizeof(v)) {
        return 0;
    }
    return v;
}

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

// Estructura para almacenar datos comprimidos de un chunk
struct CompressedChunk {
    vector<unsigned char> data;
    map<unsigned char, unsigned int> freqs;
    size_t originalSize;
    char padding;
};

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

// Funciones auxiliares estáticas para compresión de chunks
static void generateCodesStatic(HuffmanNode* root, const string& code, map<unsigned char, string>& huffmanCodes) {
    if (!root) return;
    if (!root->left && !root->right) {
        huffmanCodes[root->data] = code.empty() ? "0" : code;
        return;
    }
    generateCodesStatic(root->left, code + "0", huffmanCodes);
    generateCodesStatic(root->right, code + "1", huffmanCodes);
}

static HuffmanNode* buildHuffmanTreeStatic(const map<unsigned char, unsigned int>& freqs) {
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

// Función auxiliar para comprimir un chunk de datos
static CompressedChunk compressChunk(const unsigned char* data, size_t size) {
    CompressedChunk chunk;
    chunk.originalSize = size;
    
    if (size == 0) return chunk;
    
    // Contar frecuencias del chunk
    map<unsigned char, unsigned int> freqs;
    for (size_t i = 0; i < size; i++) {
        freqs[data[i]]++;
    }
    chunk.freqs = freqs;
    
    if (freqs.empty()) return chunk;
    
    // Construir árbol y generar códigos
    HuffmanNode* root = buildHuffmanTreeStatic(freqs);
    map<unsigned char, string> huffmanCodes;
    generateCodesStatic(root, "", huffmanCodes);
    
    // Comprimir datos
    unsigned char buffer = 0;
    int bitCount = 0;
    long long totalBits = 0;
    vector<unsigned char> compressed;
    
    for (size_t i = 0; i < size; i++) {
        string code = huffmanCodes[data[i]];
        for (char bit : code) {
            buffer = (buffer << 1) | (bit - '0');
            bitCount++;
            totalBits++;
            if (bitCount == 8) {
                compressed.push_back(buffer);
                buffer = 0;
                bitCount = 0;
            }
        }
    }
    
    // Bits restantes + padding
    chunk.padding = (8 - (totalBits % 8)) % 8;
    if (bitCount > 0) {
        buffer <<= (8 - bitCount);
        compressed.push_back(buffer);
    }
    
    chunk.data = compressed;
    deleteTree(root);
    return chunk;
}

// ------------------------ COMPRESIÓN ------------------------
void Huffman::compress(const std::string& inputFilename) {
    const size_t chunkSize = 1 << 20; // 1MB por defecto
    // 1. Abrir archivo de entrada
    int fd_in = open(inputFilename.c_str(), O_RDONLY | O_BINARY);
    if (fd_in < 0) {
        std::cerr << "Error: No se pudo abrir el archivo de entrada " << inputFilename << std::endl;
        return;
    }

    // Obtener tamaño original
    struct stat st;
    size_t originalSize = 0;
    if (fstat(fd_in, &st) == 0) {
        originalSize = st.st_size;
    }

    if (originalSize == 0) {
        std::cerr << "Archivo vacío, nada que comprimir." << std::endl;
        close(fd_in);
        return;
    }

    // 2. Leer todo el archivo en memoria
    unsigned char* fileBuffer = new unsigned char[originalSize];
    if (!read_all(fd_in, fileBuffer, originalSize)) {
        std::cerr << "Error al leer el archivo." << std::endl;
        delete[] fileBuffer;
        close(fd_in);
        return;
    }
    close(fd_in);

    // 3. Dividir en chunks
    size_t chunkCount = (originalSize + chunkSize - 1) / chunkSize;
    vector<CompressedChunk> compressedChunks(chunkCount);
    vector<size_t> compSizes(chunkCount);
    vector<size_t> uncompSizes(chunkCount);

    // 4. Procesar chunks en paralelo
    vector<thread> threads;
    size_t numThreads = thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    numThreads = min(numThreads, chunkCount);

    size_t chunksPerThread = (chunkCount + numThreads - 1) / numThreads;

    for (size_t t = 0; t < numThreads; t++) {
        threads.emplace_back([&, t]() {
            size_t start = t * chunksPerThread;
            size_t end = min(start + chunksPerThread, chunkCount);
            
            for (size_t i = start; i < end; i++) {
                size_t off = i * chunkSize;
                size_t len = min(chunkSize, originalSize - off);
                uncompSizes[i] = len;
                compressedChunks[i] = compressChunk(fileBuffer + off, len);
                compSizes[i] = compressedChunks[i].data.size();
            }
        });
    }

    // Esperar a que todos los threads terminen
    for (auto& th : threads) {
        th.join();
    }

    // 5. Crear archivo comprimido
    std::string outputFilename = inputFilename + ".huff";
    int fd_out = open(outputFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd_out < 0) {
        std::cerr << "Error: No se pudo crear el archivo " << outputFilename << std::endl;
        delete[] fileBuffer;
        return;
    }

    // 6. Escribir header
    write(fd_out, MAGIC, 8);
    write_u32(fd_out, (uint32_t)chunkCount);

    // 7. Escribir metadatos de cada chunk
    for (size_t i = 0; i < chunkCount; i++) {
        write_u64(fd_out, compSizes[i]);
        write_u64(fd_out, uncompSizes[i]);
        
        // Escribir tabla de frecuencias del chunk
        unsigned int tableSize = compressedChunks[i].freqs.size();
        write(fd_out, &tableSize, sizeof(tableSize));
        write(fd_out, &compressedChunks[i].padding, sizeof(compressedChunks[i].padding));
        
        for (auto const& pair : compressedChunks[i].freqs) {
            unsigned char key = pair.first;
            unsigned int val = pair.second;
            write(fd_out, &key, sizeof(key));
            write(fd_out, &val, sizeof(val));
        }
    }

    // 8. Escribir datos comprimidos de cada chunk
    for (size_t i = 0; i < chunkCount; i++) {
        write(fd_out, compressedChunks[i].data.data(), compSizes[i]);
    }

    close(fd_out);
    delete[] fileBuffer;

    // 9. Obtener tamaño comprimido y mostrar resultados
    size_t compressedSize = 0;
    if (stat(outputFilename.c_str(), &st) == 0) {
        compressedSize = st.st_size;
    }

    std::cout << "-----------------------------------\n";
    std::cout << "Archivo original:    " << inputFilename << "\n";
    std::cout << "Tamaño original:     " << originalSize << " bytes\n";
    std::cout << "Archivo comprimido:  " << outputFilename << "\n";
    std::cout << "Tamaño comprimido:   " << compressedSize << " bytes\n";
    if (originalSize > 0) {
        double ratio = (1.0 - (double)compressedSize / (double)originalSize) * 100.0;
        std::cout << "Reducción:           " << ratio << "%\n";
    }
    std::cout << "Chunks procesados:   " << chunkCount << "\n";
    std::cout << "-----------------------------------\n";

    std::cout << "Archivo comprimido exitosamente como " << outputFilename << std::endl;
}

// Función auxiliar para descomprimir un chunk
static vector<unsigned char> decompressChunk(const unsigned char* compressedData, size_t compSize,
                                            const map<unsigned char, unsigned int>& freqs,
                                            char padding, size_t expectedSize) {
    vector<unsigned char> output;
    if (compSize == 0 || expectedSize == 0) return output;
    
    HuffmanNode* root = buildHuffmanTreeStatic(freqs);
    if (!root) return output;
    
    HuffmanNode* currentNode = root;
    long long charsWritten = 0;
    
    for (size_t i = 0; i < compSize && charsWritten < (long long)expectedSize; i++) {
        unsigned char byte = compressedData[i];
        bool isLastByte = (i == compSize - 1);
        int bitsToRead = isLastByte ? 8 - padding : 8;
        
        for (int j = 7; j >= 8 - bitsToRead; --j) {
            int bit = (byte >> j) & 1;
            currentNode = bit ? currentNode->right : currentNode->left;
            if (!currentNode->left && !currentNode->right) {
                output.push_back(currentNode->data);
                charsWritten++;
                currentNode = root;
                if (charsWritten >= (long long)expectedSize) break;
            }
        }
    }
    
    deleteTree(root);
    return output;
}

// ------------------------ DESCOMPRESIÓN ------------------------
void Huffman::decompress(const string& inputFilename) {
    // 1. Abrir archivo de entrada
    int fd_in = open(inputFilename.c_str(), O_RDONLY | O_BINARY);
    if (fd_in < 0) {
        cerr << "Error: No se pudo abrir el archivo " << inputFilename << endl;
        return;
    }

    // 2. Leer magic number para determinar formato
    unsigned char magic[8];
    if (read(fd_in, magic, 8) != 8) {
        cerr << "Error al leer archivo." << endl;
        close(fd_in);
        return;
    }

    // Verificar si es formato de chunks
    if (memcmp(magic, MAGIC, 8) == 0) {
        // Formato de chunks
        uint32_t chunkCount = read_u32(fd_in);
        if (chunkCount == 0) {
            cerr << "Error: Número de chunks inválido." << endl;
            close(fd_in);
            return;
        }

        // Leer metadatos de chunks
        vector<uint64_t> compSizes(chunkCount), uncompSizes(chunkCount);
        vector<map<unsigned char, unsigned int>> freqsList(chunkCount);
        vector<char> paddings(chunkCount);
        
        for (size_t i = 0; i < chunkCount; i++) {
            compSizes[i] = read_u64(fd_in);
            uncompSizes[i] = read_u64(fd_in);
            
            unsigned int tableSize;
            if (read(fd_in, &tableSize, sizeof(tableSize)) != sizeof(tableSize)) {
                cerr << "Error al leer tabla de frecuencias del chunk " << i << endl;
                close(fd_in);
                return;
            }
            
            if (read(fd_in, &paddings[i], sizeof(paddings[i])) != sizeof(paddings[i])) {
                cerr << "Error al leer padding del chunk " << i << endl;
                close(fd_in);
                return;
            }
            
            for (unsigned int j = 0; j < tableSize; j++) {
                unsigned char key;
                unsigned int val;
                if (read(fd_in, &key, sizeof(key)) != sizeof(key) ||
                    read(fd_in, &val, sizeof(val)) != sizeof(val)) {
                    cerr << "Error al leer frecuencia del chunk " << i << endl;
                    close(fd_in);
                    return;
                }
                freqsList[i][key] = val;
            }
        }

        // Leer todos los datos comprimidos
        struct stat st;
        fstat(fd_in, &st);
        size_t totalCompSize = 0;
        for (size_t i = 0; i < chunkCount; i++) {
            totalCompSize += compSizes[i];
        }
        
        unsigned char* allCompressed = new unsigned char[totalCompSize];
        if (!read_all(fd_in, allCompressed, totalCompSize)) {
            cerr << "Error al leer datos comprimidos." << endl;
            delete[] allCompressed;
            close(fd_in);
            return;
        }
        close(fd_in);

        // Descomprimir chunks en paralelo
        size_t chunkCountSize = (size_t)chunkCount;
        vector<vector<unsigned char>> decompressedChunks(chunkCountSize);
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, chunkCountSize);
        
        size_t chunksPerThread = (chunkCountSize + numThreads - 1) / numThreads;
        vector<thread> threads;
        size_t offset = 0;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t, offset]() mutable {
                size_t start = t * chunksPerThread;
                size_t end = min(start + chunksPerThread, chunkCountSize);
                size_t localOffset = offset;
                
                for (size_t i = start; i < end; i++) {
                    decompressedChunks[i] = decompressChunk(
                        allCompressed + localOffset, (size_t)compSizes[i],
                        freqsList[i], paddings[i], (size_t)uncompSizes[i]
                    );
                    localOffset += (size_t)compSizes[i];
                }
            });
            
            for (size_t i = t * chunksPerThread; i < min((t + 1) * chunksPerThread, chunkCountSize); i++) {
                offset += (size_t)compSizes[i];
            }
        }
        
        for (auto& th : threads) {
            th.join();
        }
        delete[] allCompressed;

        // Crear archivo de salida
        string outputFilename = inputFilename;
        if (outputFilename.size() > 5 && outputFilename.substr(outputFilename.size() - 5) == ".huff") {
            outputFilename = outputFilename.substr(0, outputFilename.size() - 5);
        } else {
            outputFilename += ".out";
        }
        
        int fd_out = open(outputFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
        if (fd_out < 0) {
            cerr << "Error al crear archivo de salida." << endl;
            return;
        }

        // Escribir chunks descomprimidos
        for (size_t i = 0; i < chunkCountSize; i++) {
            write(fd_out, decompressedChunks[i].data(), decompressedChunks[i].size());
        }
        
        close(fd_out);
        cout << "Archivo descomprimido exitosamente como " << outputFilename << endl;
        return;
    }

    // Formato antiguo (sin chunks) - retrocompatibilidad
    lseek(fd_in, 0, SEEK_SET);
    unsigned int tableSize;
    if (read(fd_in, &tableSize, sizeof(tableSize)) != sizeof(tableSize)) {
        cerr << "Error al leer cabecera." << endl;
        close(fd_in);
        return;
    }

    char padding;
    if (read(fd_in, &padding, sizeof(padding)) != sizeof(padding)) {
        cerr << "Error al leer padding." << endl;
        close(fd_in);
        return;
    }

    map<unsigned char, unsigned int> freqs;
    long long totalChars = 0;
    for (unsigned int i = 0; i < tableSize; ++i) {
        unsigned char key;
        unsigned int val;
        if (read(fd_in, &key, sizeof(key)) != sizeof(key) ||
            read(fd_in, &val, sizeof(val)) != sizeof(val)) {
            cerr << "Error al leer tabla de frecuencias." << endl;
            close(fd_in);
            return;
        }
        freqs[key] = val;
        totalChars += val;
    }

    HuffmanNode* root = buildHuffmanTree(freqs);
    if (!root) {
        cerr << "Archivo vacío o corrupto." << endl;
        close(fd_in);
        return;
    }

    string outputFilename = inputFilename.substr(0, inputFilename.find_last_of("."));
    int fd_out = open(outputFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd_out < 0) {
        cerr << "Error al crear archivo de salida." << endl;
        close(fd_in);
        deleteTree(root);
        return;
    }

    HuffmanNode* currentNode = root;
    unsigned char byte;
    long long charsWritten = 0;
    ssize_t bytesRead;

    while ((bytesRead = read(fd_in, &byte, 1)) > 0 && charsWritten < totalChars) {
        off_t currentPos = lseek(fd_in, 0, SEEK_CUR);
        struct stat st;
        fstat(fd_in, &st);
        bool isLastByte = (currentPos >= (off_t)st.st_size);
        int bitsToRead = isLastByte ? 8 - padding : 8;

        for (int j = 7; j >= 8 - bitsToRead; --j) {
            int bit = (byte >> j) & 1;
            currentNode = bit ? currentNode->right : currentNode->left;
            if (!currentNode->left && !currentNode->right) {
                write(fd_out, &currentNode->data, 1);
                charsWritten++;
                currentNode = root;
                if (charsWritten == totalChars) break;
            }
        }
        
        if (charsWritten == totalChars) break;
    }

    close(fd_in);
    close(fd_out);
    deleteTree(root);

    cout << "Archivo descomprimido exitosamente como " << outputFilename << endl;
}
