#pragma once

#include "NetBuff/SerializeBuffer_fwd.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace nb
{

template <typename T>
concept UnsignedInteger = std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
                          std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>;

template <typename T>
concept Character = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> ||
                    std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

template <typename T>
concept String = std::is_same_v<T, std::basic_string<typename T::value_type>>;

template <typename T>
concept StringView = std::is_same_v<T, std::basic_string_view<typename T::value_type>>;

template <typename T>
concept StringOrStringView = String<T> || StringView<T>;

/// @brief Buffer to serialize your message to a byte stream.
///
/// You MUST write everything before reading, or vice versa.
/// If you want back-and-forth read & write, use `RingByteBuffer` instead.
///
/// If the buffer is full, it DOESN'T increase its size automatically;
/// You need to resize it manually via `try_resize()`.
///
/// If you want to reuse an object of this class, you must call `clear()` to reset its positions to `0`.
template <typename ByteAllocator>
class SerializeBuffer : private ByteAllocator
{
    static_assert(std::is_same_v<std::byte, typename ByteAllocator::value_type>);

public:
    using DefaultStringLengthType = std::uint32_t;

public:
    SerializeBuffer() : SerializeBuffer(0)
    {
    }

    SerializeBuffer(std::size_t capacity)
        : _buffer(capacity == 0 ? nullptr : this->allocate(capacity)), _capacity(capacity), _pos_read(0), _pos_write(0),
          _fail(false)
    {
    }

    SerializeBuffer(SerializeBuffer&& other) noexcept
        : ByteAllocator(std::move(other)), _buffer(other._buffer), _capacity(other._capacity),
          _pos_read(other._pos_read), _pos_write(other._pos_write), _fail(other._fail)
    {
        other._buffer = nullptr;
        other._capacity = 0;
        other._pos_read = 0;
        other._pos_write = 0;
        other._fail = false;
    }

    SerializeBuffer& operator=(SerializeBuffer&& other) noexcept
    {
        ByteAllocator::operator=(std::move(other));

        _buffer = other._buffer;
        _capacity = other._capacity;
        _pos_read = other._pos_read;
        _pos_write = other._pos_write;
        _fail = other._fail;

        other._buffer = nullptr;
        other._capacity = 0;
        other._pos_read = 0;
        other._pos_write = 0;
        other._fail = false;

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
    /// @brief Check if read/write was failed once or more.
    ///
    /// Fail bit is never cleared unless `clear()` is called.
    bool fail() const
    {
        return _fail;
    }

    /// @brief Check if read/write was not failed at all.
    ///
    /// Fail bit is never cleared unless `clear()` is called.
    operator bool() const
    {
        return !fail();
    }

public:
    bool try_write(const void* data, std::size_t length)
    {
        if (length > available_space())
        {
            _fail = true;
            return false;
        }

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

    bool try_peek(void* dest, std::size_t length) const
    {
        if (length > used_space())
        {
            _fail = true;
            return false;
        }

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

    /// @brief Write a `Num` data, with converting it to little-endian.
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    auto operator<<(Num data) -> SerializeBuffer&
    {
        try_write<Num>(data);
        return *this;
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

    /// @brief Read a `Num` data, with converting it to little-endian.
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    auto operator>>(Num& data) -> SerializeBuffer&
    {
        try_read<Num>(data);
        return *this;
    }

    /// @brief Peek a `Num` data, with converting it to little-endian.
    template <typename Num>
        requires std::is_arithmetic_v<Num>
    bool try_peek(Num& data) const
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
    /// @tparam StringLengthType Which type to use to store the length of the string (u8, u16, u32, u64)
    template <StringOrStringView Str, UnsignedInteger StringLengthType = DefaultStringLengthType>
    bool try_write(const Str& str)
    {
        const auto str_bytes = str.length() * sizeof(typename Str::value_type);
        if (sizeof(StringLengthType) + str_bytes > available_space())
        {
            _fail = true;
            return false;
        }

        // write length of `str`
        [[maybe_unused]] bool result = try_write(static_cast<StringLengthType>(str.length()));
        assert(result);

        // only `std::u16string` & `std::u32string` are converted to little-endian
        if constexpr ((std::is_same_v<Str, std::u16string> || std::is_same_v<Str, std::u16string_view> ||
                       std::is_same_v<Str, std::u32string> || std::is_same_v<Str, std::u32string_view>) &&
                      std::endian::native == std::endian::big)
        {
            for (auto ch : str)
            {
                result = try_write(ch); // byteswap inside
                assert(result);
            }
        }
        else
        {
            result = try_write(str.data(), str_bytes);
            assert(result);
        }

        return true;
    }

    template <StringOrStringView Str>
    auto operator<<(const Str& str) -> SerializeBuffer&
    {
        try_write<Str>(str);
        return *this;
    }

    /// @tparam StringLengthType Which type to use to store the length of the string (u8, u16, u32, u64)
    template <String Str, UnsignedInteger StringLengthType = DefaultStringLengthType>
    bool try_read(Str& str)
    {
        // read length of `str`
        StringLengthType length;
        if (!try_peek(length))
            return false;

        // check if valid length of payload exists
        const auto payload_bytes = length * sizeof(typename Str::value_type);
        if (sizeof(StringLengthType) + payload_bytes > used_space())
        {
            _fail = true;
            return false;
        }

        _pos_read += sizeof(StringLengthType);

        // only `std::u16string` & `std::u32string` are converted to little-endian
        if constexpr ((std::is_same_v<Str, std::u16string> || std::is_same_v<Str, std::u16string_view> ||
                       std::is_same_v<Str, std::u32string> || std::is_same_v<Str, std::u32string_view>) &&
                      std::endian::native == std::endian::big)
        {
            str.clear();
            str.reserve(length);

            for (std::size_t idx = 0; idx < length; ++idx)
            {
                typename Str::value_type ch;
                [[maybe_unused]] const bool result = try_read(ch); // byteswap inside
                assert(result);

                str.push_back(ch);
            }
        }
        else
        {
            str.resize(length);

            [[maybe_unused]] const bool result = try_read(reinterpret_cast<std::byte*>(str.data()), payload_bytes);
            assert(result);
        }

        return true;
    }

    template <String Str>
    auto operator>>(Str& str) -> SerializeBuffer&
    {
        try_read<Str>(str);
        return *this;
    }

    /// @tparam StringLengthType Which type to use to store the length of the string (u8, u16, u32, u64)
    template <String Str, UnsignedInteger StringLengthType = DefaultStringLengthType>
    bool try_peek(Str& str) const
    {
        const auto prev_pos = _pos_read;

        if (!const_cast<SerializeBuffer*>(this)->try_read(str))
            return false;

        const_cast<SerializeBuffer*>(this)->_pos_read = prev_pos;
        return true;
    }

public:
    template <Character Char, UnsignedInteger StringLengthType = DefaultStringLengthType>
    bool try_write(const Char* null_terminated_str)
    {
        return try_write(std::basic_string_view<Char>(null_terminated_str));
    }

    template <Character Char>
    auto operator<<(const Char* null_terminated_str) -> SerializeBuffer&
    {
        try_write<Char>(null_terminated_str);
        return *this;
    }

    template <Character Char, UnsignedInteger StringLengthType = DefaultStringLengthType>
    bool try_read(Char* null_terminated_str)
    {
        // read length of `str`
        StringLengthType length;
        if (!try_peek(length))
            return false;

        const auto payload_bytes = length * sizeof(Char);
        if (sizeof(StringLengthType) + payload_bytes > used_space())
        {
            _fail = true;
            return false;
        }

        std::memcpy(null_terminated_str, _buffer + _pos_read + sizeof(StringLengthType), length * sizeof(Char));
        null_terminated_str[length] = Char{};

        _pos_read += sizeof(StringLengthType) + payload_bytes;

        return true;
    }

    template <Character Char>
    auto operator>>(Char* null_terminated_str) -> SerializeBuffer&
    {
        try_read<Char>(null_terminated_str);
        return *this;
    }

    template <Character Char, UnsignedInteger StringLengthType = DefaultStringLengthType>
    bool try_peek(Char* null_terminated_str) const
    {
        const auto prev_pos = _pos_read;

        if (!const_cast<SerializeBuffer*>(this)->try_read(null_terminated_str))
            return false;

        const_cast<SerializeBuffer*>(this)->_pos_read = prev_pos;
        return true;
    }

public:
    void clear()
    {
        _pos_read = 0;
        _pos_write = 0;
        _fail = false;
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

    mutable bool _fail;
};

} // namespace nb
