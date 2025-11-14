#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <string>
#include <cstdint>
#include <vector>

class Compressor {
public:
    Compressor();
    ~Compressor();

    // Set password programmatically (empty string = no password)
    void setPassword(const std::string &pwd);

    // Create compressed archive from the given inputs.
    // outputPath will be used as base; ".compressed" will be appended.
    // inputs is a vector of paths (files or folders) to include (like argv[1..]).
    bool createArchive(const std::string &outputPath, const std::vector<std::string> &inputs);

private:
    struct Ersel {
        Ersel *left;
        Ersel *right;
        uint64_t number;
        unsigned char character;
        std::string bit;
        Ersel() : left(nullptr), right(nullptr), number(0), character(0), bit() {}
    };

    std::string password;

    // internal helpers
    bool pathIsFile(const std::string &path);
    uint64_t fileSize(const std::string &path);

    void countInFolder(const std::string &path, uint64_t number[256], uint64_t &totalSize);
    bool writeBitsFromUChar(unsigned char uChar, unsigned char &current_byte, int &current_bit_count, int outfd);
    bool flushCurrentByte(unsigned char &current_byte, int &current_bit_count, int outfd);

    void writeFileCount(int fileCount, unsigned char &current_byte, int &current_bit_count, int outfd);
    void writeFileSize(uint64_t size, unsigned char &current_byte, int &current_bit_count, int outfd);

    void writeFileName(const std::string &fileName, const std::string str_arr[256],
                       unsigned char &current_byte, int &current_bit_count, int outfd);

    void writeFileContent(int infd, uint64_t size, const std::string str_arr[256],
                          unsigned char &current_byte, int &current_bit_count, int outfd);

    void writeFolder(const std::string &path, const std::string str_arr[256],
                     unsigned char &current_byte, int &current_bit_count, int outfd);

    // utilities for safe I/O
    ssize_t safeRead(int fd, void *buf, size_t count);
    ssize_t safeWrite(int fd, const void *buf, size_t count);
};

#endif // COMPRESSOR_HPP
