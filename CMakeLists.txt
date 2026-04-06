cmake_minimum_required(VERSION 3.25)
project(pxkorka
        VERSION 0.1.0
        LANGUAGES C CXX
)


set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# --- OPTIONS ---
option(ENABLE_TESTS "Build tests" ON)

# Enable all warnings
if (MSVC)
    add_compile_options(/W4 /WX /permissive-)
else ()
#    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wunused)
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
        include/korka/shared/error.hpp
        include/korka/compiler/lex_token.hpp
        include/korka/utils/const_format.hpp
        include/korka/compiler/compiler.hpp
        include/korka/utils/overloaded.hpp
        include/korka/shared/types.hpp
        include/korka/shared/flat_map.hpp
        include/korka/utils/frozen_hash_string_view.hpp
        include/korka/utils/byte_reader.hpp
        include/korka/utils/function_traits.hpp
        include/korka/compiler/binding.hpp
        include/korka/compiler/info.hpp
        include/korka/vm/value.hpp
        include/korka/compiler/result.hpp
        include/korka/compiler/binding_wrapper.hpp
        include/korka/vm/context_base.hpp
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

# Ignore unused-but-set-variable shadow
target_compile_options(korka_lib PUBLIC -Wno-error=unused-but-set-variable -Wno-error=shadow)

# --- TESTS ---
#if (ENABLE_TESTS)
#    enable_testing()
#
#    add_executable(pxkorka_tests
#            test/lexer.cpp
#            test/bytecode_builder.cpp
#            test/parser.cpp
#    )
#
#    target_link_libraries(pxkorka_tests
#            PRIVATE
#            korka_lib
#            Catch2WithMain
#    )
#
#    catch_discover_tests(pxkorka_tests)
#endif ()


# --- BENCH ---
CPMAddPackage(
        NAME lua
        GIT_REPOSITORY https://github.com/lua/lua.git
        VERSION 5.3.5
        DOWNLOAD_ONLY YES
)

if (lua_ADDED)
    # lua has no CMake support, so we create our own target

    FILE(GLOB lua_sources ${lua_SOURCE_DIR}/*.c)
    list(REMOVE_ITEM lua_sources "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c")
    add_library(lua STATIC ${lua_sources})

    target_include_directories(lua
            PUBLIC
            $<BUILD_INTERFACE:${lua_SOURCE_DIR}>
    )
endif()


find_package(Python3 COMPONENTS Development REQUIRED)

add_executable(bench bench.cpp)
target_link_libraries(bench PUBLIC lua korka_lib)
target_link_libraries(bench PRIVATE Python3::Python)
target_include_directories(bench PRIVATE ${Python3_INCLUDE_DIRS})