cmake_minimum_required(VERSION 3.21)
project(NetBuff)

option(NB_MSVC_UTF8 "Use /utf-8 for MSVC" TRUE)

option(NB_TEST_ENABLED "Enable testing" FALSE)
option(NB_TEST_BENCHMARK "Enable benchmark tests" FALSE)
option(NB_TEST_MSVC_SANITIZER "Enable msvc sanitizer for tests" FALSE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(nb_compile_options
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /Zc:preprocessor /wd4324 $<IF:$<BOOL:${NB_MSVC_UTF8}>,/utf-8,/source-charset:utf-8>>
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
    add_test(NAME test_rq_validate_automatic COMMAND rq_validate_automatic)
    add_test(NAME test_sb_validate_automatic COMMAND sb_validate_automatic)
    add_test(NAME test_op_validate_automatic COMMAND op_validate_automatic)
    add_test(NAME test_il_validate_automatic COMMAND il_validate_automatic)

    add_test(NAME test_lop_validate_automatic_asan COMMAND lop_validate_automatic_asan)
    add_test(NAME test_lop_validate_automatic_tsan COMMAND lop_validate_automatic_tsan)
    set_tests_properties(test_lop_validate_automatic_tsan PROPERTIES ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")

    add_test(NAME test_srbb_validate_automatic COMMAND srbb_validate_automatic)
    set_tests_properties(test_srbb_validate_automatic PROPERTIES ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")
endif()
