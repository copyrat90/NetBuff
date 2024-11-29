#include "NetBuff/RingQueue.hpp"

#include <cstdint>
#include <iostream>
#include <random>
#include <source_location>
#include <vector>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            const auto loc = std::source_location::current(); \
            std::cout << "Failed " << #condition << " at phase #" << phase << "\n\t"; \
            std::cout << "at " << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            print_cmds(); \
            std::cout << std::flush; \
            std::exit(2); \
        } \
    } while (false)

namespace
{

constexpr int QUEUE_CAPACITY = 16;

constexpr int PHASES = 10000;
constexpr int ELEMENTS_PER_PHASE = 4096;

struct Command
{
    enum class Kind
    {
        READ,
        WRITE,
    };

    Kind kind;
    std::uint64_t value;
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

    std::vector<std::uint64_t> data_input(ELEMENTS_PER_PHASE);
    std::vector<std::uint64_t> data_output(ELEMENTS_PER_PHASE);
    std::size_t pos_input = 0;
    std::size_t pos_output = 0;

    std::vector<Command> phase_cmds;
    phase_cmds.reserve(ELEMENTS_PER_PHASE * 2);

    auto print_cmds = [&phase_cmds] {
        for (const auto& cmd : phase_cmds)
        {
            std::cout << (cmd.kind == Command::Kind::READ ? "read" : "write");
            std::cout << ": " << cmd.value << "\n";
        }
    };

    nb::RingQueue<std::uint64_t> q(QUEUE_CAPACITY);

    auto q_read = [&] {
        assert(!q.empty());

        Command cmd{Command::Kind::READ, q.front()};
        phase_cmds.push_back(cmd);

        data_output[pos_output++] = q.front();
        q.pop();
    };

    auto q_write = [&](int phase) {
        assert(!q.full());

        const auto value = data_input[pos_input];

        Command cmd{Command::Kind::WRITE, value};
        phase_cmds.push_back(cmd);

        const bool write_result = q.try_push(data_input[pos_input++]);
        TEST_ASSERT(write_result);
    };

    for (int phase = 0; phase < PHASES; ++phase)
    {
        phase_cmds.clear();
        pos_input = 0;
        pos_output = 0;

        // fill `data_input` with random numbers
        for (auto& data : data_input)
            data = u64_dist(rng);

        // fill `data_output` w/ going through `q`
        while (pos_output < data_output.size())
        {
            if (q.full() || pos_input == data_input.size())
                q_read();
            else if (q.empty())
                q_write(phase);
            else
            {
                if (yes_or_no(rng))
                    q_read();
                else
                    q_write(phase);
            }
        }

        // validate: [input == output]
        TEST_ASSERT(data_input == data_output);
    }

    std::cout << "All is well!" << std::endl;
}
