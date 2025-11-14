#include <iostream>
#include <string>
#include <vector>
// #include "compressor.hpp"
// #include "decompressor.hpp"
#include "lzw.h"

int main() {
    lzw lzw;
    lzw.compress("lena.bmp");

    return 0;

}
