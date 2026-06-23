CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -I/usr/local/include
LDFLAGS  := /usr/local/lib/libtinyxml2.a

TARGET   := search_engine
SRCS     := main.cpp KeywordProcessor.cpp DirectoryScanner.cpp PageProcessor.cpp
OBJS     := $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
