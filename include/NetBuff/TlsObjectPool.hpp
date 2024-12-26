#pragma once

#include "NetBuff/TlsObjectPool_fwd.hpp"

#include "NetBuff/TaggedPtr.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

#ifndef NB_OBJ_POOL_CHECK
#define NB_OBJ_POOL_CHECK true
#endif

#if NB_OBJ_POOL_CHECK
#include <format>
#include <ostream>
#endif

namespace nb
{

template <typename T, bool CallDestructorOnDestroy>
class TlsObjectPoolNodeTrait
{
protected:
    struct Node
    {
    public:
        // `next` and `data` can't share address, since there's no destructor call on `destroy()`
        Node* next;
#if NB_OBJ_POOL_CHECK
        TlsObjectPoolNodeTrait* pool;
#endif
        alignas(T) std::byte data[sizeof(T)];
        bool constructed; // whether the `obj` is alive or not

    public:
        auto obj() -> T&
        {
            return reinterpret_cast<T&>(data);
        }

        auto obj() const -> const T&
        {
            return reinterpret_cast<const T&>(data);
        }
    };
};

template <typename T>
class TlsObjectPoolNodeTrait<T, true>
{
protected:
    struct Node
    {
    public:
        // `next` and `data` can share address, since destructor is called on `destroy()`
        union {
            Node* next;
            alignas(T) std::byte data[sizeof(T)];
        };
#if NB_OBJ_POOL_CHECK
        TlsObjectPoolNodeTrait* pool;
#endif

    public:
        auto obj() -> T&
        {
            return reinterpret_cast<T&>(data);
        }

        auto obj() const -> const T&
        {
            return reinterpret_cast<const T&>(data);
        }
    };
};

template <typename T, bool CallDestructorOnDestroy>
class TlsObjectPoolTraits : public TlsObjectPoolNodeTrait<T, CallDestructorOnDestroy>
{
protected:
    using Node = typename TlsObjectPoolNodeTrait<T, CallDestructorOnDestroy>::Node;

