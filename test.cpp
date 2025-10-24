#include<iostream>
#include<map>
#include<vector>
#include<map>
#include<fcntl.h>
#include<unistd.h>
#include<cstring>
 
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

vector<int> encoding(string s){

    cout << "Encoding\n";
    map<string, int> table = asiicMapEnc();
    string p = "", c = "";
    p += s[0];
    int code = 256;
    vector<int> output_code;
    cout << "String\tOutput_Code\tAddition\n";
    for (int i = 0; i < s.length(); i++) {
        if (i != s.length() - 1)
            c += s[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            cout << p << "\t" << table[p] << "\t\t" 
                 << p + c << "\t" << code << endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    cout << p << "\t" << table[p] << endl;
    output_code.push_back(table[p]);
    return output_code;
}

void decoding(vector<int> op){
    cout << "\nDecoding\n";
    map<int, string> table = asiicMapDec();
    int old = op[0], n;
    string s = table[old];
    string c = "";
    c += s[0];
    cout << s;
    int count = 256;
    for (int i = 0; i < op.size() - 1; i++) {
        n = op[i + 1];
        if (table.find(n) == table.end()) {
            s = table[old];
            s = s + c;
        }
        else {
            s = table[n];
        }
        cout << s;
        c = "";
        c += s[0];
        table[count] = table[old] + c;
        count++;
        old = n;
    }
}


int main()
{
    string s = "WYS*WYGWYS*WYSWYSG";
    vector<int> output_code = encoding(s);
    cout << "Output Codes are: ";
    for (int i = 0; i < output_code.size(); i++) {
        cout << output_code[i] << " ";
    }
    cout << endl;
    decoding(output_code);
    cout<<endl;


    // int fd;
    // char buffer[80];
    // char msg[60] = "llenar el texo con muchas otras cosas nnuevasn";
    // fd = open("ejemplo.txt",O_RDWR);
    // cout<<"fd = "<<fd<<endl;

    // if(fd !=-1){
    //     // aqui se escribe el mensaje
    //     write(fd,msg,strlen(msg));
    //     // 
    //     lseek(fd,0,SEEK_SET);
    //     read(fd,buffer,strlen(msg));
    //     cout<<buffer<<endl;
    //     close(fd);
    // }
    
    
    
}
