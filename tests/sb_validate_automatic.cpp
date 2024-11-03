#include "NetBuff/SerializeBuffer.hpp"

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

constexpr std::size_t MAX_RW_SIZE_PER_OP = 64;

constexpr int PHASES = 100000;

struct Input
{
    enum class Kind
    {
        INT,
        FLOAT,
        BYTES,
        STRING,

        MAX_COUNT
    };

    enum class StrKind
    {
        NONE = 0,

        STRING,
        WSTRING,
        U8STRING,
        U16STRING,
        U32STRING,

        STRING_VIEW,
        WSTRING_VIEW,
        U8STRING_VIEW,
        U16STRING_VIEW,
        U32STRING_VIEW,

        CHAR_PTR,
        WCHAR_T_PTR,
        CHAR8_T_PTR,
        CHAR16_T_PTR,
        CHAR32_T_PTR,

        BEGIN = STRING,
        END = CHAR32_T_PTR,
    };

    Kind kind;
    std::size_t size;
    std::size_t pos;

    StrKind str_kind;
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

    std::uniform_int_distribution rand_byte_exclude_zero_dist(1, 255);
    std::uniform_int_distribution input_kind_dist(0, (int)Input::Kind::MAX_COUNT - 1);

    std::vector<std::byte> buffer_input(BUF_SIZE);
    std::vector<std::byte> buffer_output(BUF_SIZE);
    std::size_t pos = 0;

    std::vector<Input> phase_inputs;
    phase_inputs.reserve(BUF_SIZE);

    // we should put sizeof string length too, so we need more space than `BUF_SIZE`
    nb::SerializeBuffer buf(8 * BUF_SIZE);

    auto buf_write_int = [&]() {
        std::uniform_int_distribution size_dist(0, 3);
        const auto size = (std::size_t(1) << size_dist(rng));

        switch (size)
        {
        case 1: {
            std::int8_t data = *reinterpret_cast<std::int8_t*>(buffer_input.data() + pos);
            buf << data;
            break;
        }
        case 2: {
            std::int16_t data = *reinterpret_cast<std::int16_t*>(buffer_input.data() + pos);
            buf << data;
            break;
        }
        case 4: {
            std::int32_t data = *reinterpret_cast<std::int32_t*>(buffer_input.data() + pos);
            buf << data;
            break;
        }
        case 8: {
            std::int64_t data = *reinterpret_cast<std::int64_t*>(buffer_input.data() + pos);
            buf << data;
            break;
        }
        default:
            throw std::logic_error("shouldn't reach here");
        }

        if (buf.fail())
            throw std::logic_error("should be writable");

        phase_inputs.push_back(Input{
            .kind = Input::Kind::INT,
            .size = size,
            .pos = pos,
            .str_kind = Input::StrKind::NONE,
        });

        pos += size;
    };

    auto buf_write_float = [&]() {
        std::uniform_int_distribution is_double_dist(0, 1);
        const bool is_double = is_double_dist(rng);

        if (is_double)
        {
            double data = *reinterpret_cast<double*>(buffer_input.data() + pos);
            buf << data;
        }
        else
        {
            float data = *reinterpret_cast<float*>(buffer_input.data() + pos);
            buf << data;
        }

        if (!buf)
            throw std::logic_error("should be writable");

        phase_inputs.push_back(Input{
            .kind = Input::Kind::FLOAT,
            .size = (is_double ? 8ULL : 4ULL),
            .pos = pos,
            .str_kind = Input::StrKind::NONE,
        });

        pos += phase_inputs.back().size;
    };

