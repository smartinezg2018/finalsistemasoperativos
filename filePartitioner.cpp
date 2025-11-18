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

using namespace std;

int bufferSize = (1 << 20);

void filePartitioner::compressLZW(const string& filename){
    // 1. Determine file size and read entire file
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        perror("Error getting file stats");
        return;
    }
    size_t fileSize = st.st_size;

    int readFile = open(filename.c_str(), O_RDONLY);
    if (readFile < 0) {
        perror("Error opening file for reading");
        return;
    }

    // Allocate memory for the entire file content
    unsigned char* fileBuffer = new unsigned char[fileSize];
    if (read(readFile, fileBuffer, fileSize) != fileSize) {
        cerr << "Error reading entire file." << endl;
        delete[] fileBuffer;
        close(readFile);
        return;
    }
    close(readFile);

    // 2. Compress the entire file content at once
    lzw lzw_obj;
    vector<unsigned char> compressedData = lzw_obj.compress(fileBuffer, fileSize);

    // 3. Write the compressed data to the output file
    string tempWrite = filename + ".lzw";
    int writeFile = open(tempWrite.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (writeFile < 0) {
        perror("Error opening file for writing");
        delete[] fileBuffer;
        return;
    }
    
    write(writeFile, compressedData.data(), compressedData.size());

    // Cleanup
    close(writeFile);
    delete[] fileBuffer;
}


void filePartitioner::decompressLZW(const string& filename) {
    // 1. Determine file size and read entire compressed file
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        perror("Error getting file stats");
        return;
    }
    size_t fileSize = st.st_size;

    int readFile = open(filename.c_str(), O_RDONLY);
    if (readFile < 0) {
        perror("Error opening file for reading");
        return;
    }

    unsigned char* fileBuffer = new unsigned char[fileSize];
    if (read(readFile, fileBuffer, fileSize) != fileSize) {
        cerr << "Error reading entire file." << endl;
        delete[] fileBuffer;
        close(readFile);
        return;
    }
    close(readFile);

    // 2. Decompress the entire compressed stream at once
    lzw lzw_obj;
    vector<unsigned char> decompressedData = lzw_obj.decompress(fileBuffer, fileSize);

    // 3. Write the decompressed data to the output file
    string tempWrite = filename;
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".lzw") {
        tempWrite = filename.substr(0, filename.size() - 4);
    } else {
        tempWrite += ".out";
    }

    int writeFile = open(tempWrite.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (writeFile < 0) {
        perror("Error opening file for writing");
        delete[] fileBuffer;
        return;
    }
    
    write(writeFile, decompressedData.data(), decompressedData.size());

    // Cleanup
    close(writeFile);
    delete[] fileBuffer;
}