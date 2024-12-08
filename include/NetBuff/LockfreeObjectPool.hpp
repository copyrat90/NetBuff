#pragma once

#include "NetBuff/LockfreeObjectPool_fwd.hpp"

#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

#ifndef NB_VA_BITS
#define NB_VA_BITS 56
#endif

#ifndef NB_OBJ_POOL_CHECK
#define NB_OBJ_POOL_CHECK true
#endif

#if NB_OBJ_POOL_CHECK
#include <ostream>
#endif

namespace nb
{

template <typename T, bool CallDestructorOnDestroy>
class LockfreeObjectPoolTraits
{
protected:
    struct Node
    {
    public:
        // `next` and `data` can't share address, since there's no destructor call on `destroy()`
        Node* next;
#if NB_OBJ_POOL_CHECK
        LockfreeObjectPoolTraits* pool;
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
class LockfreeObjectPoolTraits<T, true>
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
        LockfreeObjectPoolTraits* pool;
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

template <typename T, bool CallDestructorOnDestroy, typename Allocator>
class LockfreeObjectPool final : private LockfreeObjectPoolTraits<T, CallDestructorOnDestroy>,
                                 private std::allocator_traits<Allocator>::template rebind_alloc<std::byte>
{
private:
    // parent types (empty base optimization)
    using Traits = LockfreeObjectPoolTraits<T, CallDestructorOnDestroy>;
    using ByteAllocator = std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;

    using Node = typename Traits::Node;

private:
    struct Block
    {
        Block* next;
        std::byte* alloc_addr; // unaligned allocation address to deallocate later

        std::size_t count; // number of `Node`s in this block

        // Node nodes[count];
    };

    static constexpr std::size_t INIT_BLOCK_NODE_COUNT = 16;
    static_assert(INIT_BLOCK_NODE_COUNT > 0);

private:
    class TaggedNodePtr
    {
    private:
        std::uintptr_t _tagged_addr;

    public:
        static_assert(std::has_single_bit(alignof(Node)), "Not power of two alignment for `Node`");
        static_assert(sizeof(_tagged_addr) == 8, "LockfreeObjectPool only supports 64-bit architecture");
        static_assert(8 <= NB_VA_BITS && NB_VA_BITS <= 64, "Invalid `NB_VA_BITS`");

    public:
        static constexpr std::size_t UPPER_TAG_BITS = 64 - NB_VA_BITS;
        static constexpr std::uintptr_t UPPER_TAG_MASK = ((std::uintptr_t(1) << UPPER_TAG_BITS) - 1) << NB_VA_BITS;

        static constexpr std::size_t LOWER_TAG_BITS = std::countr_zero(alignof(Node));
        static constexpr std::uintptr_t LOWER_TAG_MASK = alignof(Node) - 1;

        static_assert(!(UPPER_TAG_MASK & LOWER_TAG_MASK), "Tag masks overlap; Possibly invalid `NB_VA_BITS`");
        static_assert(NB_VA_BITS >= LOWER_TAG_BITS,
                      "`NB_VA_BITS` is smaller than `Node` alignment; Possibly invalid `NB_VA_BITS`");

        static constexpr std::uintptr_t TAG_MASK = UPPER_TAG_MASK | LOWER_TAG_MASK;

    public:
        explicit TaggedNodePtr(Node* node) : _tagged_addr(reinterpret_cast<decltype(_tagged_addr)>(node))
        {
            // `node` should hold aligned address value
            assert(!(_tagged_addr & LOWER_TAG_MASK));

            if (_tagged_addr & UPPER_TAG_MASK)
                throw std::logic_error(
                    std::format("Invalid node address `0x{:016x}` when `NB_VA_BITS` is {}", _tagged_addr, NB_VA_BITS));
        }

        TaggedNodePtr() : _tagged_addr(0)
        {
        }

    public:
        auto get_node_ptr() const noexcept -> Node*
        {
            return reinterpret_cast<Node*>(_tagged_addr & ~TAG_MASK);
        }

        explicit operator Node*() const noexcept
        {
            return get_node_ptr();
        }

        auto operator*() const noexcept -> Node&
        {
            return *get_node_ptr();
        }

        auto operator->() const noexcept -> Node*
        {
            return get_node_ptr();
        }

        operator bool() const noexcept
        {
            return get_node_ptr();
        }

    public:
        auto get_tag() const noexcept -> std::uintptr_t
        {
            return ((_tagged_addr & UPPER_TAG_MASK) >> (NB_VA_BITS - LOWER_TAG_BITS)) | (_tagged_addr & LOWER_TAG_MASK);
        }

        void set_tag(std::uintptr_t tag_value) noexcept
        {
            const std::uintptr_t upper = (tag_value & (UPPER_TAG_MASK >> (NB_VA_BITS - LOWER_TAG_BITS)))
                                         << (NB_VA_BITS - LOWER_TAG_BITS);
            const std::uintptr_t lower = tag_value & LOWER_TAG_MASK;

            _tagged_addr = (_tagged_addr & ~TAG_MASK) | (upper | lower);
        }

        void increase_tag() noexcept
        {
            set_tag(get_tag() + 1);
        }
    };

public:
    LockfreeObjectPool() : LockfreeObjectPool(0)
    {
    }

    /// @param capacity number of `T`s to reserve space
    LockfreeObjectPool(std::size_t capacity)
        : _next_block_node_count(capacity != 0 ? capacity : INIT_BLOCK_NODE_COUNT), _capacity(0), _used_nodes(0)
    {
        if (capacity != 0)
            add_new_block();
    }

    LockfreeObjectPool(const LockfreeObjectPool&) = delete;
    LockfreeObjectPool& operator=(const LockfreeObjectPool&) = delete;

    LockfreeObjectPool(LockfreeObjectPool&&) = delete;
    LockfreeObjectPool& operator=(LockfreeObjectPool&&) = delete;

public:
    ~LockfreeObjectPool()
    {
#if NB_OBJ_POOL_CHECK
        if (_err)
        {
            if (used_slots() > 0)
                (*_err) << std::format("[LEAK] {} nodes are not returned to `LockfreeObjectPool` at {}\n", used_slots(),
                                       reinterpret_cast<std::uintptr_t>(this));
        }
#endif

        if constexpr (!CallDestructorOnDestroy)
        {
            // Destroy all `T`s in nodes
            Node* node = _node_head.load().get_node_ptr();
            while (node)
            {
                if (node->constructed)
                    node->obj().~T();

                node = node->next;
            }
        }

        while (_block_head)
        {
            Block* const next = _block_head->next;
            this->deallocate(_block_head->alloc_addr, calc_raw_block_size(_block_head->count));
            _block_head = next;
        }
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
        TaggedNodePtr cur = _node_head.load();
        for (;;)
        {
            // if there's no unused node available, allocate a new block and try getting one again
            while (!cur)
            {
                add_new_block();
                cur = _node_head.load();
            }

            // if got the candidate `cur` node, prepare `next` node
            TaggedNodePtr next(cur->next);
            next.set_tag(cur.get_tag() + 1); // prevent ABA problem w/ increasing tag

            // try exchanging `_node_head` to `next`, and break if succeeds
            if (_node_head.compare_exchange_weak(cur, next))
                break;
        }

        ++_used_nodes;

        if constexpr (CallDestructorOnDestroy)
        {
            ::new (static_cast<void*>(cur->data)) T(std::forward<Args>(args)...);
        }
        else
        {
            if (!cur->constructed)
            {
                ::new (static_cast<void*>(cur->data)) T(std::forward<Args>(args)...);
                cur->constructed = true;
            }
        }

        return cur->obj();
    }

    /// @brief Destroy `obj` in object pool.
    ///
    /// If `CallDestructorOnDestroy` is `false`, the destructor is not called until the object pool is destroyed.
    void destroy(T& obj)
    {
        Node& node = *reinterpret_cast<Node*>(reinterpret_cast<std::byte*>(&obj) - offsetof(Node, data));

#if NB_OBJ_POOL_CHECK
        if (node.pool != this)
            throw std::logic_error(
                std::format("nb::LockfreeObjectPool::destroy(obj) called with `obj` that's not in object pool at {}",
                            reinterpret_cast<std::uintptr_t>(this)));
#endif

        if constexpr (CallDestructorOnDestroy)
            obj.~T();

        TaggedNodePtr old_head = _node_head.load();
        TaggedNodePtr new_head(&node);
        for (;;)
        {
            // prepare `new_head`
            node.next = old_head.get_node_ptr();
            new_head.set_tag(old_head.get_tag());

            // try exchanging `_node_head` to `new_head`, and break if succeeds
            if (_node_head.compare_exchange_weak(old_head, new_head))
                break;
        }

        --_used_nodes;
    }

public:
    /// @return number of total slots that can store `T`
    auto capacity() const -> std::size_t
    {
        return _capacity;
    }

    /// @return number of used slots that can store `T`
    auto used_slots() const -> std::size_t
    {
        return _used_nodes;
    }

    /// @return number of unused slots that can store `T`
    auto unused_slots() const -> std::size_t
    {
        return _capacity - _used_nodes;
    }

#if NB_OBJ_POOL_CHECK
public:
    void set_err_stream(std::ostream* err)
    {
        _err = err;
    }
#endif

private:
    void add_new_block()
    {
        std::lock_guard<std::mutex> block_guard(_block_mutex);

        // double check if new block allocation is still required
        if (!_node_head.load())
        {
            // allocate a raw block (byte buffer)
            const std::size_t raw_block_size = calc_raw_block_size(_next_block_node_count);
            std::byte* raw_block = this->allocate(raw_block_size);

            // align the `Block` (header)
            std::byte* aligned_block = raw_block;
            std::size_t block_header_space = raw_block_size;
            aligned_block = reinterpret_cast<std::byte*>(
                std::align(alignof(Block), sizeof(Block), reinterpret_cast<void*&>(aligned_block), block_header_space));
            assert(aligned_block);
            Block* block = reinterpret_cast<Block*>(aligned_block);

            // set up the new block
            block->alloc_addr = raw_block;
            block->count = _next_block_node_count;

            // connect the new block to the linked list
            block->next = _block_head;
            _block_head = block;

            // set starting address of raw nodes
            std::byte* raw_nodes = aligned_block + sizeof(Block);
            const std::size_t raw_nodes_size = raw_block_size - (raw_nodes - raw_block);

            // align the `Node`s
            std::byte* aligned_nodes = raw_nodes;
            std::size_t nodes_space = raw_nodes_size;
            aligned_nodes =
                reinterpret_cast<std::byte*>(std::align(alignof(Node), _next_block_node_count * sizeof(Node),
                                                        reinterpret_cast<void*&>(aligned_nodes), nodes_space));
            assert(aligned_nodes);
            Node* nodes = reinterpret_cast<Node*>(aligned_nodes);

            // set up & connect the new nodes
            for (std::size_t i = 0; i < _next_block_node_count - 1; ++i)
            {
                nodes[i].next = nodes + i + 1;
#if NB_OBJ_POOL_CHECK
                nodes[i].pool = this;
#endif
                if constexpr (!CallDestructorOnDestroy)
                    nodes[i].constructed = false;
            }

            // connect the last node to the linked list
            auto& last_node = nodes[_next_block_node_count - 1];
#if NB_OBJ_POOL_CHECK
            last_node.pool = this;
#endif
            if constexpr (!CallDestructorOnDestroy)
                last_node.constructed = false;
            TaggedNodePtr old_head = _node_head.load();
            TaggedNodePtr new_head(nodes);
            for (;;)
            {
                // prepare `new_head`
                last_node.next = old_head.get_node_ptr();
                new_head.set_tag(old_head.get_tag());

                // try exchanging `_node_head` to `new_head`, and break if succeeds
                if (_node_head.compare_exchange_weak(old_head, new_head))
                    break;
            }

            // adjust internal sizes
            _capacity += _next_block_node_count;
            _next_block_node_count = _capacity;
        }
    }

    auto calc_raw_block_size(std::size_t node_count) const noexcept -> std::size_t
    {
        return (alignof(Block) - 1)         // max block offset for alignment
               + sizeof(Block)              // actual block size
               + (alignof(Node) - 1)        // max node offset for alignment
               + node_count * sizeof(Node); // actual nodes size
    }

private:
    std::atomic<TaggedNodePtr> _node_head;

    std::mutex _block_mutex;

    Block* _block_head = nullptr;

#if NB_OBJ_POOL_CHECK
    std::ostream* _err = nullptr;
#endif

    std::size_t _next_block_node_count;

    std::atomic<std::size_t> _capacity; // total nodes count
    std::atomic<std::size_t> _used_nodes;

    static_assert(std::atomic<TaggedNodePtr>::is_always_lock_free);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
};

} // namespace nb
