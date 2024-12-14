#pragma once

#include <bit>
#include <cstdint>
#include <format>
#include <stdexcept>

#ifndef NB_VA_BITS
#define NB_VA_BITS 56
#endif

namespace nb
{

template <typename T>
class TaggedPtr
{
private:
    std::uintptr_t _tagged_addr;

public:
    static_assert(std::has_single_bit(alignof(T)), "Not power of two alignment for `T`");
    static_assert(sizeof(_tagged_addr) == 8, "LockfreeObjectPool only supports 64-bit architecture");
    static_assert(8 <= NB_VA_BITS && NB_VA_BITS <= 64, "Invalid `NB_VA_BITS`");

public:
    static constexpr std::size_t UPPER_TAG_BITS = 64 - NB_VA_BITS;
    static constexpr std::uintptr_t UPPER_TAG_MASK = ((std::uintptr_t(1) << UPPER_TAG_BITS) - 1) << NB_VA_BITS;

    static constexpr std::size_t LOWER_TAG_BITS = std::countr_zero(alignof(T));
    static constexpr std::uintptr_t LOWER_TAG_MASK = alignof(T) - 1;

    static_assert(!(UPPER_TAG_MASK & LOWER_TAG_MASK), "Tag masks overlap; Possibly invalid `NB_VA_BITS`");
    static_assert(NB_VA_BITS >= LOWER_TAG_BITS,
                  "`NB_VA_BITS` is smaller than `T` alignment; Possibly invalid `NB_VA_BITS`");

    static constexpr std::uintptr_t TAG_MASK = UPPER_TAG_MASK | LOWER_TAG_MASK;

public:
    TaggedPtr(T* ptr, std::uintptr_t tag) : _tagged_addr(reinterpret_cast<std::uintptr_t>(ptr))
    {
        if (_tagged_addr & TAG_MASK)
            throw std::logic_error(std::format("ptr address `0x{:016x}` holds tag bit", _tagged_addr));

        set_tag(tag);
    }

    TaggedPtr(T* ptr) : _tagged_addr(reinterpret_cast<std::uintptr_t>(ptr))
    {
        if (_tagged_addr & TAG_MASK)
            throw std::logic_error(std::format("ptr address `0x{:016x}` holds tag bit", _tagged_addr));
    }

    TaggedPtr() : _tagged_addr(0)
    {
    }

public:
    auto get_ptr() const noexcept -> T*
    {
        return reinterpret_cast<T*>(_tagged_addr & ~TAG_MASK);
    }

    void set_ptr(T* ptr)
    {
        const auto addr = reinterpret_cast<std::uintptr_t>(ptr);

        if (addr & TAG_MASK)
            throw std::logic_error(std::format("ptr address `0x{:016x}` holds tag bit", addr));

        _tagged_addr = (_tagged_addr & TAG_MASK) | addr;
    }

public:
    explicit operator T*() const noexcept
    {
        return get_ptr();
    }

    auto operator*() const noexcept -> T&
    {
        return *get_ptr();
    }

    auto operator->() const noexcept -> T*
    {
        return get_ptr();
    }

    operator bool() const noexcept
    {
        return get_ptr();
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

} // namespace nb