    struct Block
    {
        Node* node_head = nullptr;
        Block* block_next = nullptr;
        std::size_t node_count = 0;
    };
};

template <typename T, bool CallDestructorOnDestroy, typename Allocator>
class TlsObjectPool final : private TlsObjectPoolTraits<T, CallDestructorOnDestroy>,
                            private std::allocator_traits<Allocator>::template rebind_alloc<
                                typename TlsObjectPoolTraits<T, CallDestructorOnDestroy>::Node>, // NodeAllocator
                            private std::allocator_traits<Allocator>::template rebind_alloc<
                                typename TlsObjectPoolTraits<T, CallDestructorOnDestroy>::Block> // BlockAllocator
{
private:
    using Traits = TlsObjectPoolTraits<T, CallDestructorOnDestroy>;
    using Node = typename Traits::Node;
    using Block = typename Traits::Block;

    // parent type for empty base optimization
    using NodeAllocator = std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using BlockAllocator = std::allocator_traits<Allocator>::template rebind_alloc<Block>;

private:
    static constexpr std::size_t DEFAULT_BLOCK_SIZE = 16;
    static constexpr std::size_t DEFAULT_INITIAL_BLOCKS_COUNT = 0;

private:
    class InternalThreadLocalPool
    {
    public:
        ~InternalThreadLocalPool()
        {
            auto& primary_pool = TlsObjectPool::instance();

            Block* block;
            while ((block = top()))
            {

                pop();
            }
        }

    public:
        auto top_block() const -> Block*
        {
            return _block_head;
        }

        void push_block(Block& block)
        {
            block.block_next = _block_head;
            _block_head = &block;
        }

        void pop_block()
        {
            _block_head = _block_head->block_next;
        }

    private:
        Block* _block_head = nullptr;
    };

public:
    /// @brief Get singleton instance.
    ///
    /// Parameters are used only upon initialization, and after that they are always ignored.
    ///
    /// @param block_size number of nodes stored in a block
    /// @param initial_blocks_count number of blocks allocated upon initialization
    /// @return
    static auto instance(std::size_t block_size = DEFAULT_BLOCK_SIZE,
                         std::size_t initial_blocks_count = DEFAULT_INITIAL_BLOCKS_COUNT) -> TlsObjectPool&
    {
        static TlsObjectPool inst(block_size, initial_blocks_count);
        return inst;
    }

private:
    static auto internal_pool_instance() -> InternalThreadLocalPool&
    {
        thread_local InternalThreadLocalPool inst;
        return inst;
    }

private:
    TlsObjectPool(std::size_t block_size, std::size_t initial_blocks_count) : _block_size(block_size)
    {
        if (0 == block_size)
            throw std::invalid_argument("block size can't be zero");

        for (std::size_t i = 0; i < initial_blocks_count; ++i)
            add_new_block();
    }

    TlsObjectPool(const TlsObjectPool&) = delete;
    TlsObjectPool& operator=(const TlsObjectPool&) = delete;

private:
    ~TlsObjectPool()
    {
#if NB_OBJ_POOL_CHECK
        if (_err)
        {
            if (used_blocks() > 0)
                (*_err) << std::format("[LEAK] {} nodes are not returned to `ObjectPool` at {}\n", used_blocks(),
                                       reinterpret_cast<std::uintptr_t>(this));
        }
#endif

        // TODO
    }

public:
    /// @brief Construct `T` object in object pool.
    ///
    /// When `CallDestructorOnDestroy` is `false`, returned object might be already constructed long time ago.
    /// (i.e. `args` might be ignored)
    /// So, you might need a `T`'s member function to clear its states.
    template <typename... Args>
    [[nodiscard]] auto construct(Args&&... args) -> T&
    {
        // TODO
    }

    /// @brief Destroy `obj` in object pool.
    ///
    /// If `CallDestructorOnDestroy` is `false`, the destructor is not called until the object pool is destroyed.
    void destroy(T& obj)
    {
        // TODO
    }

public:
    /// @return number of nodes stored in a block
    auto block_size() const -> std::size_t
    {
        return _block_size;
    }

    /// @return number of total blocks that can store `Node`s
    auto monitor_total_blocks() const -> std::size_t
    {
        return _total_blocks_monitor.load(std::memory_order_relaxed);
    }

    /// @return number of used blocks that can store `Node`s
    auto monitor_used_blocks() const -> std::size_t
    {
        return _used_blocks_monitor.load(std::memory_order_relaxed);
    }

    /// @return number of unused blocks that can store `Node`s
    auto monitor_unused_blocks() const -> std::size_t
    {
        return _total_blocks_monitor.load(std::memory_order_relaxed) -
               _used_blocks_monitor.load(std::memory_order_relaxed);
    }

#if NB_OBJ_POOL_CHECK
public:
    void set_err_stream(std::ostream* err)
    {
        _err = err;
    }
#endif

private:
    void push_block(Block& block)
    {
        TaggedPtr<Block> old_head = _block_head.load(std::memory_order_relaxed);
        TaggedPtr<Block> new_head(&block);
        for (;;)
        {
            block.block_next = old_head.get_ptr();
            new_head.set_tag(old_head.get_tag);

            if (_block_head.compare_exchange_weak(old_head, new_head, std::memory_order_release,
                                                  std::memory_order_relaxed))
                break;
        }

        _used_blocks_monitor.fetch_sub(1, std::memory_order_relaxed);
    }

    auto pop_block() -> Block&
    {
        TaggedPtr<Block> old_head = _block_head.load(std::memory_order_acquire);
        for (;;)
        {
            // if there's no unused block available, allocate a new block & return it
            if (!old_head)
            {
                Block* new_block = BlockAllocator::allocate(1);
                ::new (static_cast<void*>(new_block)) Block;

                _total_blocks.fetch_add(1, std::memory_order_relaxed);
                _used_blocks.fetch_add(1, std::memory_order_relaxed);

                return *new_block;
            }

            // if got the candidate `old_head`, prepare `next_head`
            TaggedPtr<Node> new_head(old_head->block_next,
                                     old_head.get_tag() + 1); // prevent ABA problem w/ increasing tag

            // try exchanging `_block_head` to `next`, and break if succeeds
            if (_block_head.compare_exchange_weak(old_head, new_head, std::memory_order_acq_rel,
                                                  std::memory_order_acquire))
                break;
        }

        _used_blocks_monitor.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::atomic<TaggedPtr<Block>> _block_head;

    const std::size_t _block_size;

    std::atomic<std::size_t> _total_blocks_monitor;
    std::atomic<std::size_t> _used_blocks_monitor;

#if NB_OBJ_POOL_CHECK
    std::ostream* _err = nullptr;
#endif
};

} // namespace nb
