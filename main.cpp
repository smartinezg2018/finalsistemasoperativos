#include <iostream>
#include <vector>
#include <string>
#include "lzw.h"
#include "filePartitioner.h"

using namespace std;

int main() {
    filePartitioner partitioner;
    partitioner.compressLZW("greenland_grid_velo.bmp");

    partitioner.decompressLZW("greenland_grid_velo.bmp.lzw");

    return 0;
}