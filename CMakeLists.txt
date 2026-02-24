cmake_minimum_required(VERSION 3.25)
project(pxkorka
        VERSION 0.1.0
        LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- OPTIONS ---
option(ENABLE_TESTS "Build tests" ON)

# Enable all warnings
if (MSVC)
    add_compile_options(/W4 /WX /permissive-)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wunused)
endif ()

# Dependencies
include(cmake/CPM.cmake)

if (ENABLE_TESTS)
    CPMAddPackage("gh:catchorg/Catch2@3.13.0")
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
endif ()

add_library(korka_lib
        include/korka/vm/vm_runtime.hpp src/vm/vm_runtime.cpp
        include/korka/vm/op_codes.hpp
        include/korka/vm/bytecode_builder.hpp
        include/korka/vm/options.hpp
        include/korka/utils/byte_writer.hpp
        include/korka/compiler/parser.hpp
        include/korka/shared.hpp
        include/korka/compiler/ast_walker.hpp
        include/korka/compiler/error.hpp
        include/korka/compiler/lex_token.hpp
        include/korka/utils/const_format.hpp
        include/korka/compiler/compiler.hpp
)

target_include_directories(korka_lib
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        INTERFACE
        3party
        src
)

add_executable(pxkorka main.cpp)
target_link_libraries(pxkorka PRIVATE korka_lib)

# --- TESTS ---
if (ENABLE_TESTS)
    enable_testing()

    add_executable(pxkorka_tests
            test/lexer.cpp
            test/bytecode_builder.cpp
            test/parser.cpp
    )

    target_link_libraries(pxkorka_tests
            PRIVATE
            korka_lib
            Catch2WithMain
    )

    catch_discover_tests(pxkorka_tests)
endif ()