#include "RingByteBuffer.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <source_location>
#include <vector>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cout << "Failed at phase #" << phase << "\n"; \
            const auto loc = std::source_location::current(); \
            std::cout << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            print_buffers(); \
            print_cmds(); \
            std::cout << std::flush; \
            std::exit(2); \
        } \
    } while (false)

namespace
{

constexpr int RING_SIZE = 16;

constexpr int PHASES = 10000;
constexpr int MAX_BYTES_PER_PHASE = 4096;

struct Command
{
    enum class Kind
    {
        READ,
        WRITE,
    };

    Kind kind;
    std::size_t size;
};

} // namespace

int main()
{
    unsigned seed = []() -> unsigned {
        std::random_device rd;
        return rd();
    }();

    std::cout << "seed=" << seed << "\n";

    std::mt19937 rng(seed);

    std::uniform_int_distribution<std::uint64_t> u64_dist;
    std::uniform_int_distribution yes_or_no(0, 1);

    std::vector<std::byte> buffer_input(MAX_BYTES_PER_PHASE);
    std::vector<std::byte> buffer_output(MAX_BYTES_PER_PHASE);
    std::size_t pos_input = 0;
    std::size_t pos_output = 0;

    auto print_buffers = [&] {
        std::cout << "buffer_input : " << std::hex;
        for (const std::byte b : buffer_input)
        {
            const auto num = static_cast<unsigned>(b);
            std::cout << std::setw(2) << std::setfill('0') << num << ' ';
        }
        std::cout << "\n";

        std::cout << "buffer_output: ";
        for (const std::byte b : buffer_output)
        {
            const auto num = static_cast<unsigned>(b);
            std::cout << std::setw(2) << std::setfill('0') << num << ' ';
        }
        std::cout << std::dec << "\n";
    };

    std::vector<Command> phase_cmds;
    phase_cmds.reserve(MAX_BYTES_PER_PHASE * 2);

    auto print_cmds = [&phase_cmds] {
        for (const auto& cmd : phase_cmds)
        {
            std::cout << (cmd.kind == Command::Kind::READ ? "read" : "write");
            std::cout << "(" << cmd.size << ")\n";
        }
    };

    nb::RingByteBuffer ring(RING_SIZE);

    auto ring_read = [&](int phase) {
        const auto used = ring.used_space();
        assert(used > 0);
        std::uniform_int_distribution<std::size_t> dist(1, std::min(used, buffer_output.size() - pos_output));

        Command cmd{Command::Kind::READ, dist(rng)};
        phase_cmds.push_back(cmd);

        const bool read_result = ring.try_read(buffer_output.data() + pos_output, cmd.size);
        TEST_ASSERT(read_result);

        pos_output += cmd.size;
    };

    auto ring_write = [&](int phase) {
        const auto available = ring.available_space();
        assert(available > 0);
        std::uniform_int_distribution<std::size_t> dist(1, std::min(available, buffer_input.size() - pos_input));

        Command cmd{Command::Kind::WRITE, dist(rng)};
        phase_cmds.push_back(cmd);

        const bool write_result = ring.try_write(buffer_input.data() + pos_input, cmd.size);
        TEST_ASSERT(write_result);

        pos_input += cmd.size;
    };

    for (int phase = 0; phase < PHASES; ++phase)
    {
        phase_cmds.clear();
        pos_input = 0;
        pos_output = 0;

        // fill `buffer_input` with random bytes
        static_assert(MAX_BYTES_PER_PHASE % 8 == 0);
        for (int i = 0; i < MAX_BYTES_PER_PHASE / 8; ++i)
        {
            auto& val = reinterpret_cast<std::uint64_t*>(buffer_input.data())[i];
            val = u64_dist(rng);
        }

        // fill `buffer_output` w/ going through `ring`
        while (pos_output < buffer_output.size())
        {
            if (ring.available_space() == 0 || pos_input == buffer_input.size())
                ring_read(phase);
            else if (ring.used_space() == 0)
                ring_write(phase);
            else
            {
                if (yes_or_no(rng))
                    ring_read(phase);
                else
                    ring_write(phase);
            }
        }

        // validate: [input == output]
        TEST_ASSERT(buffer_input == buffer_output);
    }

    std::cout << "All is well!" << std::endl;
}
