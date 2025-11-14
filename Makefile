CXX = g++
CXXFLAGS = -std=c++11 -I./AES -O2
SRCS = aes_cli.cpp \
	AES/structures.cpp \
	AES/aes_encrypt.cpp \
	AES/AES_D_POSIX.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = aes_tool

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)