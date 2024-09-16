#include <benchmark/benchmark.h>

#define NB_OBJ_POOL_CHECK false
#include "ObjectPool.hpp"

#include "SerializeBuffer.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

inline constexpr int PHASES = 100;
inline constexpr int NEW_DEL_COUNT = 100000;

inline constexpr int BUFFER_CAPACITY = 4096;

inline constexpr std::string_view QUICK_BROWN_FOX = "The quick brown fox jumps over the lazy dog!";

} // namespace

template <typename T, typename... Args>
void new_delete(benchmark::State& state, Args&&... args)
{
    std::vector<T*> datas(NEW_DEL_COUNT);

    for (auto _ : state)
    {
        for (int p = 0; p < PHASES; ++p)
        {
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                datas[c] = new T(std::forward<Args>(args)...);
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                delete datas[c];
        }
    }
}

template <typename T, typename... Args>
void obj_pool_call_dtor(benchmark::State& state, Args&&... args)
{
    std::vector<T*> datas(NEW_DEL_COUNT);

    for (auto _ : state)
    {
        nb::ObjectPool<T, true> pool;

        for (int p = 0; p < PHASES; ++p)
        {
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                datas[c] = &pool.construct(std::forward<Args>(args)...);
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                pool.destroy(*datas[c]);
        }
    }
}

template <typename T, typename... Args>
void obj_pool_call_dtor_reserve(benchmark::State& state, Args&&... args)
{
    std::vector<T*> datas(NEW_DEL_COUNT);

    for (auto _ : state)
    {
        nb::ObjectPool<T, true> pool(NEW_DEL_COUNT);

        for (int p = 0; p < PHASES; ++p)
        {
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                datas[c] = &pool.construct(std::forward<Args>(args)...);
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                pool.destroy(*datas[c]);
        }
    }
}

template <typename T, typename... Args>
void obj_pool_no_dtor(benchmark::State& state, Args&&... args)
{
    std::vector<T*> datas(NEW_DEL_COUNT);

    for (auto _ : state)
    {
        nb::ObjectPool<T, false> pool;

        for (int p = 0; p < PHASES; ++p)
        {
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
            {
                datas[c] = &pool.construct(std::forward<Args>(args)...);
                datas[c]->clear();
                if constexpr (std::is_same_v<T, std::string>)
                    *datas[c] = std::string_view(std::forward<Args>(args)...);
            }
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                pool.destroy(*datas[c]);
        }
    }
}

template <typename T, typename... Args>
void obj_pool_no_dtor_reserve(benchmark::State& state, Args&&... args)
{
    std::vector<T*> datas(NEW_DEL_COUNT);

    for (auto _ : state)
    {
        nb::ObjectPool<T, false> pool(NEW_DEL_COUNT);

        for (int p = 0; p < PHASES; ++p)
        {
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
            {
                datas[c] = &pool.construct(std::forward<Args>(args)...);
                datas[c]->clear();
                if constexpr (std::is_same_v<T, std::string>)
                    *datas[c] = std::string_view(std::forward<Args>(args)...);
            }
            for (int c = 0; c < NEW_DEL_COUNT; ++c)
                pool.destroy(*datas[c]);
        }
    }
}

BENCHMARK_TEMPLATE1_CAPTURE(new_delete, std::string, string_new_delete, QUICK_BROWN_FOX)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_call_dtor, std::string, string_obj_pool_call_dtor, QUICK_BROWN_FOX)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_call_dtor_reserve, std::string, string_obj_pool_call_dtor_reserve, QUICK_BROWN_FOX)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_no_dtor, std::string, string_obj_pool_no_dtor, QUICK_BROWN_FOX)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_no_dtor_reserve, std::string, string_obj_pool_no_dtor_reserve, QUICK_BROWN_FOX)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE1_CAPTURE(new_delete, nb::SerializeBuffer<>, serialize_buffer_new_delete, BUFFER_CAPACITY)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_call_dtor, nb::SerializeBuffer<>, serialize_buffer_obj_pool_call_dtor,
                            BUFFER_CAPACITY)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_call_dtor_reserve, nb::SerializeBuffer<>,
                            serialize_buffer_obj_pool_call_dtor_reserve, BUFFER_CAPACITY)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_no_dtor, nb::SerializeBuffer<>, serialize_buffer_obj_pool_no_dtor, BUFFER_CAPACITY)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(obj_pool_no_dtor_reserve, nb::SerializeBuffer<>, serialize_buffer_obj_pool_no_dtor_reserve,
                            BUFFER_CAPACITY)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
