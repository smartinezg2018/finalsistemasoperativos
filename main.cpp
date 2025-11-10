#include<iostream>
#include "lzw.h"
#include "huffman.h"

using namespace std;

int main(){
    lzw lzw;
    Huffman huffman;

    huffman.compress("lena.bmp");
    huffman.decompress("lena.bmp.huff");

    // lzw.compress("lena.bmp");
    // lzw.decompress("lena.bmp.lzw");

}
