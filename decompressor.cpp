#include "decompressor.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <iomanip>

#define CHECK_BIT 0b10000000

using namespace std;

Decompressor::Decompressor()
    : fd(-1), totalSize(0), currentByte(0), currentBitCount(0), root(nullptr) {}

Decompressor::~Decompressor() {
    if (fd >= 0) close(fd);
    if (root) burnTree(root);
}

// Helper function to print tree structure
void printTree(Translation *node, string prefix = "", bool isLeft = true) {
    if (!node) return;
    
    if (!node->zero && !node->one) {
        // Leaf node
        cerr << prefix << (isLeft ? "├─" : "└─") << "LEAF: '" << node->character 
             << "' (ASCII " << (int)(unsigned char)node->character << ")" << endl;
        return;
    }
    
    cerr << prefix << (isLeft ? "├─" : "└─") << "NODE" << endl;
    if (node->zero) {
        printTree(node->zero, prefix + (isLeft ? "│  " : "   "), true);
    }
    if (node->one) {
        printTree(node->one, prefix + (isLeft ? "│  " : "   "), false);
    }
}

bool Decompressor::extract(const std::string &compressedFile) {
    cerr << "\n=== STARTING DECOMPRESSION DEBUG ===" << endl;
    cerr << "Opening file: " << compressedFile << endl;
    
    fd = open(compressedFile.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Error: cannot open " << compressedFile << std::endl;
        return false;
    }

    struct stat st;
    fstat(fd, &st);
    totalSize = st.st_size;
    cerr << "File size: " << totalSize << " bytes" << endl;

    unsigned char letterCount = 0;
    if (!readByte(letterCount)) return false;
    cerr << "Letter count byte: " << (int)letterCount << endl;
    if (letterCount == 0) letterCount = 256;
    cerr << "Actual letter count: " << (int)letterCount << endl;

    if (!readPasswordCheck()) {
        close(fd);
        return false;
    }

    cerr << "\n=== BUILDING HUFFMAN TREE ===" << endl;
    root = new Translation;
    loadTranslationTree(letterCount);
    
    cerr << "\n=== HUFFMAN TREE STRUCTURE ===" << endl;
    printTree(root);
    
    cerr << "\n=== READING FILE COUNT ===" << endl;
    int low = process8BitsNumber();
    int high = process8BitsNumber();
    int fileCount = low + 256 * high;
    cerr << "File count: " << fileCount << " (low=" << low << ", high=" << high << ")" << endl;

    for (int i = 0; i < fileCount; ++i) {
        cerr << "\n=== PROCESSING ITEM " << (i+1) << "/" << fileCount << " ===" << endl;
        if (thisIsAFile()) {
            cerr << "Type: FILE" << endl;
            uint64_t size = readFileSize();
            cerr << "File size: " << size << " bytes" << endl;
            
            int nameLen = process8BitsNumber();
            cerr << "Filename length: " << nameLen << endl;
            
            std::string name;
            writeFileName(name, nameLen);
            cerr << "Decoded filename: '" << name << "'" << endl;
            
            name = changeNameIfExists(name);
            cerr << "Final filename: '" << name << "'" << endl;
            translateFile(name, size);
        } else {
            cerr << "Type: DIRECTORY" << endl;
            int nameLen = process8BitsNumber();
            cerr << "Directory name length: " << nameLen << endl;
            
            std::string name;
            writeFileName(name, nameLen);
            cerr << "Decoded directory name: '" << name << "'" << endl;
            
            name = changeNameIfExists(name);
            cerr << "Final directory name: '" << name << "'" << endl;
            mkdir(name.c_str(), 0755);
            translateFolder(name);
        }
    }

    std::cout << "\n✅ Decompression complete.\n";
    return true;
}

bool Decompressor::readByte(unsigned char &byte) {
    bool success = read(fd, &byte, 1) == 1;
    if (success) {
        cerr << "[READ BYTE] 0x" << hex << setw(2) << setfill('0') 
             << (int)byte << dec << " (" << (int)byte << ")" << endl;
    } else {
        cerr << "[READ BYTE] FAILED!" << endl;
    }
    return success;
}

