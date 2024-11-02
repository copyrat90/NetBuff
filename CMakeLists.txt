cmake_minimum_required(VERSION 3.21)
project(NetBuff)

option(NB_TEST_ENABLED FALSE)
option(NB_TEST_BENCHMARK FALSE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(nb_compile_options
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /Zc:preprocessor /source-charset:utf-8 /wd4324>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Wno-interference-size>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

add_library(NetBuff INTERFACE)

target_include_directories(NetBuff INTERFACE include/)
target_compile_options(NetBuff INTERFACE ${nb_compile_options})

if(NB_TEST_ENABLED)
    include(CTest)
    enable_testing()

    add_subdirectory(tests/)
    add_test(NAME test_rbb_validate_automatic COMMAND rbb_validate_automatic)
    add_test(NAME test_sb_validate_automatic COMMAND sb_validate_automatic)
    add_test(NAME test_op_validate_automatic COMMAND op_validate_automatic)
    add_test(NAME test_il_validate_automatic COMMAND il_validate_automatic)

    add_test(NAME test_srbb_validate_automatic COMMAND srbb_validate_automatic)
    set_tests_properties(test_srbb_validate_automatic PROPERTIES ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")
endif()
