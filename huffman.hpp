// huffman.hpp
// Declaraciones de funciones Huffman (POSIX)

#ifndef HUFFMAN_HPP
#define HUFFMAN_HPP

#include <cstdint>

bool compressFile(const char* inpath, const char* outpath);
bool decompressFile(const char* inpath, const char* outpath);

#endif
