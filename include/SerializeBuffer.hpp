#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

namespace nb
{

/// @brief Buffer to serialize your message to a byte stream.
///
/// You MUST write everything before reading, or vice versa.
/// If you want back-and-forth read & write, use `RingByteBuffer` instead.
///
/// If the buffer is full, it DOESN'T increase its size automatically;
/// You need to resize it manually via `try_resize()`.
///
/// If you want to reuse an object of this class, you must call `clear()` to reset its positions to `0`.
template <typename ByteAllocator = std::allocator<std::byte>>
class SerializeBuffer : private ByteAllocator
{
    static_assert(std::is_same_v<std::byte, typename ByteAllocator::value_type>);

public:
    SerializeBuffer() : SerializeBuffer(0)
    {
    }

    SerializeBuffer(std::size_t capacity)
        : _buffer(capacity == 0 ? nullptr : this->allocate(capacity)), _capacity(capacity), _pos_read(0), _pos_write(0)
    {
    }

    SerializeBuffer(SerializeBuffer&& other) noexcept
        : ByteAllocator(std::move(other)), _buffer(other._buffer), _capacity(other._capacity),
          _pos_read(other._pos_read), _pos_write(other._pos_write)
    {
        other._buffer = nullptr;
        other._capacity = 0;
        other._pos_read = 0;
        other._pos_write = 0;
    }

    SerializeBuffer& operator=(SerializeBuffer&& other) noexcept
    {
        ByteAllocator::operator=(std::move(other));

        _buffer = other._buffer;
        _capacity = other._capacity;
        _pos_read = other._pos_read;
        _pos_write = other._pos_write;

        other._buffer = nullptr;
        other._capacity = 0;
        other._pos_read = 0;
        other._pos_write = 0;

        return *this;
    }

    SerializeBuffer(const SerializeBuffer&) = delete;
    SerializeBuffer& operator=(const SerializeBuffer&) = delete;

public:
    ~SerializeBuffer()
    {
        if (_buffer)
            this->deallocate(_buffer, _capacity);
    }

public:
    bool try_write(const void* data, std::size_t length)
    {
        if (length > available_space())
            return false;

        std::memcpy(_buffer + _pos_write, data, length);
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

        std::memcpy(dest, _buffer + _pos_read, length);

        return true;
    }

public:
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big,
                  "Mixed endian system is not supported");

    /// @brief Write a `Num` data, with converting it to little-endian.
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    bool try_write(Num data)
    {
        if constexpr (std::endian::native == std::endian::big)
            data = byteswap(data);

        return try_write(&data, sizeof(data));
    }

    /// @brief Read a `Num` data, with converting it to little-endian.
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    bool try_read(Num& data)
    {
        const bool result = try_read(&data, sizeof(data));

        if constexpr (std::endian::native == std::endian::big)
        {
            if (result)
                data = byteswap(data);
        }

        return result;
    }

    /// @brief Peek a `Num` data, with converting it to little-endian.
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    bool try_peek(Num data)
    {
        const bool result = try_peek(&data, sizeof(data));

        if constexpr (std::endian::native == std::endian::big)
        {
            if (result)
                data = byteswap(data);
        }

        return result;
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
    bool try_resize(std::size_t new_capacity)
    {
        const std::size_t used = used_space();

        if (new_capacity < used || new_capacity == _capacity)
            return false;

        std::byte* new_buffer = (new_capacity == 0) ? nullptr : this->allocate(new_capacity);

        if (new_buffer && !empty())
            std::memcpy(new_buffer, _buffer + _pos_read, used);

        _pos_read = 0;
        _pos_write = used;

        if (_buffer)
            this->deallocate(_buffer, _capacity);
        _buffer = new_buffer;
        _capacity = new_capacity;

        return true;
    }

public:
    auto capacity() const -> std::size_t
    {
        return _capacity;
    }

public:
    /// @brief Checks if the buffer is empty.
    ///
    /// WARNING! This just checks if `read_pos() == write_pos()`,
    /// so BOTH `empty()` and `full()` can be `true` if both positions are in the end of buffer.
    bool empty() const
    {
        return _pos_read == _pos_write;
    }

    /// @brief Checks if the buffer is full.
    ///
    /// WARNING! This just checks if `available_space() == 0`,
    /// so BOTH `full()` and `empty()` can be `true` if both positions are in the end of buffer.
    bool full() const
    {
        return available_space() == 0;
    }

    /// @brief Used space (i.e. How many bytes you can read before empty)
    auto used_space() const -> std::size_t
    {
        return _pos_write - _pos_read;
    }

    /// @brief Available space (i.e. How many bytes you can write before full)
    auto available_space() const -> std::size_t
    {
        return _capacity - _pos_write;
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

    auto read_pos() const -> std::size_t
    {
        return _pos_read;
    }

    auto write_pos() const -> std::size_t
    {
        return _pos_write;
    }

    // No checks performed - Use with caution!
    void move_read_pos(std::ptrdiff_t diff)
    {
        _pos_read += diff;
    }

    // No checks performed - Use with caution!
    void move_write_pos(std::ptrdiff_t diff)
    {
        _pos_write += diff;
    }

private:
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    static Num byteswap(Num value) noexcept
    {
        static_assert(std::has_unique_object_representations_v<Num>, "`Num` may not have padding bits");
        auto value_representation = std::bit_cast<std::array<std::byte, sizeof(Num)>>(value);
        std::ranges::reverse(value_representation);
        return std::bit_cast<Num>(value_representation);
    }

private:
    std::byte* _buffer;
    std::size_t _capacity;

    std::size_t _pos_read;
    std::size_t _pos_write;
};

} // namespace nb
