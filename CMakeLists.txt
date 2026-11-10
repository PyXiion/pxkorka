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
option(KORKA_BUILD_BENCH "Build benchmark" OFF)

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

# C++26 reflection lets bound structs work without a korka::struct_def specialisation
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS -freflection)
check_cxx_source_compiles("
#ifndef __cpp_impl_reflection
#error reflection is not supported
#endif
int main() {}
" KORKA_HAS_REFLECTION)
unset(CMAKE_REQUIRED_FLAGS)

add_library(korka_lib INTERFACE)

target_include_directories(korka_lib
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3party>
        $<INSTALL_INTERFACE:include>
)

if (KORKA_HAS_REFLECTION)
    target_compile_options(korka_lib INTERFACE -freflection)
endif ()

add_executable(pxkorka main.cpp)
target_link_libraries(pxkorka PRIVATE korka_lib)

# --- TESTS ---
if (ENABLE_TESTS)
    enable_testing()

    add_executable(pxkorka_tests
            test/lexer.cpp
            test/e2e.cpp
    )

    target_link_libraries(pxkorka_tests
            PRIVATE
            korka_lib
            Catch2WithMain
    )

    catch_discover_tests(pxkorka_tests)
endif ()


# --- BENCH ---
if (KORKA_BUILD_BENCH)
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

        # The Lua repository has no lua.hpp, and a system one would pull in system Lua headers
        file(WRITE ${CMAKE_BINARY_DIR}/lua_compat/lua.hpp
                "extern \"C\" {\n#include \"lua.h\"\n#include \"lualib.h\"\n#include \"lauxlib.h\"\n}\n")

        target_include_directories(lua
                PUBLIC
                $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/lua_compat>
                $<BUILD_INTERFACE:${lua_SOURCE_DIR}>
        )
    endif()


    find_package(Python3 COMPONENTS Development REQUIRED)

    add_executable(bench bench.cpp)
    target_link_libraries(bench PUBLIC lua korka_lib)
    target_link_libraries(bench PRIVATE Python3::Python)
    target_include_directories(bench PRIVATE ${Python3_INCLUDE_DIRS})
endif ()
