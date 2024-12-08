#pragma once

#include <memory>

namespace nb
{

/// @brief Auto-increasing lock-free object pool.
///
/// This allocates many "block"s of memory for `T`s.
/// The size of a new block is increased each time a new one is allocated. (2x)
///
/// It is only lock-free if there's unused node available.
/// (i.e. When allocating a new block, it still locks.)
///
/// @tparam CallDestructorOnDestroy if this is `true`,
/// calls destructor on every `destroy()`, and calls constructor on every `construct()`.
template <typename T, bool CallDestructorOnDestroy, typename Allocator = std::allocator<T>>
class LockfreeObjectPool;

} // namespace nb