    auto buf_write_str = [&]() {
        std::uniform_int_distribution<std::size_t> dist(
            1, std::min((buffer_input.size() - pos) / 4, MAX_RW_SIZE_PER_OP / 4));
        const auto length = dist(rng);

        Input input{
            .kind = Input::Kind::STRING,
            .size = 0,
            .pos = pos,
            .str_kind = (Input::StrKind)std::uniform_int_distribution((int)Input::StrKind::BEGIN,
                                                                      (int)Input::StrKind::END)(rng),
        };

        alignas(4) std::byte temp[MAX_RW_SIZE_PER_OP + 4];

        switch (input.str_kind)
        {
        case Input::StrKind::STRING: {
            std::string data(reinterpret_cast<char*>(buffer_input.data() + pos), length);
            buf << data;
            input.size = length * sizeof(char);
            break;
        }
        case Input::StrKind::WSTRING: {
            const std::wstring data(reinterpret_cast<wchar_t*>(buffer_input.data() + pos), length);
            buf << data;
            input.size = length * sizeof(wchar_t);
            break;
        }
        case Input::StrKind::U8STRING: {
            const std::u8string data(reinterpret_cast<char8_t*>(buffer_input.data() + pos), length);
            buf << data;
            input.size = length * sizeof(char8_t);
            break;
        }
        case Input::StrKind::U16STRING: {
            const std::u16string data(reinterpret_cast<char16_t*>(buffer_input.data() + pos), length);
            buf << data;
            input.size = length * sizeof(char16_t);
            break;
        }
        case Input::StrKind::U32STRING: {
            const std::u32string data(reinterpret_cast<char32_t*>(buffer_input.data() + pos), length);
            buf << data;
            input.size = length * sizeof(char32_t);
            break;
        }
        case Input::StrKind::STRING_VIEW: {
            const auto* data = reinterpret_cast<char*>(buffer_input.data() + pos);
            buf << std::string_view(data, length);
            input.size = length * sizeof(char);
            break;
        }
        case Input::StrKind::WSTRING_VIEW: {
            const auto* data = reinterpret_cast<wchar_t*>(buffer_input.data() + pos);
            buf << std::wstring_view(data, length);
            input.size = length * sizeof(wchar_t);
            break;
        }
        case Input::StrKind::U8STRING_VIEW: {
            const auto* data = reinterpret_cast<char8_t*>(buffer_input.data() + pos);
            buf << std::u8string_view(data, length);
            input.size = length * sizeof(char8_t);
            break;
        }
        case Input::StrKind::U16STRING_VIEW: {
            const auto* data = reinterpret_cast<char16_t*>(buffer_input.data() + pos);
            buf << std::u16string_view(data, length);
            input.size = length * sizeof(char16_t);
            break;
        }
        case Input::StrKind::U32STRING_VIEW: {
            const auto* data = reinterpret_cast<char32_t*>(buffer_input.data() + pos);
            buf << std::u32string_view(data, length);
            input.size = length * sizeof(char32_t);
            break;
        }
        case Input::StrKind::CHAR_PTR: {
            input.size = length * sizeof(char);
            auto* data = reinterpret_cast<char*>(temp);
            std::memcpy(data, buffer_input.data() + pos, input.size);
            data[length] = '\0';
            buf << data;
            break;
        }
        case Input::StrKind::WCHAR_T_PTR: {
            input.size = length * sizeof(wchar_t);
            auto* data = reinterpret_cast<wchar_t*>(temp);
            std::memcpy(data, buffer_input.data() + pos, input.size);
            data[length] = L'\0';
            buf << data;
            break;
        }
        case Input::StrKind::CHAR8_T_PTR: {
            input.size = length * sizeof(char8_t);
            auto* data = reinterpret_cast<char8_t*>(temp);
            std::memcpy(data, buffer_input.data() + pos, input.size);
            data[length] = u8'\0';
            buf << data;
            break;
        }
        case Input::StrKind::CHAR16_T_PTR: {
            input.size = length * sizeof(char16_t);
            auto* data = reinterpret_cast<char16_t*>(temp);
            std::memcpy(data, buffer_input.data() + pos, input.size);
            data[length] = u'\0';
            buf << data;
            break;
        }
        case Input::StrKind::CHAR32_T_PTR: {
            input.size = length * sizeof(char32_t);
            auto* data = reinterpret_cast<char32_t*>(temp);
            std::memcpy(data, buffer_input.data() + pos, input.size);
            data[length] = U'\0';
            buf << data;
            break;
        }
        default:
            throw std::logic_error("shouldn't reach here");
        }

        if (!buf)
            throw std::logic_error("should be writable");

        phase_inputs.push_back(input);

        pos += input.size;
    };

    auto buf_write_bytes = [&]() {
        std::uniform_int_distribution<std::size_t> dist(1, std::min(buffer_input.size() - pos, MAX_RW_SIZE_PER_OP));
        const auto length = dist(rng);

        const bool result = buf.try_write(buffer_input.data() + pos, length);
        if (!result)
            throw std::logic_error("should be writable");

        phase_inputs.push_back(Input{
            .kind = Input::Kind::BYTES,
            .size = length,
            .pos = pos,
            .str_kind = Input::StrKind::NONE,
        });

        pos += length;
    };

