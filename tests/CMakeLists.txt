cmake_minimum_required(VERSION 3.21)
project(NetBuffTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

if(NB_TEST_THIRD_PARTY_LIBS)
    include(FetchContent)

    # https://github.com/google/benchmark/issues/1845
    option(BENCHMARK_ENABLE_GTEST_TESTS FALSE)

    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(
        googletest
        googlebenchmark
        SFML
    )
endif()

add_executable(rbb_validate_automatic rbb_validate_automatic.cpp)
target_link_libraries(rbb_validate_automatic PRIVATE NetBuff)
target_compile_options(rbb_validate_automatic PRIVATE ${nb_compile_options})

add_executable(rbb_validate_handwritten rbb_validate_handwritten.cpp)
target_link_libraries(rbb_validate_handwritten PRIVATE NetBuff)
target_compile_options(rbb_validate_handwritten PRIVATE ${nb_compile_options})

add_executable(sb_validate_automatic sb_validate_automatic.cpp)
target_link_libraries(sb_validate_automatic PRIVATE NetBuff)
target_compile_options(sb_validate_automatic PRIVATE ${nb_compile_options})

add_executable(op_validate_automatic op_validate_automatic.cpp)
target_link_libraries(op_validate_automatic PRIVATE NetBuff)
target_compile_options(op_validate_automatic PRIVATE ${nb_compile_options})

add_executable(il_validate_handwritten il_validate_handwritten.cpp)
target_link_libraries(il_validate_handwritten PRIVATE NetBuff)
target_compile_options(il_validate_handwritten PRIVATE ${nb_compile_options})

if(NB_TEST_THIRD_PARTY_LIBS)
    add_executable(op_benchmark op_benchmark.cpp)
    target_link_libraries(op_benchmark PRIVATE NetBuff benchmark::benchmark)
    target_compile_options(op_benchmark PRIVATE ${nb_compile_options})

    add_executable(sb_benchmark sb_benchmark.cpp)
    target_link_libraries(sb_benchmark PRIVATE NetBuff benchmark::benchmark SFML::Network)
    target_compile_options(sb_benchmark PRIVATE ${nb_compile_options})
    set_property(TARGET sb_benchmark PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
