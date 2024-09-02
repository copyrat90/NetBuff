#include "RingByteBuffer.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <utility>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            const auto loc = std::source_location::current(); \
            std::cout << "Failed: " << #condition << "\n\t"; \
            std::cout << "at " << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            std::exit(1); \
        } \
    } while (false)

static constexpr std::array<std::byte, 5> HELLO = {
    std::byte('h'), std::byte('e'), std::byte('l'), std::byte('l'), std::byte('o'),
};

int main()
{
    nb::RingByteBuffer ring;

    std::byte temp = std::byte(0);
    std::array<std::byte, 5> temp_5;

    TEST_ASSERT(!ring.try_resize(0));
    TEST_ASSERT(!ring.data());

    TEST_ASSERT(ring.try_resize(1));
    TEST_ASSERT(ring.data());
    TEST_ASSERT(ring.try_write(HELLO.data(), 1));
    TEST_ASSERT(!ring.try_resize(0));

    TEST_ASSERT(!ring.try_read(&temp, 2));
    TEST_ASSERT(temp == std::byte(0));
    TEST_ASSERT(ring.try_read(&temp, 1));
    TEST_ASSERT(temp == HELLO[0]);

    TEST_ASSERT(ring.try_write(HELLO.data() + 1, 1));
    TEST_ASSERT(ring.try_resize(2));
    TEST_ASSERT(ring.try_read(&temp, 1));
    TEST_ASSERT(temp == HELLO[1]);
    TEST_ASSERT(ring.try_resize(0));
    TEST_ASSERT(ring.empty());
    TEST_ASSERT(!ring.data());
    TEST_ASSERT(ring.available_space() == 0);

    TEST_ASSERT(ring.try_resize(sizeof(HELLO)));
    TEST_ASSERT(ring.try_write(HELLO.data(), sizeof(HELLO)));
    TEST_ASSERT(ring.try_read(temp_5.data(), sizeof(temp_5)));
    TEST_ASSERT(temp_5 == HELLO);
    TEST_ASSERT(ring.try_write(HELLO.data(), sizeof(HELLO)));
    TEST_ASSERT(ring.consecutive_read_length() == 1);
    TEST_ASSERT(ring.consecutive_write_length() == 0);
    TEST_ASSERT(ring.try_read(temp_5.data(), 3));
    TEST_ASSERT(ring.consecutive_read_length() == 2);
    TEST_ASSERT(ring.consecutive_write_length() == 2);
    TEST_ASSERT(ring.available_space() == 3);
    TEST_ASSERT(ring.used_space() == 2);
    TEST_ASSERT(ring.try_read(temp_5.data() + 3, 2));
    TEST_ASSERT(temp_5 == HELLO);
    TEST_ASSERT(ring.empty());

    TEST_ASSERT(ring.try_write(HELLO.data(), sizeof(HELLO)));
    nb::RingByteBuffer new_ring(std::move(ring));
    TEST_ASSERT(ring.empty());
    TEST_ASSERT(ring.effective_capacity() == 0);
    TEST_ASSERT(new_ring.used_space() == 5);
    TEST_ASSERT(new_ring.effective_capacity() == 5);
    TEST_ASSERT(new_ring.try_read(temp_5.data(), sizeof(temp_5)));
    TEST_ASSERT(temp_5 == HELLO);
    TEST_ASSERT(new_ring.empty());

    std::cout << "All is well!" << std::endl;
}
