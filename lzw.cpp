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
#include "lzw.h"
using namespace std;


map<string,int> lzw::asiicMapEnc(){
    map<string, int> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[ch] = i;
    }
    return table;
    
}

map<int,string> lzw::asiicMapDec(){
    map<int, string> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[i] = ch;
    }
    return table;
    
}

#include <sstream>

void lzw::compress(const string& filename){
// en este metodo vamos a comprimir el archivo que sea, solo recibiendo su nombre;

    int readFile = open(filename.c_str(),O_RDONLY);
    string tempWrite = filename+".lzw";
    int writeFile = open(tempWrite.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);

    char zero = 0;
    write(writeFile, &zero, 1);
    write(writeFile,filename.data(),filename.size());
    write(writeFile, &zero, 1);

    unsigned char buffer;

    if(read(readFile,&buffer,1)<0)
        return;

    map<string, int> table = asiicMapEnc();
    string p = "", c = "";
    p += buffer;
    // cout<<"p;"<<p<<endl;
    int code = 256;
    while(read(readFile,&buffer,1) > 0){       
        c = buffer;
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            // output_code.push_back(table[p]);
            int num = table[p];
            unsigned char second_half = num & 0xFF;
            unsigned char first_half = (num >> 8) & 0xFF;
            write(writeFile, &second_half, 1);
            write(writeFile, &first_half, 1);
            // cout<<table[p]<<" = "<<num<<endl;
            if(table.size()<65536)
                table[p + c] = code;

            code++;
            p = c;
        }
        c = "";
        
    }
    int num = table[p];
    unsigned char second_half = num & 0xFF;
    unsigned char first_half = (num >> 8) & 0xFF;
    write(writeFile, &second_half, 1);
    write(writeFile, &first_half, 1);
    close(writeFile);
    close(readFile);

}

void lzw::decompress(const string filename){
    int readFile = open(filename.c_str(),O_RDONLY);

    unsigned char buffer[2];

    if(readFile<0)
        return;
    char zero = 0;
    string name = "";
    unsigned char nameBuffer;
    read(readFile, &nameBuffer, 1);
    while(read(readFile, &nameBuffer, 1) > 0 && nameBuffer != 0){
        name.push_back(nameBuffer);
    }
    
    
    
    
    int writeFile = open(name.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    
    read(readFile,&buffer,2);
    string final = "";
    map<int, string> table = asiicMapDec();
    int old = (buffer[1] << 8) | buffer[0], n;
    string s = table[old];
    write(writeFile,s.data(),s.size());
    string c = "";
    c += s[0];
    final+=s;
    int count = 256;

    while(read(readFile,&buffer,2) > 0){

        n = (buffer[1] << 8) | buffer[0];
        if (table.find(n) == table.end()) {
            s = table[old];
            s = s + c;
        }
        else {
            s = table[n];
        }
        // final+=s;
        write(writeFile,s.data(),s.size());
        c = "";
        c += s[0];
        table[count] = table[old] + c;
        count++;
        old = n;
        
    }


    close(readFile);
    close(writeFile);


    
}



