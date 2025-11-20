CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./AES -O2

# Main program with menu
MAIN_TARGET = main
MAIN_OBJS = main.o menu.o filePartitioner.o lzw.o huffman.o fileEncrypting.o \
	AES/structures.o AES/aes_encrypt.o AES/aes_decrypt.o

# Huffman project (standalone, if needed separately)
# HUFFMAN_TARGET = huffman
# HUFFMAN_OBJS = huffman_main.o huffman.o

# AES project
AES_SRCS = aes_cli.cpp \
	AES/structures.cpp \
	AES/aes_encrypt.cpp \
	AES/aes_decrypt.cpp
AES_OBJS = $(AES_SRCS:.cpp=.o)
AES_TARGET = aes_tool

# Default target
all: $(MAIN_TARGET) $(AES_TARGET)

# Main program target
$(MAIN_TARGET): $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) -o $(MAIN_TARGET) $(MAIN_OBJS)

main.o: main.cpp menu.h
	$(CXX) $(CXXFLAGS) -c main.cpp

menu.o: menu.cpp menu.h filePartitioner.h huffman.h fileEncrypting.h AES/structures.h
	$(CXX) $(CXXFLAGS) -c menu.cpp

filePartitioner.o: filePartitioner.cpp filePartitioner.h lzw.h
	$(CXX) $(CXXFLAGS) -c filePartitioner.cpp

lzw.o: lzw.cpp lzw.h
	$(CXX) $(CXXFLAGS) -c lzw.cpp

huffman.o: huffman.cpp huffman.h
	$(CXX) $(CXXFLAGS) -c huffman.cpp

fileEncrypting.o: fileEncrypting.cpp fileEncrypting.h AES/aes_encrypt.h AES/aes_decrypt.h
	$(CXX) $(CXXFLAGS) -c fileEncrypting.cpp

# Huffman target (commented out - Huffman is integrated in main)
# $(HUFFMAN_TARGET): $(HUFFMAN_OBJS)
# 	$(CXX) $(CXXFLAGS) -o $(HUFFMAN_TARGET) $(HUFFMAN_OBJS)

# AES target
$(AES_TARGET): $(AES_OBJS)
	$(CXX) $(CXXFLAGS) -o $(AES_TARGET) $(AES_OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(MAIN_OBJS) $(AES_OBJS) $(MAIN_TARGET) $(AES_TARGET)

.PHONY: all clean
