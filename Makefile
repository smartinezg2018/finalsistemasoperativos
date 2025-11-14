CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2
TARGET = huffman
OBJS = main.o HuffmanCompressor.o HuffmanDecompressor.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp HuffmanCompressor.h HuffmanDecompressor.h
	$(CXX) $(CXXFLAGS) -c main.cpp

HuffmanCompressor.o: HuffmanCompressor.cpp HuffmanCompressor.h
	$(CXX) $(CXXFLAGS) -c HuffmanCompressor.cpp

HuffmanDecompressor.o: HuffmanDecompressor.cpp HuffmanDecompressor.h
	$(CXX) $(CXXFLAGS) -c HuffmanDecompressor.cpp

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean