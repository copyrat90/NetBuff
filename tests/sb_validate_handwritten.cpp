#include "NetBuff/SerializeBuffer.hpp"

#include <cstdint>
#include <iostream>
#include <source_location>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cout << "Failed " << #condition << " at " << std::endl; \
            const auto loc = std::source_location::current(); \
            std::cout << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            std::exit(2); \
        } \
    } while (false)

int main()
{
    std::uint8_t data_8 = 8;
    std::uint16_t data_16 = 16;

    nb::SerializeBuffer<> buf;

    TEST_ASSERT(buf.empty());
    TEST_ASSERT(!(buf << data_8));
    buf.clear();
    TEST_ASSERT(buf.try_resize(3));
    TEST_ASSERT(0 == buf.used_space());
    TEST_ASSERT(3 == buf.capacity());
    TEST_ASSERT(buf << data_8);
    TEST_ASSERT(buf << data_16);
    TEST_ASSERT(buf >> data_8);
    TEST_ASSERT(8 == data_8);
    TEST_ASSERT(2 == buf.used_space());
    TEST_ASSERT(buf.try_resize(2)); // succeeds, but does not shrink to fit
    TEST_ASSERT(3 == buf.capacity());
    buf.shrink_to_fit();
    TEST_ASSERT(2 == buf.capacity());
    TEST_ASSERT(buf >> data_16);
    TEST_ASSERT(16 == data_16);
    TEST_ASSERT(buf.empty());
    buf.shrink_to_fit();
    TEST_ASSERT(0 == buf.capacity());

    std::cout << "All is well!" << std::endl;
}