bool Decompressor::readBit(bool &bit) {
    if (currentBitCount == 0) {
        if (!readByte(currentByte)) return false;
        currentBitCount = 8;
        cerr << "[BIT BUFFER] Loaded new byte: " << bitset<8>(currentByte) << endl;
    }
    bit = currentByte & CHECK_BIT;
    cerr << "[READ BIT] " << (bit ? "1" : "0") 
         << " (remaining bits: " << (currentBitCount-1) << ")" << endl;
    currentByte <<= 1;
    currentBitCount--;
    return true;
}

unsigned char Decompressor::process8BitsNumber() {
    unsigned char val, temp;
    if (!readByte(temp)) return 0;
    
    cerr << "[8-BIT NUMBER] currentByte=" << bitset<8>(currentByte) 
         << ", temp=" << bitset<8>(temp) 
         << ", currentBitCount=" << currentBitCount << endl;
    
    val = currentByte | (temp >> currentBitCount);
    currentByte = temp << (8 - currentBitCount);
    
    cerr << "[8-BIT NUMBER] Result: " << (int)val << " (0x" << hex 
         << setw(2) << setfill('0') << (int)val << dec << ")" << endl;
    
    return val;
}

void Decompressor::processNBitsToString(int n, Translation *node, unsigned char uChar) {
    cerr << "  Building path for '" << uChar << "' (ASCII " << (int)uChar 
         << ") with " << n << " bits:" << endl;
    cerr << "  Path: ";
    
    for (int i = 0; i < n; ++i) {
        bool bit;
        if (!readBit(bit)) return;
        cerr << (bit ? "1" : "0");
        
        if (bit) {
            if (!node->one) node->one = new Translation;
            node = node->one;
        } else {
            if (!node->zero) node->zero = new Translation;
            node = node->zero;
        }
    }
    cerr << " -> '" << uChar << "'" << endl;
    node->character = uChar;
}

bool Decompressor::thisIsAFile() {
    bool bit;
    readBit(bit);
    cerr << "[FILE CHECK] " << (bit ? "FILE" : "DIRECTORY") << endl;
    return bit;
}

uint64_t Decompressor::readFileSize() {
    uint64_t size = 0;
    uint64_t mult = 1;
    cerr << "[FILE SIZE] Reading 8 bytes (little-endian):" << endl;
    for (int i = 0; i < 8; ++i) {
        unsigned char byte = process8BitsNumber();
        cerr << "  Byte " << i << ": " << (int)byte << endl;
        size += byte * mult;
        mult *= 256;
    }
    cerr << "[FILE SIZE] Total: " << size << endl;
    return size;
}

void Decompressor::writeFileName(std::string &outName, int length) {
    outName.reserve(length);
    cerr << "[FILENAME DECODE] Length: " << length << endl;
    
    for (int i = 0; i < length; ++i) {
        Translation *node = root;
        bool bit;
        int depth = 0;
        string path = "";
        
        cerr << "  Char " << i << ": ";
        
        while (node->zero || node->one) {
            if (!readBit(bit)) {
                cerr << " READ BIT FAILED!" << endl;
                return;
            }
            path += (bit ? "1" : "0");
            node = bit ? node->one : node->zero;
            depth++;
            
            if (depth > 256) {
                cerr << " INFINITE LOOP DETECTED!" << endl;
                outName.push_back('?');
                break;
            }
            
            if (!node) {
                cerr << " NULL NODE at depth " << depth << "!" << endl;
                outName.push_back('?');
                break;
            }
        }
        
        if (node && !node->zero && !node->one) {
            cerr << "'" << node->character << "' (ASCII " 
                 << (int)(unsigned char)node->character << ") via path: " << path << endl;
            outName.push_back(node->character);
        }
    }
    
    cerr << "[FILENAME DECODE] Result: '" << outName << "'" << endl;
}

