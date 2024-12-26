#pragma once

#include <memory>

namespace nb
{

/// @brief Auto-increasing lock-free object pool, which uses thread-local secondary pool to minimize contention.
///
/// This allocates many "block"s for internal nodes,
/// and pass those blocks to the thread-local secondary pool when it's empty.
///
/// When secondary pool's all nodes are used,
/// it requests for another block from the primary pool.
///
/// When secondary pool's some nodes are returned so that a block is fully free (not a single node is used),
/// it returns the block to the primary pool.
///
/// @tparam CallDestructorOnDestroy if this is `true`,
/// calls destructor on every `destroy()`, and calls constructor on every `construct()`.
template <typename T, bool CallDestructorOnDestroy, typename Allocator = std::allocator<T>>
class TlsObjectPool;

} // namespace nb