    for (int phase = 0; phase < PHASES; ++phase)
    {
        phase_inputs.clear();
        buf.clear();
        pos = 0;

        // fill `buffer_input` with random bytes (excluding `0`, since it messes up null-terminated strings)
        static_assert(BUF_SIZE % 8 == 0);
        for (int i = 0; i < BUF_SIZE; ++i)
        {
            auto& val = reinterpret_cast<std::uint8_t*>(buffer_input.data())[i];
            val = static_cast<std::uint8_t>(rand_byte_exclude_zero_dist(rng));
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
                case Input::Kind::STRING:
                    buf_write_str();
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
            alignas(4) std::byte temp[MAX_RW_SIZE_PER_OP + 4];

            switch (input.kind)
            {
            case Input::Kind::INT:
                switch (input.size)
                {
                case 1: {
                    auto& data = *reinterpret_cast<std::int8_t*>(buffer_output.data() + pos);
                    buf >> data;
                    break;
                }
                case 2: {
                    auto& data = *reinterpret_cast<std::int16_t*>(buffer_output.data() + pos);
                    buf >> data;
                    break;
                }
                case 4: {
                    auto& data = *reinterpret_cast<std::int32_t*>(buffer_output.data() + pos);
                    buf >> data;
                    break;
                }
                case 8: {
                    auto& data = *reinterpret_cast<std::int64_t*>(buffer_output.data() + pos);
                    buf >> data;
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
                    buf >> data;
                    break;
                }
                case 8: {
                    auto& data = *reinterpret_cast<double*>(buffer_output.data() + pos);
                    buf >> data;
                    break;
                }
                default:
                    throw std::logic_error("shouldn't reach here");
                }
                break;
            case Input::Kind::BYTES:
                buf.try_read(buffer_output.data() + pos, input.size);
                break;
            case Input::Kind::STRING:
                switch (input.str_kind)
                {
                case Input::StrKind::STRING:
                case Input::StrKind::STRING_VIEW: {
                    std::string str;
                    buf >> str;
                    assert(str.length() * sizeof(decltype(str)::value_type) == input.size);
                    std::memcpy(buffer_output.data() + pos, str.data(), input.size);
                }
                break;
                case Input::StrKind::WSTRING:
                case Input::StrKind::WSTRING_VIEW: {
                    std::wstring str;
                    buf >> str;
                    assert(str.length() * sizeof(decltype(str)::value_type) == input.size);
                    std::memcpy(buffer_output.data() + pos, str.data(), input.size);
                    break;
                }
                case Input::StrKind::U8STRING:
                case Input::StrKind::U8STRING_VIEW: {
                    std::u8string str;
                    buf >> str;
                    assert(str.length() * sizeof(decltype(str)::value_type) == input.size);
                    std::memcpy(buffer_output.data() + pos, str.data(), input.size);
                    break;
                }
                case Input::StrKind::U16STRING:
                case Input::StrKind::U16STRING_VIEW: {
                    std::u16string str;
                    buf >> str;
                    assert(str.length() * sizeof(decltype(str)::value_type) == input.size);
                    std::memcpy(buffer_output.data() + pos, str.data(), input.size);
                    break;
                }
                case Input::StrKind::U32STRING:
                case Input::StrKind::U32STRING_VIEW: {
                    std::u32string str;
                    buf >> str;
                    assert(str.length() * sizeof(decltype(str)::value_type) == input.size);
                    std::memcpy(buffer_output.data() + pos, str.data(), input.size);
                    break;
                }
                case Input::StrKind::CHAR_PTR: {
                    auto* z_str = reinterpret_cast<char*>(temp);
                    buf >> z_str;
                    std::memcpy(buffer_output.data() + pos, z_str, input.size);
                    break;
                }
                case Input::StrKind::WCHAR_T_PTR: {
                    auto* z_str = reinterpret_cast<wchar_t*>(temp);
                    buf >> z_str;
                    std::memcpy(buffer_output.data() + pos, z_str, input.size);
                    break; // TODO
                }
                case Input::StrKind::CHAR8_T_PTR: {
                    auto* z_str = reinterpret_cast<char8_t*>(temp);
                    buf >> z_str;
                    std::memcpy(buffer_output.data() + pos, z_str, input.size);
                    break; // TODO
                }
                case Input::StrKind::CHAR16_T_PTR: {
                    auto* z_str = reinterpret_cast<char16_t*>(temp);
                    buf >> z_str;
                    std::memcpy(buffer_output.data() + pos, z_str, input.size);
                    break; // TODO
                }
                case Input::StrKind::CHAR32_T_PTR: {
                    auto* z_str = reinterpret_cast<char32_t*>(temp);
                    buf >> z_str;
                    std::memcpy(buffer_output.data() + pos, z_str, input.size);
                    break; // TODO
                }
                default:
                    throw std::logic_error("shouldn't reach here");
                }
                break;
            default:
                throw std::logic_error("shouldn't reach here");
            }

            if (buf.fail())
                throw std::logic_error("should be readable");

            pos += input.size;
        }

        if (pos != buffer_output.size())
            throw std::logic_error("output position mismatch");

        // validate: everything was read
        TEST_ASSERT(!buf.fail());
        TEST_ASSERT(buf.empty());

        // validate: [input == output]
        TEST_ASSERT(buffer_input == buffer_output);
    }

    std::cout << "All is well!" << std::endl;
}
