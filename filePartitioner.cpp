#include "lzw.h"
#include<iostream>
#include<map>
#include<vector>
#include<map>
#include<fcntl.h>
#include<unistd.h>
#include<cstring>
#include<sys/stat.h>
#include<algorithm>
#include<string>
#include "filePartitioner.h"
#include<unordered_map>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>
#ifdef _WIN32
#include <io.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#else
#define O_BINARY 0
#endif

using namespace std;

// Magic number para identificar archivos LZW con formato de chunks
static const unsigned char MAGIC[8] = {'L', 'Z', 'W', 'C', 'H', 'U', 'N', 'K'};

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

// Función para leer completamente un buffer
static bool read_all(int fd, unsigned char* buf, size_t to_read) {
    size_t off = 0;
    while (off < to_read) {
        ssize_t r = read(fd, buf + off, to_read - off);
        if (r <= 0) return false;
        off += r;
    }
    return true;
}

int bufferSize = (1 << 20);

void filePartitioner::compressLZW(const string& filename, size_t chunkSize) {
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        perror("stat");
        return;
    }
    size_t fileSize = st.st_size;
    if (fileSize == 0) return;

    int fd = open(filename.c_str(), O_RDONLY | O_BINARY);
    if (fd < 0) {
        perror("open");
        return;
    }

    unsigned char* fileBuf = new unsigned char[fileSize];
    if (!read_all(fd, fileBuf, fileSize)) {
        perror("read");
        close(fd);
        delete[] fileBuf;
        return;
    }
    close(fd);

    size_t chunkCount = (fileSize + chunkSize - 1) / chunkSize;

    vector<vector<unsigned char>> compressedChunks(chunkCount);
    vector<size_t> compSizes(chunkCount);
    vector<size_t> uncompSizes(chunkCount);

    lzw compressor;

    // Procesamiento paralelo de chunks
    vector<thread> threads;
    size_t numThreads = thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // Fallback si no se puede detectar
    numThreads = min(numThreads, chunkCount);

    size_t chunksPerThread = (chunkCount + numThreads - 1) / numThreads;

    for (size_t t = 0; t < numThreads; t++) {
        threads.emplace_back([&, t]() {
            size_t start = t * chunksPerThread;
            size_t end = min(start + chunksPerThread, chunkCount);
            
            for (size_t i = start; i < end; i++) {
                size_t off = i * chunkSize;
                size_t len = min(chunkSize, fileSize - off);
                uncompSizes[i] = len;
                compressedChunks[i] = compressor.compress(fileBuf + off, len);
                compSizes[i] = compressedChunks[i].size();
            }
        });
    }

    // Esperar a que todos los threads terminen
    for (auto& th : threads) {
        th.join();
    }

    string outname = filename + ".lzw";
    int outfd = open(outname.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (outfd < 0) {
        perror("open out");
        delete[] fileBuf;
        return;
    }

    // Escribir header
    write(outfd, MAGIC, 8);
    write_u32(outfd, (uint32_t)chunkCount);

    // Escribir metadatos de cada chunk
    for (size_t i = 0; i < chunkCount; i++) {
        write_u64(outfd, compSizes[i]);
        write_u64(outfd, uncompSizes[i]);
    }

    // Escribir datos comprimidos de cada chunk
    for (size_t i = 0; i < chunkCount; i++) {
        write(outfd, compressedChunks[i].data(), compSizes[i]);
    }

    close(outfd);
    delete[] fileBuf;
}


void filePartitioner::decompressLZW(const string& filename) {
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        perror("stat");
        return;
    }
    size_t fileSize = st.st_size;

    int fd = open(filename.c_str(), O_RDONLY | O_BINARY);
    if (fd < 0) {
        perror("open");
        return;
    }

    // Leer todo el archivo de una vez (más eficiente)
    unsigned char* fileBuf = new unsigned char[fileSize];
    if (!read_all(fd, fileBuf, fileSize)) {
        perror("read");
        close(fd);
        delete[] fileBuf;
        return;
    }
    close(fd);

    // Verificar magic number usando memcmp (más eficiente)
    if (memcmp(fileBuf, MAGIC, 8) != 0) {
        // Formato antiguo: descomprimir todo el archivo directamente
        lzw lzw_obj;
        vector<unsigned char> decompressedData = lzw_obj.decompress(fileBuf, fileSize);

        string outname = filename;
        if (outname.size() > 4 && outname.substr(outname.size() - 4) == ".lzw") {
            outname = outname.substr(0, outname.size() - 4);
        } else {
            outname += ".out";
        }

        int outfd = open(outname.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
        if (outfd < 0) {
            perror("open out");
            delete[] fileBuf;
            return;
        }
        
        write(outfd, decompressedData.data(), decompressedData.size());
        close(outfd);
        delete[] fileBuf;
        return;
    }

    // Formato nuevo con chunks - parsear usando memcpy (más eficiente)
    size_t pos = 8;
    uint32_t chunkCount_u32;
    memcpy(&chunkCount_u32, fileBuf + pos, sizeof(chunkCount_u32));
    pos += sizeof(chunkCount_u32);
    size_t chunkCount = (size_t)chunkCount_u32;

    if (chunkCount == 0) {
        cerr << "Error: Invalid chunk count" << endl;
        delete[] fileBuf;
        return;
    }

    // Leer metadatos de chunks usando memcpy
    vector<uint64_t> compSizes(chunkCount);
    vector<uint64_t> uncompSizes(chunkCount);
    for (size_t i = 0; i < chunkCount; i++) {
        memcpy(&compSizes[i], fileBuf + pos, sizeof(uint64_t));
        pos += sizeof(uint64_t);
        memcpy(&uncompSizes[i], fileBuf + pos, sizeof(uint64_t));
        pos += sizeof(uint64_t);
    }

    // Crear punteros a los chunks (evita cálculos de offset repetidos)
    vector<const unsigned char*> chunkPtrs(chunkCount);
    for (size_t i = 0; i < chunkCount; i++) {
        chunkPtrs[i] = fileBuf + pos;
        pos += compSizes[i];
    }

    // Descomprimir chunks en paralelo
    vector<vector<unsigned char>> decompressedChunks(chunkCount);
    vector<thread> threads;
    size_t numThreads = thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    numThreads = min(numThreads, chunkCount);

    size_t chunksPerThread = (chunkCount + numThreads - 1) / numThreads;

    for (size_t t = 0; t < numThreads; t++) {
        threads.emplace_back([&, t]() {
            size_t start = t * chunksPerThread;
            size_t end = min(start + chunksPerThread, chunkCount);
            
            lzw decompressor;
            for (size_t i = start; i < end; i++) {
                decompressedChunks[i] = decompressor.decompress(chunkPtrs[i], compSizes[i]);
            }
        });
    }

    // Esperar a que todos los threads terminen
    for (auto& th : threads) {
        th.join();
    }

    // Escribir archivo descomprimido
    string outname = filename;
    if (outname.size() > 4 && outname.substr(outname.size() - 4) == ".lzw") {
        outname = outname.substr(0, outname.size() - 4);
    } else {
        outname += ".out";
    }

    int outfd = open(outname.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (outfd < 0) {
        perror("open out");
        delete[] fileBuf;
        return;
    }

    for (size_t i = 0; i < chunkCount; i++) {
        write(outfd, decompressedChunks[i].data(), decompressedChunks[i].size());
    }

    close(outfd);
    delete[] fileBuf;
}