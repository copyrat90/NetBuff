#pragma once

#include "NetBuff/SpscRingByteBuffer_fwd.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

namespace nb
{

/// @brief Single-producer, single-consumer ring buffer to store some bytes.
///
/// If the buffer is full, it DOESN'T increase its size automatically;
/// You need to resize it manually via `try_resize()`.
template <typename ByteAllocator>
class SpscRingByteBuffer : private ByteAllocator
{
    static_assert(std::is_same_v<std::byte, typename ByteAllocator::value_type>);

public:
    SpscRingByteBuffer() : SpscRingByteBuffer(0)
    {
    }

    SpscRingByteBuffer(std::size_t effective_capacity)
        : _buffer(effective_capacity == 0 ? nullptr : this->allocate(effective_capacity + 1)),
          _capacity(effective_capacity + 1), _pos_read(0), _pos_write(0)
    {
    }

    SpscRingByteBuffer(SpscRingByteBuffer&& other) noexcept
        : ByteAllocator(std::move(other)), _buffer(other._buffer), _capacity(other._capacity),
          _pos_read(other._pos_read.load(std::memory_order_relaxed)),
          _pos_write(other._pos_write.load(std::memory_order_relaxed))
    {
        other._buffer = nullptr;
        other._capacity = 1;
        other._pos_read.store(0, std::memory_order_relaxed);
        other._pos_write.store(0, std::memory_order_relaxed);
    }

