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
#include<map>
#include <cstdint>
using namespace std;


map<string,int> lzw::asiicMapEnc(){
    map<string, int> table;
    for (int i = 0; i <= 255; i++) {
        string ch(1, char(i)); // Más eficiente que concatenar
        table[ch] = i;
    }
    return table;
}

map<int,string> lzw::asiicMapDec(){
    map<int, string> table;
    for (int i = 0; i <= 255; i++) {
        string ch(1, char(i)); // Más eficiente que concatenar
        table[i] = ch;
    }
    return table;
}

struct BitWriter {
    vector<unsigned char> data;
    uint32_t bitBuffer = 0;
    int bitCount = 0;

    void writeBits(uint32_t value, int bits) {
        bitBuffer = (bitBuffer << bits) | (value & ((1u << bits) - 1));
        bitCount += bits;

        while (bitCount >= 8) {
            bitCount -= 8;
            unsigned char byte = (bitBuffer >> bitCount) & 0xFF;
            data.push_back(byte);
        }
    }

    vector<unsigned char> finish() {
        if (bitCount > 0) {
            unsigned char byte = (bitBuffer << (8 - bitCount)) & 0xFF;
            data.push_back(byte);
        }
        return data;
    }
};

struct BitReader {
    const unsigned char *input;
    size_t size;
    size_t pos = 0;
    uint32_t bitBuffer = 0;
    int bitCount = 0;

    BitReader(const unsigned char* i, size_t s) : input(i), size(s) {}

    bool readBits(uint32_t &value, int bits) {
        while (bitCount < bits) {
            if (pos >= size) return false;
            bitBuffer = (bitBuffer << 8) | input[pos++];
            bitCount += 8;
        }

        bitCount -= bits;
        value = (bitBuffer >> bitCount) & ((1u << bits) - 1);
        return true;
    }
};

// **Correction: Use 12 bits for LZW codes, max dictionary size 4096.**
vector<unsigned char> lzw::compress(const unsigned char *input, size_t size) {
    if (size == 0) return {};

    // Initial dictionary: 0-255 ASCII
    map<string, int> table = asiicMapEnc(); 
    
    // Start of dictionary expansion at code 256
    int nextCode = 256; 
    const int MAX_CODES = 4096; // 12-bit LZW limit

    BitWriter writer;
    string currentString(1, input[0]);

    for (size_t i = 1; i < size; i++) {
        string nextChar(1, input[i]);
        string newString = currentString + nextChar;

        if (table.count(newString)) {
            currentString = newString;
        } else {
            // Output the code for the longest found prefix (currentString)
            writer.writeBits(table[currentString], 12);

            // Add new pattern to dictionary if there is space
            if (nextCode < MAX_CODES) {
                table[newString] = nextCode++;
            }
            
            // Start over with the new character
            currentString = nextChar;
        }
    }

    // Output the code for the last string
    writer.writeBits(table[currentString], 12); 

    // Return the bit-packed compressed data
    return writer.finish();
}


// **Correction: Use BitReader to correctly parse 12-bit codes.**
vector<unsigned char> lzw::decompress(const unsigned char* input, size_t length) {
    if (length == 0) return {};

    // Initial dictionary: 0-255 ASCII
    map<int, string> dictionary = asiicMapDec();
    
    // Start of dictionary expansion at code 256
    int nextCode = 256;
    const int MAX_CODES = 4096; // 12-bit LZW limit

    BitReader reader(input, length);
    vector<unsigned char> output;
    uint32_t prevCode;

    // Read first code
    if (!reader.readBits(prevCode, 12)) return {};

    string prevString = dictionary[prevCode];
    output.insert(output.end(), prevString.begin(), prevString.end());

    uint32_t currentCode;
    while (reader.readBits(currentCode, 12)) {
        string currentString;

        if (dictionary.count(currentCode)) {
            // Case 1: Code is in dictionary
            currentString = dictionary[currentCode];
        } else if (currentCode == nextCode) {
            // Case 2 (KLK): Code is P + first char of P
            currentString = prevString + prevString[0];
        } else {
            // Error: Invalid code (should not happen in a valid LZW stream)
            // For robustness, you might stop or throw an exception.
            break; 
        }

        // Output current string
        output.insert(output.end(), currentString.begin(), currentString.end());

        // Add new entry to dictionary: prevString + first char of currentString
        if (nextCode < MAX_CODES) {
            string newEntry = prevString + currentString[0];
            dictionary[nextCode++] = newEntry;
        }

        prevString = currentString;
    }

    return output;
}