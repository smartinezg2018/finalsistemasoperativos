#include<iostream>
#include "lzw.h"

using namespace std;

int main(){
    lzw lzw;

    // lzw.compress("lena.bmp");
    lzw.decompress("lena.bmp.lzw");

}
