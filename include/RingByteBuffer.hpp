#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

namespace nb
{

/// @brief Ring buffer to store some bytes.
///
/// If the buffer is full, it DOESN'T increase its size automatically;
/// You need to resize it manually via `try_resize()`.
template <typename ByteAllocator = std::allocator<std::byte>>
class RingByteBuffer : private ByteAllocator
{
    static_assert(std::is_same_v<std::byte, typename ByteAllocator::value_type>);

public:
    RingByteBuffer() : RingByteBuffer(0)
    {
    }

    RingByteBuffer(std::size_t effective_capacity)
        : _buffer(effective_capacity == 0 ? nullptr : this->allocate(effective_capacity + 1)),
          _capacity(effective_capacity + 1), _pos_read(0), _pos_write(0)
    {
    }

    RingByteBuffer(RingByteBuffer&& other) noexcept
        : ByteAllocator(std::move(other)), _buffer(other._buffer), _capacity(other._capacity),
          _pos_read(other._pos_read), _pos_write(other._pos_write)
    {
        other._buffer = nullptr;
        other._capacity = 1;
        other._pos_read = 0;
        other._pos_write = 0;
    }

    RingByteBuffer& operator=(RingByteBuffer&& other) noexcept
    {
        ByteAllocator::operator=(std::move(other));

        _buffer = other._buffer;
        _capacity = other._capacity;
        _pos_read = other._pos_read;
        _pos_write = other._pos_write;

        other._buffer = nullptr;
        other._capacity = 1;
        other._pos_read = 0;
        other._pos_write = 0;

        return *this;
    }

    RingByteBuffer(const RingByteBuffer&) = delete;
    RingByteBuffer& operator=(const RingByteBuffer&) = delete;

public:
    ~RingByteBuffer()
    {
        if (_buffer)
            this->deallocate(_buffer, _capacity);
    }

public:
    bool try_write(const void* data, std::size_t length)
    {
        if (length > available_space())
            return false;

        const std::size_t consecutive_len = consecutive_write_length();
        // 1-phase copy
        if (length <= consecutive_len)
        {
            std::memcpy(_buffer + _pos_write, data, length);
        }
        // 2-phase copy
        else
        {
            const std::size_t len_1 = consecutive_len;
            const std::size_t len_2 = length - consecutive_len;

            std::memcpy(_buffer + _pos_write, data, len_1);
            std::memcpy(_buffer, static_cast<const std::byte*>(data) + len_1, len_2);
        }

        move_write_pos(length);
        return true;
    }

    bool try_read(void* dest, std::size_t length)
    {
        const bool result = try_peek(dest, length);
        if (result)
            move_read_pos(length);

        return result;
    }

    bool try_peek(void* dest, std::size_t length)
    {
        if (length > used_space())
            return false;

        const std::size_t consecutive_len = consecutive_read_length();
        // 1-phase copy
        if (length <= consecutive_len)
        {
            std::memcpy(dest, _buffer + _pos_read, length);
        }
        // 2-phase copy
        else
        {
            const std::size_t len_1 = consecutive_len;
            const std::size_t len_2 = length - consecutive_len;

            std::memcpy(dest, _buffer + _pos_read, len_1);
            std::memcpy(static_cast<std::byte*>(dest) + len_1, _buffer, len_2);
        }

        return true;
    }

public:
    void clear()
    {
        _pos_read = 0;
        _pos_write = 0;
    }

    /// @brief Try resizing the buffer.
    ///
    /// If requested capacity is not enough to store the existing data in it, this function fails.
    /// If requested capacity is same as before, this function fails.
    ///
    /// @return Whether the resize took place or not
    bool try_resize(std::size_t new_effective_capacity)
    {
        const std::size_t used = used_space();

        if (new_effective_capacity < used || new_effective_capacity == _capacity - 1)
            return false;

        std::byte* new_buffer = (new_effective_capacity == 0) ? nullptr : this->allocate(new_effective_capacity + 1);

        if (new_buffer && !empty())
        {
            const std::size_t consecutive_len = consecutive_read_length();
            // 1-phase copy
            if (used == consecutive_len)
            {
                std::memcpy(new_buffer, _buffer + _pos_read, used);
            }
            // 2-phase copy
            else
            {
                assert(used > consecutive_len);

                const std::size_t len_1 = consecutive_len;
                const std::size_t len_2 = used - consecutive_len;

                std::memcpy(new_buffer, _buffer + _pos_read, len_1);
                std::memcpy(new_buffer + len_1, _buffer, len_2);
            }
        }

        _pos_read = 0;
        _pos_write = used;

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
    bool empty() const
    {
        return _pos_read == _pos_write;
    }

    auto used_space() const -> std::size_t
    {
        return (_capacity + _pos_write - _pos_read) % _capacity;
    }

    auto available_space() const -> std::size_t
    {
        return effective_capacity() - used_space();
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

    auto consecutive_write_length() const -> std::size_t
    {
        return std::min(_capacity - _pos_write, available_space());
    }

    auto consecutive_read_length() const -> std::size_t
    {
        return std::min(_capacity - _pos_read, used_space());
    }

    auto read_pos() const -> std::size_t
    {
        return _pos_read;
    }

    auto write_pos() const -> std::size_t
    {
        return _pos_write;
    }

    void move_read_pos(std::ptrdiff_t diff)
    {
        _pos_read = (_pos_read + diff + _capacity) % _capacity;
    }

    void move_write_pos(std::ptrdiff_t diff)
    {
        _pos_write = (_pos_write + diff + _capacity) % _capacity;
    }

private:
    std::byte* _buffer;
    std::size_t _capacity;

    std::size_t _pos_read;
    std::size_t _pos_write;
};

} // namespace nb
