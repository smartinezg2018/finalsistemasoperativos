// huffman_posix.cpp
// Huffman compression/decompression using POSIX syscalls (open/read/write).
// Usage:
//   Compile: g++ -std=c++17 huffman_posix.cpp -O2 -o huffman
//   Compress:   ./huffman c input.bin output.huf
//   Decompress: ./huffman d input.huf output.bin

#include "huffman.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdint>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

using namespace std;

static const char MAGIC[4] = {'H','U','F','1'}; // file signature

// Node for Huffman tree
struct Node {
    unsigned char ch;
    uint64_t freq;
    Node* left;
    Node* right;
    Node(unsigned char c, uint64_t f) : ch(c), freq(f), left(nullptr), right(nullptr) {}
    Node(uint64_t f, Node* l, Node* r) : ch(0), freq(f), left(l), right(r) {}
};

struct Compare {
    bool operator()(const Node* a, const Node* b) const {
        return a->freq > b->freq;
    }
};

void freeTree(Node* root) {
    if (!root) return;
    freeTree(root->left);
    freeTree(root->right);
    delete root;
}

// Read frequencies by streaming the file (doesn't store entire file)
bool countFrequencies(const char* path, uint64_t freq[256], uint64_t &total_bytes) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open (countFrequencies)");
        return false;
    }
    memset(freq, 0, sizeof(uint64_t)*256);
    total_bytes = 0;

    const size_t BUF_SZ = 65536;
    vector<unsigned char> buf(BUF_SZ);
    ssize_t r;
    while ((r = read(fd, buf.data(), BUF_SZ)) > 0) {
        for (ssize_t i = 0; i < r; ++i) {
            ++freq[buf[i]];
        }
        total_bytes += (uint64_t)r;
    }
    if (r < 0) {
        perror("read (countFrequencies)");
        close(fd);
        return false;
    }
    close(fd);
    return true;
}

// Build Huffman tree from frequency table
Node* buildTreeFromFreq(const uint64_t freq[256]) {
    priority_queue<Node*, vector<Node*>, Compare> pq;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            pq.push(new Node((unsigned char)i, freq[i]));
        }
    }
    if (pq.empty()) {
        // empty file -> no nodes
        return nullptr;
    }
    while (pq.size() > 1) {
        Node* a = pq.top(); pq.pop();
        Node* b = pq.top(); pq.pop();
        Node* parent = new Node(a->freq + b->freq, a, b);
        pq.push(parent);
    }
    return pq.top();
}

// Generate codes (string of '0'/'1') for each byte
void generateCodes(Node* root, string &cur, vector<string>& codes) {
    if (!root) return;
    if (!root->left && !root->right) {
        codes[root->ch] = cur.empty() ? string("0") : cur; // handle single-symbol file
    } else {
        if (root->left) {
            cur.push_back('0');
            generateCodes(root->left, cur, codes);
            cur.pop_back();
        }
        if (root->right) {
            cur.push_back('1');
            generateCodes(root->right, cur, codes);
            cur.pop_back();
        }
    }
}

// Write little helper to write all bytes
ssize_t writeAll(int fd, const void* buf, size_t count) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
    size_t written = 0;
    while (written < count) {
        ssize_t w = write(fd, p + written, count - written);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += (size_t)w;
    }
    return (ssize_t)written;
}

// Write header:
// [4 bytes magic]
// [uint64_t original_size]
// [uint16_t num_symbols]
// For each symbol:
//   [uint8_t symbol][uint64_t freq]
bool writeHeader(int outfd, uint64_t original_size, const uint64_t freq[256]) {
    // magic
    if (writeAll(outfd, MAGIC, 4) < 0) { perror("write (magic)"); return false; }
    // original size
    if (writeAll(outfd, &original_size, sizeof(original_size)) < 0) { perror("write (orig size)"); return false; }
    // collect symbols
    uint16_t nsymbols = 0;
    for (int i = 0; i < 256; ++i) if (freq[i] > 0) ++nsymbols;
    if (writeAll(outfd, &nsymbols, sizeof(nsymbols)) < 0) { perror("write (nsymbols)"); return false; }
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            uint8_t sym = (uint8_t)i;
            if (writeAll(outfd, &sym, sizeof(sym)) < 0) { perror("write (sym)"); return false; }
            if (writeAll(outfd, &freq[i], sizeof(freq[i])) < 0) { perror("write (freq)"); return false; }
        }
    }
    return true;
}

