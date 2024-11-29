#pragma once

#include "NetBuff/RingQueue_fwd.hpp"

#include <cassert>
#include <cstddef>
#include <list>
#include <utility>

namespace nb
{

/// @brief Ring queue to store some `T` elements
///
/// If the reserved buffer is full, it DOESN'T increase its size automatically;
/// You need to reserve it manually via `try_resize_buffer()`.
template <typename T, typename Allocator>
class RingQueue : private std::allocator_traits<Allocator>::template rebind_alloc<std::byte>
{
public:
    using ByteAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;

public:
    RingQueue() : RingQueue(0)
    {
    }

    RingQueue(std::size_t capacity) : _capacity_plus_one(capacity + 1), _read_idx(0), _write_idx(0)
    {
        if (capacity)
        {
            _alloc_size = (capacity + 1) * sizeof(T) + (alignof(T) - 1);
            _alloc_addr = this->allocate(_alloc_size);
            _elements = reinterpret_cast<T*>(_alloc_addr);
            std::size_t space = _alloc_size;
            _elements = reinterpret_cast<T*>(
                std::align(alignof(T), sizeof(T) * _capacity_plus_one, reinterpret_cast<void*&>(_elements), space));
            assert(_elements);
        }
        else
        {
            _elements = nullptr;
            _alloc_addr = nullptr;
            _alloc_size = 0;
        }
    }

    RingQueue(RingQueue&& other) noexcept : RingQueue(0)
    {
        swap(other);
    }

    // Move and swap idiom
    RingQueue& operator=(RingQueue other) noexcept
    {
        swap(other);
        return *this;
    }

    RingQueue(const RingQueue&) = delete;

public:
    ~RingQueue()
    {
        if (_elements)
        {
            while (!empty())
                pop();

            assert(_alloc_addr);
            this->deallocate(_alloc_addr, _alloc_size);
        }
    }

public:
    /// @brief Try resizing the buffer.
    ///
    /// If requested capacity is not enough to store the existing elements in it, this function fails.
    ///
    /// If requested capacity is same or less than before, this function succeeds, but it's capacity remains the same.
    /// If you want to shrink the reserved space, use `shrink_to_fit()` instead.
    bool try_resize_buffer(std::size_t new_capacity)
    {
        if (new_capacity < size())
            return false;
        if (new_capacity <= capacity())
            return true;

        resize(new_capacity);
        return true;
    }

    void shrink_to_fit()
    {
        if (!full())
            resize(size());
    }

public: // Element access
    auto front() -> T&
    {
        return _elements[_read_idx];
    }

    auto front() const -> const T&
    {
        return _elements[_read_idx];
    }

    auto back() -> T&
    {
        return _elements[move_idx(_write_idx, -1)];
    }

    auto back() const -> const T&
    {
        return _elements[move_idx(_write_idx, -1)];
    }

public: // Capacity
    auto capacity() const -> std::size_t
    {
        return _capacity_plus_one - 1;
    }

    bool empty() const
    {
        return _read_idx == _write_idx;
    }

    bool full() const
    {
        return move_idx(_write_idx, +1) == _read_idx;
    }

    auto size() const -> std::size_t
    {
        return (_write_idx - _read_idx + _capacity_plus_one) % _capacity_plus_one;
    }

public: // Modifiers
    bool try_push(const T& value)
    {
        if (full())
            return false;

        ::new (static_cast<void*>(_elements + _write_idx)) T(value);
        _write_idx = move_idx(_write_idx, +1);

        return true;
    }

    bool try_push(T&& value)
    {
        if (full())
            return false;

        ::new (static_cast<void*>(_elements + _write_idx)) T(std::move(value));
        _write_idx = move_idx(_write_idx, +1);

        return true;
    }

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        if (full())
            return false;

        ::new (static_cast<void*>(_elements + _write_idx)) T(std::forward<Args>(args)...);
        _write_idx = move_idx(_write_idx, +1);

        return true;
    }

    void pop()
    {
        front().~T();
        _read_idx = move_idx(_read_idx, +1);
    }

    void swap(RingQueue& other) noexcept
    {
        using std::swap;

        if constexpr (std::allocator_traits<ByteAllocator>::propagate_on_container_swap::value)
            swap<ByteAllocator>(*this, other);

        swap(_elements, other._elements);
        swap(_capacity_plus_one, other._capacity_plus_one);
        swap(_read_idx, other._read_idx);
        swap(_write_idx, other._write_idx);
        swap(_alloc_addr, other._alloc_addr);
        swap(_alloc_size, other._alloc_size);
    }

private:
    void resize(std::size_t new_capacity)
    {
        if (0 == new_capacity)
        {
            assert(0 == size());

            // Deallocate old buffer if present
            if (_alloc_addr)
                this->deallocate(_alloc_addr, _alloc_size);

            _elements = nullptr;
            _capacity_plus_one = 1;
            _read_idx = 0;
            _write_idx = 0;
            _alloc_addr = nullptr;
            _alloc_size = 0;
        }
        else
        {
            // Allocate new buffer & Align it
            std::size_t new_alloc_size = (new_capacity + 1) * sizeof(T) + (alignof(T) - 1);
            std::byte* new_alloc_addr = this->allocate(new_alloc_size);
            T* new_elements = reinterpret_cast<T*>(new_alloc_addr);
            std::size_t space = new_alloc_size;
            new_elements = reinterpret_cast<T*>(
                std::align(alignof(T), sizeof(T) * (new_capacity + 1), reinterpret_cast<void*&>(new_elements), space));
            assert(new_elements);

            // Move the existing elements to new buffer
            const std::size_t elem_size = size();
            for (std::size_t new_idx = 0, old_idx = _read_idx; new_idx < elem_size;
                 ++new_idx, old_idx = move_idx(old_idx, +1))
                ::new (static_cast<void*>(new_elements + new_idx)) T(std::move_if_noexcept(_elements[old_idx]));

            // Set position infos
            _read_idx = 0;
            _write_idx = elem_size;

            // Deallocate old buffer if present
            if (_alloc_addr)
                this->deallocate(_alloc_addr, _alloc_size);

            // Set new buffer
            _elements = new_elements;
            _capacity_plus_one = new_capacity + 1;
            _alloc_addr = new_alloc_addr;
            _alloc_size = new_alloc_size;
        }
    }

private:
    auto move_idx(std::size_t idx, std::ptrdiff_t diff) const -> std::size_t
    {
        return idx = (idx + diff + _capacity_plus_one) % _capacity_plus_one;
    }

private:
    T* _elements;

    std::size_t _capacity_plus_one;
    std::size_t _read_idx;
    std::size_t _write_idx;

    std::byte* _alloc_addr; // unaligned allocation address to deallocate later
    std::size_t _alloc_size;
};

} // namespace nb
