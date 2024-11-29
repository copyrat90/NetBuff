#pragma once

#include <cstddef>
#include <memory>

namespace nb
{
template <typename T, typename Allocator = std::allocator<T>>
class RingQueue;
}
