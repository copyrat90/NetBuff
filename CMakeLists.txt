cmake_minimum_required(VERSION 3.21)
project(NetBuff)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(nb_compile_options
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /Zc:preprocessor>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

add_library(NetBuff INTERFACE
    include/RingByteBuffer.hpp
)

target_include_directories(NetBuff INTERFACE include/)
target_compile_options(NetBuff INTERFACE ${nb_compile_options})

add_subdirectory(tests/)
add_test(NAME test_rbb_validate_automatic COMMAND rbb_validate_automatic)
add_test(NAME test_sb_validate_automatic COMMAND sb_validate_automatic)
