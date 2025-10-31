CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread

OBJS = main.o huffman.o

all: gsea

gsea: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o gsea

main.o: main.cpp huffman.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

huffman.o: huffman.cpp huffman.hpp
	$(CXX) $(CXXFLAGS) -c huffman.cpp

clean:
	rm -f *.o gsea
