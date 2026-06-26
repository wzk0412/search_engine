CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -I/usr/local/include
LDFLAGS  := /usr/local/lib/libtinyxml2.a

# ---- 离线处理 (search_engine) ----
TARGET   := search_engine
SRCS     := main.cpp KeywordProcessor.cpp DirectoryScanner.cpp PageProcessor.cpp
OBJS     := $(SRCS:.cpp=.o)

# ---- 在线客户端 (search_client) ----
CLIENT_TARGET := search_client
CLIENT_SRCS   := Client.cpp ChatClient.cpp Server.cpp \
                 CacheManager.cpp RedisCache.cpp
CLIENT_OBJS   := $(CLIENT_SRCS:.cpp=.o)
CLIENT_LIBS   := /usr/local/lib/libmuduo_net.a \
                 /usr/local/lib/libmuduo_base.a \
                 -lpthread -lhiredis

# ---- 在线服务器 (search_server) ----
SERVER_TARGET := search_server
SERVER_SRCS   := Server_main.cpp Server.cpp \
                 CacheManager.cpp RedisCache.cpp
SERVER_OBJS   := $(SERVER_SRCS:.cpp=.o)
SERVER_LIBS   := /usr/local/lib/libmuduo_net.a \
                 /usr/local/lib/libmuduo_base.a \
                 -lpthread -lhiredis

.PHONY: all clean

all: $(TARGET) $(CLIENT_TARGET) $(SERVER_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CLIENT_LIBS)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SERVER_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(CLIENT_OBJS) $(SERVER_OBJS) \
	      $(TARGET) $(CLIENT_TARGET) $(SERVER_TARGET)
