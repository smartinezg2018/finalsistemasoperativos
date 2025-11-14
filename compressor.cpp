#include "compressor.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <vector>
#include <errno.h>

Compressor::Compressor() {}
Compressor::~Compressor() {}

void Compressor::setPassword(const std::string &pwd) {
    password = pwd;
}

ssize_t Compressor::safeRead(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *b = (char*)buf;
    while (total < count) {
        ssize_t r = ::read(fd, b + total, count - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        total += r;
    }
    return total;
}

ssize_t Compressor::safeWrite(int fd, const void *buf, size_t count) {
    size_t total = 0;
    const char *b = (const char*)buf;
    while (total < count) {
        ssize_t w = ::write(fd, b + total, count - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += w;
    }
    return total;
}

bool Compressor::pathIsFile(const std::string &path) {
    DIR* d = opendir(path.c_str());
    if (d) { closedir(d); return false; }
    return true;
}

uint64_t Compressor::fileSize(const std::string &path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return 0;
    off_t s = lseek(fd, 0, SEEK_END);
    close(fd);
    return (s < 0) ? 0 : (uint64_t)s;
}

// Recursively count bytes in folder and files
void Compressor::countInFolder(const std::string &path, uint64_t number[256], uint64_t &totalSize) {
    std::string dirpath = path;
    if (dirpath.back() != '/') dirpath.push_back('/');
    DIR *dir = opendir(dirpath.c_str());
    if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        const char *dn = ent->d_name;
        if (dn[0] == '.') {
            if (dn[1] == '\0') continue;
            if (dn[1] == '.' && dn[2] == '\0') continue;
        }
        // count filename bytes
        for (const char *c = dn; *c; ++c) number[(unsigned char)(*c)]++;

        std::string next = dirpath + dn;
        DIR *sub = opendir(next.c_str());
        if (sub) {
            closedir(sub);
            countInFolder(next, number, totalSize);
        } else {
            // file
            uint64_t sz = fileSize(next);
            totalSize += sz;
            // read file to count bytes
            int fd = open(next.c_str(), O_RDONLY);
            if (fd >= 0) {
                unsigned char buf[4096];
                ssize_t r;
                while ((r = safeRead(fd, buf, sizeof(buf))) > 0) {
                    for (ssize_t i = 0; i < r; ++i) number[buf[i]]++;
                }
                close(fd);
            }
        }
    }
    closedir(dir);
}

// Write single uChar mixing with current buffer per original logic
bool Compressor::writeBitsFromUChar(unsigned char uChar, unsigned char &current_byte, int &current_bit_count, int outfd) {
    // Based on original write_from_uChar:
    // current_byte <<= 8-current_bit_count;
    // current_byte |= (uChar >> current_bit_count);
    // fwrite(&current_byte,1,1,fp_write);
    // current_byte = uChar;
    int shift = 8 - current_bit_count;
    unsigned char out = (unsigned char)((current_byte << shift) | (uChar >> current_bit_count));
    if (safeWrite(outfd, &out, 1) != 1) return false;
    current_byte = uChar;
    return true;
}

bool Compressor::flushCurrentByte(unsigned char &current_byte, int &current_bit_count, int outfd) {
    if (current_bit_count == 0) return true; // nothing pending (in original, full byte already written)
    // pad left with zeros to fill 8 bits
    int pad = 8 - current_bit_count;
    unsigned char out = (unsigned char)(current_byte << pad);
    if (safeWrite(outfd, &out, 1) != 1) return false;
    current_bit_count = 0;
    current_byte = 0;
    return true;
}

void Compressor::writeFileCount(int fileCount, unsigned char &current_byte, int &current_bit_count, int outfd) {
    unsigned char low = fileCount % 256;
    unsigned char high = (fileCount / 256) % 256;
    writeBitsFromUChar(low, current_byte, current_bit_count, outfd);
    writeBitsFromUChar(high, current_byte, current_bit_count, outfd);
}

void Compressor::writeFileSize(uint64_t size, unsigned char &current_byte, int &current_bit_count, int outfd) {
    for (int i = 0; i < 8; ++i) {
        unsigned char b = (unsigned char)(size % 256);
        writeBitsFromUChar(b, current_byte, current_bit_count, outfd);
        size /= 256;
    }
}

void Compressor::writeFileName(const std::string &fileName, const std::string str_arr[256],
                               unsigned char &current_byte, int &current_bit_count, int outfd) {
    unsigned char len = (unsigned char)fileName.length();
    writeBitsFromUChar(len, current_byte, current_bit_count, outfd);
    for (char ch : fileName) {
        const std::string &code = str_arr[(unsigned char)ch];
        for (char bit : code) {
            if (current_bit_count == 8) {
                // write full current_byte
                if (safeWrite(outfd, &current_byte, 1) != 1) { return; }
                current_bit_count = 0;
            }
            current_byte <<= 1;
            if (bit == '1') current_byte |= 1;
            current_bit_count++;
        }
    }
}

void Compressor::writeFileContent(int infd, uint64_t size, const std::string str_arr[256],
                                  unsigned char &current_byte, int &current_bit_count, int outfd) {
    unsigned char buf[4096];
    uint64_t remaining = size;
    while (remaining) {
        ssize_t toRead = (remaining > sizeof(buf)) ? sizeof(buf) : (ssize_t)remaining;
        ssize_t r = safeRead(infd, buf, toRead);
        if (r <= 0) break;
        for (ssize_t i = 0; i < r; ++i) {
            const std::string &code = str_arr[buf[i]];
            for (char bit : code) {
                if (current_bit_count == 8) {
                    if (safeWrite(outfd, &current_byte, 1) != 1) return;
                    current_bit_count = 0;
                }
                current_byte <<= 1;
                if (bit == '1') current_byte |= 1;
                current_bit_count++;
            }
        }
        remaining -= r;
    }
}

void Compressor::writeFolder(const std::string &path, const std::string str_arr[256],
                             unsigned char &current_byte, int &current_bit_count, int outfd) {
    std::string dirpath = path;
    if (dirpath.back() != '/') dirpath.push_back('/');
    DIR *dir = opendir(dirpath.c_str());
    if (!dir) return;
    // Count entries (excluding . and ..)
    int file_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        const char *dn = ent->d_name;
        if (dn[0] == '.') {
            if (dn[1] == '\0') continue;
            if (dn[1] == '.' && dn[2] == '\0') continue;
        }
        file_count++;
    }
    rewinddir(dir);
    writeFileCount(file_count, current_byte, current_bit_count, outfd);

    while ((ent = readdir(dir)) != nullptr) {
        const char *dn = ent->d_name;
        if (dn[0] == '.') {
            if (dn[1] == '\0') continue;
            if (dn[1] == '.' && dn[2] == '\0') continue;
        }
        std::string next = dirpath + dn;
        if (pathIsFile(next)) {
            uint64_t sz = fileSize(next);
            // write fifth: file bit = 1
            if (current_bit_count == 8) {
                if (safeWrite(outfd, &current_byte, 1) != 1) { closedir(dir); return; }
                current_bit_count = 0;
            }
            current_byte <<= 1;
            current_byte |= 1;
            current_bit_count++;

            writeFileSize(sz, current_byte, current_bit_count, outfd);
            writeFileName(dn, str_arr, current_byte, current_bit_count, outfd);

            int infd = open(next.c_str(), O_RDONLY);
            if (infd >= 0) {
                writeFileContent(infd, sz, str_arr, current_byte, current_bit_count, outfd);
                close(infd);
            }
        } else {
            // folder bit = 0
            if (current_bit_count == 8) {
                if (safeWrite(outfd, &current_byte, 1) != 1) { closedir(dir); return; }
                current_bit_count = 0;
            }
            current_byte <<= 1;
            current_bit_count++;

            writeFileName(dn, str_arr, current_byte, current_bit_count, outfd);
            writeFolder(next, str_arr, current_byte, current_bit_count, outfd);
        }
    }
    closedir(dir);
}

