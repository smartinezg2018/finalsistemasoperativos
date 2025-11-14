#ifndef DECOMPRESSOR_HPP
#define DECOMPRESSOR_HPP

#include <string>
#include <cstdint>

class Decompressor {
public:
    Decompressor();
    ~Decompressor();

    // Extracts a compressed archive (file or folder) at the given path
    bool extract(const std::string &compressedFile);

private:
    struct Translation {
        Translation *zero;
        Translation *one;
        unsigned char character;
        Translation() : zero(nullptr), one(nullptr), character(0) {}
    };

    // POSIX file descriptor for reading
    int fd;
    off_t totalSize;

    // --- Internal methods ---
    bool readByte(unsigned char &byte);
    bool readBit(bool &bit);
    unsigned char process8BitsNumber();
    void processNBitsToString(int n, Translation *node, unsigned char uChar);

    bool thisIsAFile();
    uint64_t readFileSize();
    void writeFileName(std::string &outName, int length);
    void translateFile(const std::string &path, uint64_t size);
    void translateFolder(const std::string &path);

    bool fileExists(const std::string &path);
    std::string changeNameIfExists(const std::string &name);

    void burnTree(Translation *node);

    // --- Internal state ---
    unsigned char currentByte;
    int currentBitCount;
    Translation *root;

    // Small helper functions
    bool readPasswordCheck();
    void loadTranslationTree(int letterCount);
};

#endif
