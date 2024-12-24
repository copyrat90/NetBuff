#pragma once

#include <memory>
#include <type_traits>

namespace nb
{

/// @brief Michael-Scott concurrent queue.
///
/// This uses lockfree object pool to manage internal nodes.
template <typename T, typename Allocator = std::allocator<T>>
    requires std::is_trivially_destructible_v<T> && std::is_trivially_copy_constructible_v<T>
class LockfreeQueue;

}