bool Compressor::createArchive(const std::string &outputPath, const std::vector<std::string> &inputs) {
    if (inputs.empty()) return false;

    // 1) Count bytes across all inputs (names + file contents)
    uint64_t number[256];
    for (int i = 0; i < 256; ++i) number[i] = 0;
    uint64_t total_size = 0;

    // Also count filename bytes for each top-level input
    for (const std::string &p : inputs) {
        for (char c : p) number[(unsigned char)c]++;
        if (pathIsFile(p)) {
            uint64_t sz = fileSize(p);
            total_size += sz;
            int fd = open(p.c_str(), O_RDONLY);
            if (fd >= 0) {
                unsigned char buf[4096];
                ssize_t r;
                while ((r = safeRead(fd, buf, sizeof(buf))) > 0) {
                    for (ssize_t i = 0; i < r; ++i) number[buf[i]]++;
                }
                close(fd);
            }
        } else {
            countInFolder(p, number, total_size);
        }
    }

    // letter_count
    int letter_count = 0;
    for (int i = 0; i < 256; ++i) if (number[i]) letter_count++;
    if (letter_count == 0) letter_count = 256; // follow original behaviour (edge case)

    // Build ersel array (leaves only)
    std::vector<Ersel> arr;
    arr.reserve(letter_count * 2 - 1);
    for (int i = 0; i < 256; ++i) {
        if (number[i]) {
            Ersel e;
            e.left = e.right = nullptr;
            e.number = number[i];
            e.character = (unsigned char)i;
            arr.push_back(e);
        }
    }
    // sort by ascending frequency
    auto cmp = [](const Ersel &a, const Ersel &b) { return a.number < b.number; };
    std::sort(arr.begin(), arr.end(), cmp);

    // Build tree by original algorithm
    // We'll transform vector into nodes array of size letter_count*2-1
    int L = (int)arr.size();
    if (L == 0) return false;
    std::vector<Ersel> nodes(L * 2 - 1);
    // copy leaves
    for (int i = 0; i < L; ++i) nodes[i] = arr[i];
    // pointers/indices
    int min1i = 0, min2i = 1, currentIdx = L, notleaf = L, isleaf = 2;
    for (int i = 0; i < L - 1; ++i) {
        nodes[currentIdx].number = nodes[min1i].number + nodes[min2i].number;
        nodes[currentIdx].left = &nodes[min1i];
        nodes[currentIdx].right = &nodes[min2i];
        nodes[min1i].bit = "1";
        nodes[min2i].bit = "0";
        currentIdx++;

        // advance min1
        if (isleaf >= L) {
            min1i = notleaf++;
        } else {
            if (nodes[isleaf].number < nodes[notleaf].number) { min1i = isleaf++; }
            else { min1i = notleaf++; }
        }
        // advance min2
        if (isleaf >= L) {
            min2i = notleaf++;
        } else if (notleaf >= currentIdx) {
            min2i = isleaf++;
        } else {
            if (nodes[isleaf].number < nodes[notleaf].number) { min2i = isleaf++; }
            else { min2i = notleaf++; }
        }
    }

    // Build bitstrings from root to leaves (propagate)
    for (int i = (int)nodes.size() - 1; i >= 0; --i) {
        if (nodes[i].left) nodes[i].left->bit = nodes[i].bit + nodes[i].left->bit;
        if (nodes[i].right) nodes[i].right->bit = nodes[i].bit + nodes[i].right->bit;
    }

    // Build str_arr mapping (256 entries, empty if unused)
    std::string str_arr[256];
    for (int i = 0; i < (int)nodes.size(); ++i) {
        if (!nodes[i].left && !nodes[i].right) {
            unsigned char c = nodes[i].character;
            str_arr[c] = nodes[i].bit;
        }
    }

    // Prepare output filename (append .compressed)
    std::string outname = outputPath;
    if (outname.find(".compressed", outname.size() - 11) == std::string::npos) outname += ".compressed";
    int outfd = open(outname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) {
        std::cerr << "Cannot open output: " << outname << "\n";
        return false;
    }

    // Start writing bits according to format
    unsigned char current_byte = 0;
    int current_bit_count = 0;

    // write first: letter_count (1 byte)
    unsigned char letterCountByte = (unsigned char)letter_count;
    if (safeWrite(outfd, &letterCountByte, 1) != 1) { close(outfd); return false; }

    // write second: password_length (1 byte) and password bytes (if present)
    unsigned char passwdLen = (unsigned char)password.length();
    if (passwdLen > 0xFF) passwdLen = 0xFF;
    if (safeWrite(outfd, &passwdLen, 1) != 1) { close(outfd); return false; }
    if (passwdLen) {
        if (safeWrite(outfd, password.data(), passwdLen) != (ssize_t)passwdLen) { close(outfd); return false; }
    }

    // write third: for each unique byte -> byte (8 bits), len (8 bits), then code bits
    for (int i = 0; i < 256; ++i) {
        if (str_arr[i].empty()) continue;
        unsigned char uChar = (unsigned char)i;
        unsigned char len = (unsigned char)str_arr[i].length();
        // write uChar and len using writeBitsFromUChar (mixing)
        if (!writeBitsFromUChar(uChar, current_byte, current_bit_count, outfd)) { close(outfd); return false; }
        if (!writeBitsFromUChar(len, current_byte, current_bit_count, outfd)) { close(outfd); return false; }
        // then write the bit string for this byte
        const std::string &s = str_arr[i];
        for (char ch : s) {
            if (current_bit_count == 8) {
                if (safeWrite(outfd, &current_byte, 1) != 1) { close(outfd); return false; }
                current_bit_count = 0;
            }
            current_byte <<= 1;
            if (ch == '1') current_byte |= 1;
            current_bit_count++;
        }
    }

    // write fourth (top-level): file count (2 bytes, LSB first)
    int topFileCount = (int)inputs.size();
    writeFileCount(topFileCount, current_byte, current_bit_count, outfd);

    // write each top-level entry (file or folder)
    for (const std::string &p : inputs) {
        if (pathIsFile(p)) {
            uint64_t sz = fileSize(p);
            // write fifth: file bit = 1
            if (current_bit_count == 8) {
                if (safeWrite(outfd, &current_byte, 1) != 1) { close(outfd); return false; }
                current_bit_count = 0;
            }
            current_byte <<= 1;
            current_byte |= 1;
            current_bit_count++;

            writeFileSize(sz, current_byte, current_bit_count, outfd);
            // write seventh: name length + name (encoded)
            // top-level path can be a full path given by user; original wrote argv entry directly
            // We'll write the provided string's last path component as the name to preserve original behaviour
            std::string name = p;
            // If user passed a path with '/', we use full string (Original code used argv[i] which could be relative)
            // We'll write exactly what was passed.
            writeFileName(name, str_arr, current_byte, current_bit_count, outfd);

            int infd = open(p.c_str(), O_RDONLY);
            if (infd >= 0) {
                writeFileContent(infd, sz, str_arr, current_byte, current_bit_count, outfd);
                close(infd);
            }
        } else {
            // folder bit = 0
            if (current_bit_count == 8) {
                if (safeWrite(outfd, &current_byte, 1) != 1) { close(outfd); return false; }
                current_bit_count = 0;
            }
            current_byte <<= 1;
            current_bit_count++;

            std::string name = p;
            writeFileName(name, str_arr, current_byte, current_bit_count, outfd);
            writeFolder(p, str_arr, current_byte, current_bit_count, outfd);
        }
    }

    // flush last partial byte
    if (!flushCurrentByte(current_byte, current_bit_count, outfd)) { close(outfd); return false; }
    close(outfd);
    std::cout << "Created compressed file: " << outname << "\n";
    return true;
}
