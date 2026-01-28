cmake_minimum_required(VERSION 3.25)
project(pxkorka
        VERSION 0.1.0
        LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable all warnings
if (MSVC)
    add_compile_options(/W4 /WX /permissive-)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wunused)
endif ()

# Dependencies
include(FetchContent)

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        SYSTEM
)
FetchContent_MakeAvailable(Catch2)

add_library(korka_lib
        src/vm/vm_runtime.cpp
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
option(ENABLE_TESTS "Build tests" ON)

if (ENABLE_TESTS)
    enable_testing()

    add_executable(pxkorka_tests
            test/lexer.cpp
    )

    target_link_libraries(pxkorka_tests
            PRIVATE
            korka_lib
            Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(pxkorka_tests)
endif ()