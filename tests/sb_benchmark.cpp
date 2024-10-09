#include <benchmark/benchmark.h>

#include "NetBuff/SerializeBuffer.hpp"

#include <SFML/Network/Packet.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{

constexpr int PHASES = 100;
constexpr int RW_COUNT = 100000;

constexpr int BUF_SIZE = 4096;

} // namespace

template <typename SB, typename... Args>
void sb_read_after_write(benchmark::State& state, Args&&... args)
{
    std::int8_t i8 = 0;
    std::uint8_t u8 = 0;
    std::int16_t i16 = 0;
    std::uint16_t u16 = 0;
    std::int32_t i32 = 0;
    std::uint32_t u32 = 0;
    std::int64_t i64 = 0;
    std::uint64_t u64 = 0;
    float f = 0;
    double d = 0;
    char c_str[] = {"The quick brown fox jumps over the lazy dog!"};
    std::string str = "The quick brown fox jumps over the lazy dog!";
    wchar_t c_wstr[] = {L"The quick brown fox jumps over the lazy dog!"};
    std::wstring wstr = L"The quick brown fox jumps over the lazy dog!";

    for (auto _ : state)
    {
        for (int p = 0; p < PHASES; ++p)
        {
            SB sb(std::forward<Args>(args)...);

            for (int c = 0; c < RW_COUNT; ++c)
            {
                sb.clear();
                sb << i8 << u8 << i16 << u16 << i32 << u32 << i64 << u64 << f << d << c_str << str << c_wstr << wstr;
                sb >> i8 >> u8 >> i16 >> u16 >> i32 >> u32 >> i64 >> u64 >> f >> d >> c_str >> str >> c_wstr >> wstr;
            }
        }
    }
}

BENCHMARK_TEMPLATE1(sb_read_after_write, sf::Packet)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE1_CAPTURE(sb_read_after_write, nb::SerializeBuffer<>, nb_serialize_buffer_rw, BUF_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
