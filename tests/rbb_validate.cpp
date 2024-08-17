#include "RingByteBuffer.hpp"

#include <cassert>
#include <iostream>

int main()
{
    rbb::RingByteBuffer ring(16);

    for (int i = 0; i < 5; ++i)
    {
        const bool result = ring.try_write(&i, sizeof(i));
        assert(i == 4 ? !result : result);
    }

    for (int i = 0; i < 5; ++i)
    {
        int data;
        const bool result = ring.try_read(&data, sizeof(data));
        if (i == 4)
            assert(!result);
        else
            assert(i == data);
    }

    for (int i = 0; i < 5; ++i)
    {
        const bool result = ring.try_write(&i, sizeof(i));
        assert(i == 4 ? !result : result);
    }

    for (int i = 0; i < 5; ++i)
    {
        int data;
        const bool result = ring.try_read(&data, sizeof(data));
        if (i == 4)
            assert(!result);
        else
            assert(i == data);
    }

    std::cout << "All is well!" << std::endl;
}
