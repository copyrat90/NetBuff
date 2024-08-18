cmake_minimum_required(VERSION 3.21)
project(RingByteBufferTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

add_executable(rbb_validate rbb_validate.cpp)
target_link_libraries(rbb_validate PRIVATE RingByteBuffer)
target_compile_options(rbb_validate PRIVATE ${rbb_compile_options})

add_executable(rbb_resize_test rbb_resize_test.cpp)
target_link_libraries(rbb_resize_test PRIVATE RingByteBuffer)
target_compile_options(rbb_resize_test PRIVATE ${rbb_compile_options})
