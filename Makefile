CXX = g++
CXXFLAGS = -std=c++11 -Wall -I./AES -O2

# Huffman project
HUFFMAN_TARGET = huffman
HUFFMAN_OBJS = main.o HuffmanCompressor.o HuffmanDecompressor.o

# AES project
AES_SRCS = aes_cli.cpp \
	AES/structures.cpp \
	AES/aes_encrypt.cpp \
	AES/aes_decrypt.cpp
AES_OBJS = $(AES_SRCS:.cpp=.o)
AES_TARGET = aes_tool

# Default target
all: $(HUFFMAN_TARGET) $(AES_TARGET)

# Huffman target
$(HUFFMAN_TARGET): $(HUFFMAN_OBJS)
	$(CXX) $(CXXFLAGS) -o $(HUFFMAN_TARGET) $(HUFFMAN_OBJS)

main.o: main.cpp HuffmanCompressor.h HuffmanDecompressor.h
	$(CXX) $(CXXFLAGS) -c main.cpp

HuffmanCompressor.o: HuffmanCompressor.cpp HuffmanCompressor.h
	$(CXX) $(CXXFLAGS) -c HuffmanCompressor.cpp

HuffmanDecompressor.o: HuffmanDecompressor.cpp HuffmanDecompressor.h
	$(CXX) $(CXXFLAGS) -c HuffmanDecompressor.cpp

# AES target
$(AES_TARGET): $(AES_OBJS)
	$(CXX) $(CXXFLAGS) -o $(AES_TARGET) $(AES_OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(HUFFMAN_OBJS) $(AES_OBJS) $(HUFFMAN_TARGET) $(AES_TARGET)

.PHONY: all clean
