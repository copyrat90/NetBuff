#pragma once

#include "LockfreeQueue_fwd.hpp"

#include "NetBuff/LockfreeObjectPool.hpp"
#include "NetBuff/TaggedPtr.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

namespace nb
{

template <typename T, typename Allocator>
    requires std::is_trivially_destructible_v<T> && std::is_trivially_copy_constructible_v<T>
class LockfreeQueue
{
private:
    struct Node
    {
    private:
        // workaround `alignof(Node)` not usable in `Node`
        static constexpr auto ALIGNMENT = std::max(alignof(T), alignof(std::atomic<std::uintptr_t>));

    public:
        std::atomic<TaggedPtrAligned<Node, ALIGNMENT>> next;

        alignas(T) std::byte data[sizeof(T)];

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

    static_assert(std::atomic<TaggedPtr<Node>>::is_always_lock_free);

public:
    /// @param capacity reserved capacity for internal object pool
    explicit LockfreeQueue(std::size_t capacity = 0) : _node_pool(capacity)
    {
        Node& dummy = _node_pool.construct();
        TaggedPtr<Node> tagged_dummy(&dummy);

        _head.store(tagged_dummy, std::memory_order_release);
        _tail.store(tagged_dummy, std::memory_order_release);
    }

    LockfreeQueue(const LockfreeQueue&) = delete;
    LockfreeQueue& operator=(const LockfreeQueue&) = delete;

public:
    ~LockfreeQueue()
    {
        while (pop())
            ;

        assert(_head.load() == _tail.load());
        // dealloc final dummy node
        _node_pool.destroy(*_head.load());
    }

public: // Capacity
    auto size() const noexcept -> std::size_t
    {
        return _size;
    }

public: // Modifiers
    auto push(const T& value) -> std::size_t
    {
        return emplace(value);
    }

    auto push(T&& value) -> std::size_t
    {
        return emplace(std::move(value));
    }

    template <typename... Args>
    auto emplace(Args&&... args) -> std::size_t
    {
        const auto size = ++_size;

        // alloc `adding_node` from pool
        Node& adding_node = _node_pool.construct();

        // destroy `adding_node` on fail
        struct AddingNodeDestroyerOnFail
        {
            decltype((_node_pool)) pool;
            Node* node;

            AddingNodeDestroyerOnFail(Node* node_, decltype((_node_pool)) pool_) : pool(pool_), node(node_) {};

            ~AddingNodeDestroyerOnFail()
            {
                if (node)
                    pool.destroy(*node);
            }
        } adding_node_destroyer(&adding_node, _node_pool);

        // construct `T` in `adding_node`
        ::new (static_cast<void*>(adding_node.data)) T(std::forward<Args>(args)...);

        // clear the `adding_node.next` to `nullptr`
        TaggedPtr<Node> old_node_next = adding_node.next.load(std::memory_order_acquire);
        TaggedPtr<Node> new_node_next(nullptr, old_node_next.get_tag());
        adding_node.next.store(new_node_next, std::memory_order_release);

        // try push loop
        for (;;)
        {
            TaggedPtr<Node> old_tail = _tail.load(std::memory_order_acquire);
            TaggedPtr<Node> old_tail_next = old_tail->next.load(std::memory_order_acquire);

            // check if `old_tail_next` got from `old_tail` is valid
            // via checking `old_tail` was current `_tail`
            const TaggedPtr<Node> old_tail_validate = _tail.load(std::memory_order_acquire);
            if (old_tail_validate != old_tail)
                continue;

            if (!old_tail_next)
            {
                TaggedPtr<Node> new_tail_next(&adding_node, old_tail_next.get_tag() + 1);
                if (old_tail->next.compare_exchange_weak(old_tail_next, new_tail_next, std::memory_order_release,
                                                         std::memory_order_relaxed))
                {
                    // move tail
                    TaggedPtr<Node> new_tail(&adding_node, old_tail.get_tag() + 1);
                    _tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release,
                                                  std::memory_order_relaxed);
                    break;
                }
            }
            else
            {
                // failed, but update tail to recent observed one
                TaggedPtr<Node> new_tail(old_tail_next.get_ptr(), old_tail.get_tag() + 1);
                _tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }
        }

        adding_node_destroyer.node = nullptr;
        return size;
    }

    auto pop() -> std::optional<T>
    {
        std::optional<T> result;

        for (;;)
        {
            TaggedPtr<Node> old_head = _head.load(std::memory_order_acquire);
            TaggedPtr<Node> old_tail = _tail.load(std::memory_order_acquire);
            TaggedPtr<Node> old_head_next = old_head->next.load(std::memory_order_acquire);

            // check if `old_head_next` got from `old_head` is valid
            // via checking `old_head` was still current `_head`
            const TaggedPtr<Node> old_head_validate = _head.load(std::memory_order_acquire);
            if (old_head_validate != old_head)
                continue;

            // queue was empty / tail was not updated yet on `push()`
            if (old_head == old_tail)
            {
                // queue was empty
                if (!old_head_next)
                {
                    result.reset();
                    break;
                }

                // tail was not updated yet on `push()`
                else
                {
                    // update to recent observed one
                    TaggedPtr<Node> new_tail(old_head_next.get_ptr(), old_tail.get_tag() + 1);
                    _tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release,
                                                  std::memory_order_relaxed);
                    continue;
                }
            }
            // non-dummy node found in the queue
            else
            {
                // data is in `old_head_next`;
                // it should be extracted before CAS to prevent reuse before extracting.
                result = old_head_next->obj(); // can't move, CAS might fail

                // can't destroy `obj` anywhere, so `T` requires to be trivially destructible
                // old_head_next->obj().~T();

                TaggedPtr<Node> new_head(old_head_next.get_ptr(), old_head.get_tag() + 1);
                // try moving new head
                if (_head.compare_exchange_weak(old_head, new_head, std::memory_order_release,
                                                std::memory_order_relaxed))
                {
                    --_size;
                    // dealloc `old_head`
                    _node_pool.destroy(*old_head);
                    break;
                }
                else
                {
                    continue;
                }
            }
        }

        return result;
    }

private:
    LockfreeObjectPool<Node, false, Allocator> _node_pool;

    std::atomic<TaggedPtr<Node>> _head;
    std::atomic<TaggedPtr<Node>> _tail;

    std::atomic<std::size_t> _size = 0;

    static_assert(decltype(_size)::is_always_lock_free);
};

} // namespace nb
