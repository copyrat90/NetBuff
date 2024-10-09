#pragma once

#include <memory>

namespace nb
{

/// @brief Auto-increasing object pool.
///
/// This allocates many "block"s of memory for `T`s.
/// The size of a new block is increased each time a new one is allocated. (2x)
///
/// @tparam CallDestructorOnDestroy if this is `true`,
/// calls destructor on every `destroy()`, and calls constructor on every `construct()`.
template <typename T, bool CallDestructorOnDestroy, typename Allocator = std::allocator<T>>
class ObjectPool;

} // namespace nb
