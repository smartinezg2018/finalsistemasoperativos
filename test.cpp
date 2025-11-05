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
using namespace std;



map<string,int> asiicMapEnc(){
    map<string, int> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[ch] = i;
    }
    return table;
    
}

map<int,string> asiicMapDec(){
    map<int, string> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[i] = ch;
    }
    return table;
    
}


// cuando quiera encriptar tomar el nombre de un archivo y que se encripte con el mimso nombre

string readFile(const string& filename){

    struct stat st;
    stat(filename.c_str(),&st);
    off_t fileSize = st.st_size;
    int fr = open(filename.c_str(),O_RDWR);
    char* buffer = new char[fileSize];
    read(fr,buffer,fileSize);
    string temp = buffer;
    cout<<temp<<endl;
    delete[] buffer;
    return temp;
    
}

// cuando quiera desincriptar solo dar el nombre del.bin
void writingFile(const string& filename,vector<int> codes){
    int fr = open(filename.c_str(),O_RDWR |O_CREAT);
    for(int code : codes){
        u_int8_t second_half = code & 0xFF;
        u_int8_t first_half = (code >> 8) & 0xFF;
        write(fr, &second_half, 1);
        write(fr, &first_half, 1);
    }
    close(fr);
    
}



vector<int> encoding(string s){

    map<string, int> table = asiicMapEnc();
    string p = "", c = "";
    p += s[0];
    int code = 256;
    vector<int> output_code;
    for (int i = 0; i < s.length(); i++) {
        if (i != s.length() - 1)
            c += s[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    output_code.push_back(table[p]);
    return output_code;
}



string decoding(const string& fileName){

    int fd = open(fileName.c_str(),O_RDONLY);
    vector<int> codes;
    unsigned char buffer[2];
    int temp;
    while(read(fd,&buffer,2) > 0){
        
        temp =  (buffer[1] << 8) | buffer[0];
        codes.push_back(temp);
        cout<<temp<<endl;
        
    }
    return "";
    
    string final = "";
    map<int, string> table = asiicMapDec();
    int old = codes[0], n;
    string s = table[old];
    string c = "";
    c += s[0];
    final+=s;
    int count = 256;
    for (int i = 0; i < codes.size() - 1; i++) {
        n = codes[i + 1];
        if (table.find(n) == table.end()) {
            s = table[old];
            s = s + c;
        }
        else {
            s = table[n];
        }
        final+=s;
        c = "";
        c += s[0];
        table[count] = table[old] + c;
        count++;
        old = n;
    }
    close(fd);
    return final;
}


void compress(const string& filename){
// en este metodo vamos a comprimir el archivo que sea, solo recibiendo su nombre;


    int readFile = open(filename.c_str(),O_RDONLY);
    int writeFile = open("compressed.bin", O_RDWR | O_CREAT | O_TRUNC, 0666);
    unsigned char buffer;
    // struct stat st;
    // stat(filename.c_str(),&st);
    // off_t fileSize = st.st_size;

    if(read(readFile,&buffer,1)<0){
        return;
    }

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
            u_int8_t second_half = num & 0xFF;
            u_int8_t first_half = (num >> 8) & 0xFF;
            write(writeFile, &second_half, 1);
            write(writeFile, &first_half, 1);
            // cout<<table[p]<<" = "<<num<<endl;
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
        
    }
    int num = table[p];
    u_int8_t second_half = num & 0xFF;
    u_int8_t first_half = (num >> 8) & 0xFF;
    write(writeFile, &second_half, 1);
    write(writeFile, &first_half, 1);
    close(writeFile);
    close(readFile);

}

void uncompress(const string filename){
    int readFile = open(filename.c_str(),O_RDONLY);
    int writeFile = open("uncompressed.bmp", O_RDWR | O_CREAT | O_TRUNC, 0666);
    unsigned char buffer[2];
    if(read(readFile,&buffer,2)<0)
        return;

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

int main(){

    // cout<<convertDecimaltoBinary(10)<<endl;
    // string text = R"(
    // asdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasdasd
    // )";
    // writingFile("plan.bin",encoding(text));

    compress("lena.bmp");
    uncompress("compressed.bin");
    // cout<<readFile("test.bin");



    
}
