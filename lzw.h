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

using namespace std;

class lzw {
public:

    map<string, int> asiicMapEnc();
    map<int, string> asiicMapDec();
    void compress(const string& filename);
    void decompress(const string filename);
};

#endif // LZW_H
