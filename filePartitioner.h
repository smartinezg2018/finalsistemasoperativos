#ifndef FILEPARTITIONER_H
#define FILEPARTITIONER_H
#include <string>
#include <cstddef>

using namespace std;

class filePartitioner {
public:
void compressLZW(const string& filename, size_t chunkSize = (1 << 20)); // Default 1MB chunks
void decompressLZW(const string& filename);
};

#endif 