void Decompressor::translateFile(const std::string &path, uint64_t size) {
    cout << "Creating file: " << path << endl;
    int outfd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (outfd < 0) {
        std::cerr << "Error creating file: " << path << "\n";
        return;
    }

    cerr << "[FILE CONTENT] Decoding " << size << " characters..." << endl;
    for (uint64_t i = 0; i < size; ++i) {
        Translation *node = root;
        bool bit;
        while (node->zero || node->one) {
            readBit(bit);
            node = bit ? node->one : node->zero;
        }
        write(outfd, &node->character, 1);
        
        // Show first few characters
        if (i < 20) {
            cerr << "  Byte " << i << ": '" << node->character 
                 << "' (ASCII " << (int)(unsigned char)node->character << ")" << endl;
        }
    }

    close(outfd);
    cerr << "[FILE CONTENT] Done" << endl;
}

void Decompressor::translateFolder(const std::string &path) {
    cerr << "\n[FOLDER] Entering: " << path << endl;
    int low = process8BitsNumber();
    int high = process8BitsNumber();
    int fileCount = low + 256 * high;
    cerr << "[FOLDER] Contains " << fileCount << " items" << endl;

    for (int i = 0; i < fileCount; ++i) {
        if (thisIsAFile()) {
            uint64_t size = readFileSize();
            int nameLen = process8BitsNumber();
            std::string name;
            writeFileName(name, nameLen);
            std::string fullPath = path + "/" + name;
            translateFile(fullPath, size);
        } else {
            int nameLen = process8BitsNumber();
            std::string name;
            writeFileName(name, nameLen);
            std::string fullPath = path + "/" + name;
            mkdir(fullPath.c_str(), 0755);
            translateFolder(fullPath);
        }
    }
}

bool Decompressor::fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string Decompressor::changeNameIfExists(const std::string &name) {
    if (!fileExists(name)) return name;
    std::string base = name;
    std::string ext;
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        base = name.substr(0, dot);
        ext = name.substr(dot);
    }
    for (int i = 1; i < 100; ++i) {
        std::string alt = base + "(" + std::to_string(i) + ")" + ext;
        if (!fileExists(alt)) return alt;
    }
    return base + "_copy" + ext;
}

void Decompressor::burnTree(Translation *node) {
    if (!node) return;
    burnTree(node->zero);
    burnTree(node->one);
    delete node;
}

bool Decompressor::readPasswordCheck() {
    unsigned char passLen;
    if (!readByte(passLen)) return false;
    cerr << "[PASSWORD] Length: " << (int)passLen << endl;
    
    if (passLen == 0) {
        cerr << "[PASSWORD] No password required" << endl;
        return true;
    }

    std::vector<char> stored(passLen);
    if (read(fd, stored.data(), passLen) != passLen) return false;
    
    cerr << "[PASSWORD] Stored password (hex): ";
    for (size_t i = 0; i < passLen; ++i) {
        cerr << hex << setw(2) << setfill('0') << (int)(unsigned char)stored[i] << " ";
    }
    cerr << dec << endl;

    std::string input;
    std::cout << "Enter password: ";
    std::cin >> input;

    if (input.size() != passLen) {
        cerr << "[PASSWORD] Length mismatch" << endl;
        return false;
    }
    for (size_t i = 0; i < passLen; ++i) {
        if (stored[i] != input[i]) {
            std::cout << "Wrong password.\n";
            return false;
        }
    }
    std::cout << "Password OK.\n";
    return true;
}

void Decompressor::loadTranslationTree(int letterCount) {
    cerr << "[TREE BUILD] Loading " << letterCount << " characters" << endl;
    
    for (int i = 0; i < letterCount; ++i) {
        unsigned char currentChar = process8BitsNumber();
        int len = process8BitsNumber();
        if (len == 0) len = 256;
        
        cerr << "\n[TREE BUILD] Entry " << (i+1) << "/" << letterCount << ":" << endl;
        cerr << "  Character: '" << currentChar << "' (ASCII " << (int)currentChar << ")" << endl;
        cerr << "  Code length: " << len << " bits" << endl;
        
        processNBitsToString(len, root, currentChar);
    }
    
    cerr << "\n[TREE BUILD] Complete!" << endl;
}