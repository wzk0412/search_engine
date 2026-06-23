cmake_minimum_required(VERSION 3.10)
project(search_engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件目录（cppjieba / simhash / utfcpp 均为 header-only）
include_directories(/usr/local/include)

# 源文件
add_executable(search_engine
    main.cpp
    KeywordProcessor.cpp
    DirectoryScanner.cpp
)

# 链接 tinyxml2 静态库
target_link_libraries(search_engine
    /usr/local/lib/libtinyxml2.a
)
