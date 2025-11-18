#ifndef LZW_H
#define LZW_H

#include <iostream>
#include <map>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <map>

using namespace std;

class lzw {
public:

    map<string, int> asiicMapEnc();
    map<int, string> asiicMapDec();
    vector<unsigned char> compress(const unsigned char *input,size_t size);
    vector<unsigned char> decompress(const unsigned char *input,size_t size);
};

#endif // LZW_H
