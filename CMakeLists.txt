cmake_minimum_required(VERSION 4.0)
project(pxkorka)

set(CMAKE_CXX_STANDARD 23)

add_executable(pxkorka main.cpp
        include/korka/lexer.hpp
        3party/frozen/unordered_map.h
        include/korka/utils/utils.hpp
        include/korka/utils/string.hpp
        src/vm/vm_runtime.cpp
        include/korka/vm/vm_runtime.hpp
)
target_include_directories(pxkorka PRIVATE 3party)
target_include_directories(pxkorka PUBLIC include)