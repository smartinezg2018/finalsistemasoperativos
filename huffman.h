#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <string>
#include <vector>
#include <map>
#include <queue>

// Nodo del árbol de Huffman
struct HuffmanNode {
    unsigned char data;
    unsigned int freq;
    HuffmanNode *left, *right;

    HuffmanNode(unsigned char data, unsigned int freq) : data(data), freq(freq), left(nullptr), right(nullptr) {}

    // Sobrecarga del operador para la cola de prioridad (min-heap)
    bool operator>(const HuffmanNode& other) const {
        return freq > other.freq;
    }
};

class huffman {
public:
    void compress(const std::string& inputFilename);
    void decompress(const std::string& inputFilename);

private:
    void generateCodes(HuffmanNode* root, const std::string& code, std::map<unsigned char, std::string>& huffmanCodes);
    HuffmanNode* buildHuffmanTree(const std::map<unsigned char, unsigned int>& freqs);
};

#endif // HUFFMAN_H