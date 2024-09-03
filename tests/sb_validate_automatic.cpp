#include "SerializeBuffer.hpp"

#include <iostream>
#include <random>
#include <source_location>
#include <stdexcept>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cout << "Failed at phase #" << phase << std::endl; \
            const auto loc = std::source_location::current(); \
            std::cout << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            std::exit(2); \
        } \
    } while (false)

namespace
{

constexpr int BUF_SIZE = 4096;

constexpr int PHASES = 100000;

struct Input
{
    enum class Kind
    {
        INT,
        FLOAT,
        BYTES,

        MAX_COUNT
    };

    Kind kind;
    std::size_t size;
    std::size_t pos;
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
    std::uniform_int_distribution input_kind_dist(0, (int)Input::Kind::MAX_COUNT - 1);

    std::vector<std::byte> buffer_input(BUF_SIZE);
    std::vector<std::byte> buffer_output(BUF_SIZE);
    std::size_t pos = 0;

    std::vector<Input> phase_inputs;
    phase_inputs.reserve(BUF_SIZE);

    nb::SerializeBuffer buf(BUF_SIZE);

    auto buf_write_int = [&]() {
        std::uniform_int_distribution size_dist(0, 3);
        const auto size = (1ULL << size_dist(rng));
        bool result;

        switch (size)
        {
        case 1: {
            std::int8_t data = *reinterpret_cast<std::int8_t*>(buffer_input.data() + pos);
            result = buf.try_write(data);
            break;
        }
        case 2: {
            std::int16_t data = *reinterpret_cast<std::int16_t*>(buffer_input.data() + pos);
            result = buf.try_write(data);
            break;
        }
        case 4: {
            std::int32_t data = *reinterpret_cast<std::int32_t*>(buffer_input.data() + pos);
            result = buf.try_write(data);
            break;
        }
        case 8: {
            std::int64_t data = *reinterpret_cast<std::int64_t*>(buffer_input.data() + pos);
            result = buf.try_write(data);
            break;
        }
        default:
            throw std::logic_error("shouldn't reach here");
        }

        if (!result)
            throw std::logic_error("should be writable");

        phase_inputs.push_back(Input{
            .kind = Input::Kind::INT,
            .size = size,
            .pos = pos,
        });

        pos += size;
    };

    auto buf_write_float = [&]() {
        std::uniform_int_distribution is_double_dist(0, 1);
        const bool is_double = is_double_dist(rng);
        bool result;

        if (is_double)
        {
            double data = *reinterpret_cast<double*>(buffer_input.data() + pos);
            result = buf.try_write(data);
        }
        else
        {
            float data = *reinterpret_cast<float*>(buffer_input.data() + pos);
            result = buf.try_write(data);
        }

        if (!result)
            throw std::logic_error("should be writable");

        phase_inputs.push_back(Input{
            .kind = Input::Kind::FLOAT,
            .size = (is_double ? 8ULL : 4ULL),
            .pos = pos,
        });

        pos += phase_inputs.back().size;
    };

    auto buf_write_bytes = [&]() {
        std::uniform_int_distribution<std::size_t> dist(1, buffer_input.size() - pos);
        const auto length = dist(rng);

        const bool result = buf.try_write(buffer_input.data() + pos, length);
        if (!result)
            throw std::logic_error("should be writable");

        phase_inputs.push_back(Input{
            .kind = Input::Kind::BYTES,
            .size = length,
            .pos = pos,
        });

        pos += length;
    };

    for (int phase = 0; phase < PHASES; ++phase)
    {
        phase_inputs.clear();
        buf.clear();
        pos = 0;

        // fill `buffer_input` with random bytes
        static_assert(BUF_SIZE % 8 == 0);
        for (int i = 0; i < BUF_SIZE / 8; ++i)
        {
            auto& val = reinterpret_cast<std::uint64_t*>(buffer_input.data())[i];
            val = u64_dist(rng);
        }

        // `buffer_input` -> `buf`
        while (pos < buffer_input.size())
        {
            if (buffer_input.size() - pos < 8)
                buf_write_bytes();
            else
            {
                switch ((Input::Kind)input_kind_dist(rng))
                {
                case Input::Kind::INT:
                    buf_write_int();
                    break;
                case Input::Kind::FLOAT:
                    buf_write_float();
                    break;
                case Input::Kind::BYTES:
                    buf_write_bytes();
                    break;
                default:
                    throw std::logic_error("shouldn't reach here");
                }
            }
        }

        if (pos != buffer_input.size())
            throw std::logic_error("input position mismatch");

        pos = 0;

        // `buf` -> `buffer_output`
        for (const auto& input : phase_inputs)
        {
            bool result;

            switch (input.kind)
            {
            case Input::Kind::INT:
                switch (input.size)
                {
                case 1: {
                    auto& data = *reinterpret_cast<std::int8_t*>(buffer_output.data() + pos);
                    result = buf.try_read(data);
                    break;
                }
                case 2: {
                    auto& data = *reinterpret_cast<std::int16_t*>(buffer_output.data() + pos);
                    result = buf.try_read(data);
                    break;
                }
                case 4: {
                    auto& data = *reinterpret_cast<std::int32_t*>(buffer_output.data() + pos);
                    result = buf.try_read(data);
                    break;
                }
                case 8: {
                    auto& data = *reinterpret_cast<std::int64_t*>(buffer_output.data() + pos);
                    result = buf.try_read(data);
                    break;
                }
                default:
                    throw std::logic_error("shouldn't reach here");
                }
                break;
            case Input::Kind::FLOAT:
                switch (input.size)
                {
                case 4: {
                    auto& data = *reinterpret_cast<float*>(buffer_output.data() + pos);
                    result = buf.try_read(data);
                    break;
                }
                case 8: {
                    auto& data = *reinterpret_cast<double*>(buffer_output.data() + pos);
                    result = buf.try_read(data);
                    break;
                }
                default:
                    throw std::logic_error("shouldn't reach here");
                }
                break;
            case Input::Kind::BYTES: {
                result = buf.try_read(buffer_output.data() + pos, input.size);
            }
            break;
            default:
                throw std::logic_error("shouldn't reach here");
            }

            if (!result)
                throw std::logic_error("should be readable");

            pos += input.size;
        }

        if (pos != buffer_output.size())
            throw std::logic_error("output position mismatch");

        // validate: [input == output]
        TEST_ASSERT(buffer_input == buffer_output);
    }

    std::cout << "All is well!" << std::endl;
}