    SpscRingByteBuffer& operator=(SpscRingByteBuffer&& other) noexcept
    {
        ByteAllocator::operator=(std::move(other));

        _buffer = other._buffer;
        _capacity = other._capacity;
        _pos_read.store(other._pos_read.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _pos_write.store(other._pos_write.load(std::memory_order_relaxed), std::memory_order_relaxed);

        other._buffer = nullptr;
        other._capacity = 1;
        other._pos_read.store(0, std::memory_order_relaxed);
        other._pos_write.store(0, std::memory_order_relaxed);

        return *this;
    }

    SpscRingByteBuffer(const SpscRingByteBuffer&) = delete;
    SpscRingByteBuffer& operator=(const SpscRingByteBuffer&) = delete;

public:
    ~SpscRingByteBuffer()
    {
        if (_buffer)
            this->deallocate(_buffer, _capacity);
    }

public:
    // (Producer only)
    bool try_write(const void* data, std::size_t length)
    {
        if (length > available_write())
            return false;

        const std::size_t consecutive_len = consecutive_write_length();
        // 1-phase copy
        if (length <= consecutive_len)
        {
            std::memcpy(_buffer + _pos_write.load(std::memory_order_relaxed), data, length);
        }
        // 2-phase copy
        else
        {
            const std::size_t len_1 = consecutive_len;
            const std::size_t len_2 = length - consecutive_len;

            std::memcpy(_buffer + _pos_write.load(std::memory_order_relaxed), data, len_1);
            std::memcpy(_buffer, static_cast<const std::byte*>(data) + len_1, len_2);
        }

        move_write_pos(length);
        return true;
    }

    // (Consumer only)
    bool try_read(void* dest, std::size_t length)
    {
        const bool result = try_peek(dest, length);
        if (result)
            move_read_pos(length);

        return result;
    }

    // (Consumer only)
    bool try_peek(void* dest, std::size_t length) const
    {
        if (length > available_read())
            return false;

        const std::size_t consecutive_len = consecutive_read_length();
        // 1-phase copy
        if (length <= consecutive_len)
        {
            std::memcpy(dest, _buffer + _pos_read.load(std::memory_order_relaxed), length);
        }
        // 2-phase copy
        else
        {
            const std::size_t len_1 = consecutive_len;
            const std::size_t len_2 = length - consecutive_len;

            std::memcpy(dest, _buffer + _pos_read.load(std::memory_order_relaxed), len_1);
            std::memcpy(static_cast<std::byte*>(dest) + len_1, _buffer, len_2);
        }

        return true;
    }

public:
    // (Single-thread only)
    void clear()
    {
        _pos_read.store(0, std::memory_order_relaxed);
        _pos_write.store(0, std::memory_order_relaxed);
    }

    /// @brief (Single-thread only) Try resizing the buffer.
    ///
    /// If requested capacity is not enough to store the existing data in it, this function fails.
    /// If requested capacity is same as before, this function fails.
    ///
    /// @return Whether the resize took place or not
    bool try_resize(std::size_t new_effective_capacity)
    {
        const std::size_t used = available_read();

        if (new_effective_capacity < used || new_effective_capacity == _capacity - 1)
            return false;

        std::byte* new_buffer = (new_effective_capacity == 0) ? nullptr : this->allocate(new_effective_capacity + 1);

        if (new_buffer && available_read() > 0)
        {
            const std::size_t consecutive_len = consecutive_read_length();
            // 1-phase copy
            if (used == consecutive_len)
            {
                std::memcpy(new_buffer, _buffer + _pos_read.load(std::memory_order_relaxed), used);
            }
            // 2-phase copy
            else
            {
                assert(used > consecutive_len);

                const std::size_t len_1 = consecutive_len;
                const std::size_t len_2 = used - consecutive_len;

                std::memcpy(new_buffer, _buffer + _pos_read.load(std::memory_order_relaxed), len_1);
                std::memcpy(new_buffer + len_1, _buffer, len_2);
            }
        }

        _pos_read.store(0, std::memory_order_relaxed);
        _pos_write.store(used, std::memory_order_relaxed);

        if (_buffer)
            this->deallocate(_buffer, _capacity);
        _buffer = new_buffer;
        _capacity = new_effective_capacity + 1;

        return true;
    }

public:
    auto effective_capacity() const -> std::size_t
    {
        return _capacity - 1;
    }

    auto capacity() const -> std::size_t
    {
        return _capacity;
    }

public:
    /// @brief (Consumer only) How many bytes you can read before empty
    auto available_read() const -> std::size_t
    {
        return (_capacity + _pos_write.load(std::memory_order_acquire) - _pos_read.load(std::memory_order_relaxed)) %
               _capacity;
    }

    /// @brief (Producer only) How many bytes you can write before full
    auto available_write() const -> std::size_t
    {
        return effective_capacity() -
               (_capacity + _pos_write.load(std::memory_order_relaxed) - _pos_read.load(std::memory_order_acquire)) %
                   _capacity;
    }

public:
    auto data() -> std::byte*
    {
        return _buffer;
    }

    auto data() const -> const std::byte*
    {
        return _buffer;
    }

    // (Producer only)
    auto consecutive_write_length() const -> std::size_t
    {
        return std::min(_capacity - _pos_write.load(std::memory_order_relaxed), available_write());
    }

    // (Consumer only)
    auto consecutive_read_length() const -> std::size_t
    {
        return std::min(_capacity - _pos_read.load(std::memory_order_relaxed), available_read());
    }

    // (Consumer only)
    auto read_pos() const -> std::size_t
    {
        return _pos_read.load(std::memory_order_relaxed);
    }

    // (Producer only)
    auto write_pos() const -> std::size_t
    {
        return _pos_write.load(std::memory_order_relaxed);
    }

    // (Consumer only) No checks performed - Use with caution!
    void move_read_pos(std::ptrdiff_t diff)
    {
        _pos_read.store((_pos_read.load(std::memory_order_relaxed) + diff + _capacity) % _capacity,
                        std::memory_order_release);
    }

    // (Producer only) No checks performed - Use with caution!
    void move_write_pos(std::ptrdiff_t diff)
    {
        _pos_write.store((_pos_write.load(std::memory_order_relaxed) + diff + _capacity) % _capacity,
                         std::memory_order_release);
    }

public:
    // (Monitoring only)
    auto monitor_used_space() const -> std::size_t
    {
        const auto pos_read = _pos_read.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        const auto pos_write = _pos_write.load(std::memory_order_relaxed);

        return (_capacity - pos_read + pos_write) % _capacity;
    }

    // (Monitoring only)
    auto monitor_available_space() const -> std::size_t
    {
        return effective_capacity() - monitor_used_space();
    }

private:
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

    std::byte* _buffer;
    std::size_t _capacity;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> _pos_read;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> _pos_write;
};

} // namespace nb
