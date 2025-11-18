#ifndef FILEPARTITIONER_H
#define FILEPARTITIONER_H
#include <string>

using namespace std;

class filePartitioner {
public:
void compressLZW(const string& filename);
void decompressLZW(const string& filename);
};

#endif 