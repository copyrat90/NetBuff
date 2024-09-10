#pragma once

#include <cstddef>
#include <memory>

namespace nb
{
template <typename ByteAllocator = std::allocator<std::byte>>
class SerializeBuffer;
}
