CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# Main program with menu
MAIN_TARGET = main
MAIN_OBJS = main.o menu.o filePartitioner.o lzw.o huffman.o fileEncrypting.o vigenere.o

# GSEA CLI program
GSEA_TARGET = gsea
GSEA_OBJS = gsea_cli.o filePartitioner.o lzw.o huffman.o fileEncrypting.o vigenere.o

# Default target
all: $(MAIN_TARGET) $(GSEA_TARGET)

# Main program target
$(MAIN_TARGET): $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) -o $(MAIN_TARGET) $(MAIN_OBJS)

main.o: main.cpp menu.h
	$(CXX) $(CXXFLAGS) -c main.cpp

menu.o: menu.cpp menu.h filePartitioner.h huffman.h fileEncrypting.h
	$(CXX) $(CXXFLAGS) -c menu.cpp

filePartitioner.o: filePartitioner.cpp filePartitioner.h lzw.h
	$(CXX) $(CXXFLAGS) -c filePartitioner.cpp

lzw.o: lzw.cpp lzw.h
	$(CXX) $(CXXFLAGS) -c lzw.cpp

huffman.o: huffman.cpp huffman.h
	$(CXX) $(CXXFLAGS) -c huffman.cpp

vigenere.o: vigenere.cpp vigenere.h
	$(CXX) $(CXXFLAGS) -c vigenere.cpp

fileEncrypting.o: fileEncrypting.cpp fileEncrypting.h vigenere.h
	$(CXX) $(CXXFLAGS) -c fileEncrypting.cpp

# GSEA CLI target
$(GSEA_TARGET): $(GSEA_OBJS)
	$(CXX) $(CXXFLAGS) -o $(GSEA_TARGET) $(GSEA_OBJS)

gsea_cli.o: gsea_cli.cpp filePartitioner.h huffman.h fileEncrypting.h
	$(CXX) $(CXXFLAGS) -c gsea_cli.cpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(MAIN_OBJS) $(GSEA_OBJS) $(MAIN_TARGET) $(GSEA_TARGET) *.o

.PHONY: all clean