// Compress: two-pass streaming (count freq, build tree, second pass encode & write)
// Output format as per header above followed by packed bits (MSB-first per byte)
bool compressFile(const char* inpath, const char* outpath) {
    uint64_t freq[256];
    uint64_t total_bytes = 0;
    if (!countFrequencies(inpath, freq, total_bytes)) return false;

    Node* root = buildTreeFromFreq(freq);
    if (!root) {
        // empty file -> create small header with original size 0 and no data
        int outfd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd < 0) { perror("open (out)"); return false; }
        if (!writeHeader(outfd, 0, freq)) { close(outfd); return false; }
        close(outfd);
        return true;
    }

    // generate codes
    vector<string> codes(256);
    string cur;
    generateCodes(root, cur, codes);

    // open files for second pass
    int infd = open(inpath, O_RDONLY);
    if (infd < 0) { perror("open (in second pass)"); freeTree(root); return false; }
    int outfd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) { perror("open (out)"); close(infd); freeTree(root); return false; }

    // write header
    if (!writeHeader(outfd, total_bytes, freq)) { close(infd); close(outfd); freeTree(root); return false; }

    // encode and write bits
    const size_t BUF_SZ = 65536;
    vector<unsigned char> buf(BUF_SZ);
    ssize_t r;
    unsigned char outbyte = 0;
    int bits_filled = 0; // number of bits in outbyte (from MSB side)
    auto flushOutByte = [&](bool pad_remaining) -> bool {
        if (bits_filled == 0) return true;
        // left-shift to align MSB-first: if bits_filled < 8, shift remaining to the left
        outbyte <<= (8 - bits_filled);
        if (writeAll(outfd, &outbyte, 1) < 0) { perror("write (data byte)"); return false; }
        outbyte = 0;
        bits_filled = 0;
        return true;
    };

    // Read input and encode
    while ((r = read(infd, buf.data(), BUF_SZ)) > 0) {
        for (ssize_t i = 0; i < r; ++i) {
            unsigned char symbol = buf[i];
            const string& code = codes[symbol];
            for (char bit : code) {
                // push bit to outbyte (MSB-first)
                outbyte = (outbyte << 1) | (bit == '1' ? 1 : 0);
                bits_filled++;
                if (bits_filled == 8) {
                    if (writeAll(outfd, &outbyte, 1) < 0) { perror("write (data)"); close(infd); close(outfd); freeTree(root); return false; }
                    outbyte = 0;
                    bits_filled = 0;
                }
            }
        }
    }
    if (r < 0) { perror("read (second pass)"); close(infd); close(outfd); freeTree(root); return false; }
    // flush remaining bits (pad with zeros on the right)
    if (!flushOutByte(true)) { close(infd); close(outfd); freeTree(root); return false; }

    close(infd);
    close(outfd);
    freeTree(root);
    return true;
}

// Read header; fills original_size and freq table. Assumes fd positioned at file start.
bool readHeader(int infd, uint64_t &original_size, uint64_t freq[256]) {
    // read magic
    char magic[4];
    if (read(infd, magic, 4) != 4) { perror("read (magic)"); return false; }
    if (memcmp(magic, MAGIC, 4) != 0) {
        cerr << "Invalid file format (magic mismatch)\n";
        return false;
    }
    // original size
    if (read(infd, &original_size, sizeof(original_size)) != (ssize_t)sizeof(original_size)) { perror("read (orig size)"); return false; }
    // num symbols
    uint16_t nsymbols;
    if (read(infd, &nsymbols, sizeof(nsymbols)) != (ssize_t)sizeof(nsymbols)) { perror("read (nsymbols)"); return false; }
    memset(freq, 0, sizeof(uint64_t)*256);
    for (uint16_t i = 0; i < nsymbols; ++i) {
        uint8_t sym;
        uint64_t f;
        if (read(infd, &sym, sizeof(sym)) != (ssize_t)sizeof(sym)) { perror("read (sym)"); return false; }
        if (read(infd, &f, sizeof(f)) != (ssize_t)sizeof(f)) { perror("read (freq)"); return false; }
        freq[sym] = f;
    }
    return true;
}

// Decode bits and write original bytes. We will read the rest of the file byte-by-byte and traverse tree.
bool decompressFile(const char* inpath, const char* outpath) {
    int infd = open(inpath, O_RDONLY);
    if (infd < 0) { perror("open (in decompress)"); return false; }
    uint64_t original_size;
    uint64_t freq[256];
    if (!readHeader(infd, original_size, freq)) { close(infd); return false; }

    Node* root = buildTreeFromFreq(freq);
    if (!root) {
        // empty original file
        int outfd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd < 0) { perror("open (out empty)"); close(infd); return false; }
        close(outfd);
        close(infd);
        return true;
    }

    int outfd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) { perror("open (out)"); close(infd); freeTree(root); return false; }

    const size_t BUF_SZ = 65536;
    vector<unsigned char> buf(BUF_SZ);
    ssize_t r;
    Node* curr = root;
    uint64_t bytes_written = 0;

    // Read rest of file
    while ((r = read(infd, buf.data(), BUF_SZ)) > 0) {
        for (ssize_t i = 0; i < r; ++i) {
            unsigned char byte = buf[i];
            // process 8 bits MSB-first
            for (int b = 7; b >= 0; --b) {
                int bit = (byte >> b) & 1;
                if (bit == 0) curr = curr->left;
                else curr = curr->right;

                if (!curr->left && !curr->right) {
                    // leaf
                    unsigned char outc = curr->ch;
                    if (writeAll(outfd, &outc, 1) < 0) { perror("write (decompress)"); close(infd); close(outfd); freeTree(root); return false; }
                    ++bytes_written;
                    if (bytes_written == original_size) {
                        // done; close and return
                        close(infd);
                        close(outfd);
                        freeTree(root);
                        return true;
                    }
                    curr = root;
                }
            }
        }
    }
    if (r < 0) { perror("read (decompress)"); close(infd); close(outfd); freeTree(root); return false; }

    // If we exit loop but didn't produce enough bytes, file may be corrupted/truncated
    if (bytes_written != original_size) {
        cerr << "Warning: decompressed bytes (" << bytes_written << ") != original size (" << original_size << ")\n";
        // still close and cleanup
    }

    close(infd);
    close(outfd);
    freeTree(root);
    return (bytes_written == original_size);
}
