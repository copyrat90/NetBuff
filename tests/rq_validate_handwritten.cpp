#include "NetBuff/RingQueue.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <source_location>

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

int main()
{
    nb::RingQueue<int> q1;
    // empty check
    TEST_ASSERT(q1.empty());
    TEST_ASSERT(q1.full());
    TEST_ASSERT(0 == q1.size());
    TEST_ASSERT(0 == q1.capacity());
    // can't push items to capacity == 0 queue
    TEST_ASSERT(!q1.try_push(1));
    TEST_ASSERT(!q1.try_push(2));
    TEST_ASSERT(q1.empty());
    TEST_ASSERT(q1.full());
    TEST_ASSERT(0 == q1.size());
    TEST_ASSERT(0 == q1.capacity());
    // move construct capacity == 0 queue
    nb::RingQueue<int> q2(std::move(q1));
    TEST_ASSERT(q1.empty());
    TEST_ASSERT(q1.full());
    TEST_ASSERT(0 == q1.size());
    TEST_ASSERT(0 == q1.capacity());
    TEST_ASSERT(q2.empty());
    TEST_ASSERT(q2.full());
    TEST_ASSERT(0 == q2.size());
    TEST_ASSERT(0 == q2.capacity());
    // resize capacity to 4
    TEST_ASSERT(q2.try_resize_buffer(4));
    TEST_ASSERT(q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(0 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    // push 3 items in capacity == 4 queue
    TEST_ASSERT(q2.try_push(1));
    TEST_ASSERT(q2.try_push(2));
    TEST_ASSERT(q2.try_push(3));
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(3 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    // make it full
    TEST_ASSERT(q2.try_push(4));
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(q2.full());
    TEST_ASSERT(4 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    // move the full queue to empty queue
    q1 = std::move(q2);
    TEST_ASSERT(!q1.empty());
    TEST_ASSERT(q1.full());
    TEST_ASSERT(4 == q1.size());
    TEST_ASSERT(4 == q1.capacity());
    TEST_ASSERT(q2.empty());
    TEST_ASSERT(q2.full());
    TEST_ASSERT(0 == q2.size());
    TEST_ASSERT(0 == q2.capacity());
    // resize requests test
    TEST_ASSERT(!q1.try_resize_buffer(3));
    TEST_ASSERT(4 == q1.capacity());
    TEST_ASSERT(q1.try_resize_buffer(5));
    TEST_ASSERT(5 == q1.capacity());
    TEST_ASSERT(q1.try_resize_buffer(4));
    TEST_ASSERT(5 == q1.capacity()); // not shrinked
    TEST_ASSERT(!q1.full());
    q1.shrink_to_fit();
    TEST_ASSERT(4 == q1.capacity()); // shrinked
    TEST_ASSERT(q1.full());
    TEST_ASSERT(q2.try_resize_buffer(0));
    TEST_ASSERT(0 == q2.capacity());
    // move the full queue to half full queue
    TEST_ASSERT(q2.try_resize_buffer(2));
    TEST_ASSERT(q2.try_push(1));
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(1 == q2.size());
    TEST_ASSERT(2 == q2.capacity());
    q2 = std::move(q1);
    TEST_ASSERT(q1.empty());
    TEST_ASSERT(q1.full());
    TEST_ASSERT(0 == q1.size());
    TEST_ASSERT(0 == q1.capacity());
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(q2.full());
    TEST_ASSERT(4 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    // pop the items
    TEST_ASSERT(1 == q2.front());
    q2.pop();
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(3 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    TEST_ASSERT(2 == q2.front());
    q2.pop();
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(2 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    TEST_ASSERT(3 == q2.front());
    q2.pop();
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(1 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    TEST_ASSERT(4 == q2.front());
    q2.pop();
    TEST_ASSERT(q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(0 == q2.size());
    TEST_ASSERT(4 == q2.capacity());
    // fresh queue with initial capacity 3
    nb::RingQueue<int> q3(3);
    TEST_ASSERT(q3.empty());
    TEST_ASSERT(!q3.full());
    TEST_ASSERT(0 == q3.size());
    TEST_ASSERT(3 == q3.capacity());
    TEST_ASSERT(q3.try_push(1));
    // swap test
    q2.swap(q3);
    TEST_ASSERT(!q2.empty());
    TEST_ASSERT(!q2.full());
    TEST_ASSERT(1 == q2.size());
    TEST_ASSERT(3 == q2.capacity());
    TEST_ASSERT(q3.empty());
    TEST_ASSERT(!q3.full());
    TEST_ASSERT(0 == q3.size());
    TEST_ASSERT(4 == q3.capacity());

    std::cout << "All is well!\n";
}
