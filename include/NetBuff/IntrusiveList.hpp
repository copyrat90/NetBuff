#pragma once

#include "NetBuff/IntrusiveList_fwd.hpp"

#include <cstddef>
#include <iterator>
#include <utility>

namespace nb
{

struct IntrusiveListNode
{
private:
    template <typename T>
        requires std::is_base_of_v<IntrusiveListNode, T>
    friend class IntrusiveList;

    IntrusiveListNode* prev;
    IntrusiveListNode* next;
};

template <typename T>
    requires std::is_base_of_v<IntrusiveListNode, T>
class IntrusiveList
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

public:
    class const_iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = IntrusiveList::value_type;
        using difference_type = IntrusiveList::difference_type;
        using reference = IntrusiveList::reference;
        using const_reference = IntrusiveList::const_reference;
        using pointer = IntrusiveList::pointer;
        using const_pointer = IntrusiveList::const_pointer;

    private:
        friend class IntrusiveList;

        explicit const_iterator(const IntrusiveListNode* node) : _node(const_cast<IntrusiveListNode*>(node))
        {
        }

    protected:
        IntrusiveListNode* _node;

    public:
        auto operator*() const -> const_reference
        {
            return static_cast<const_reference>(*_node);
        }

        auto operator->() const -> const_pointer
        {
            return static_cast<const_pointer>(_node);
        }

        bool operator==(const const_iterator& other) const
        {
            return _node == other._node;
        }

        auto operator++() -> const_iterator&
        {
            _node = _node->next;
            return *this;
        }

        auto operator++(int) const -> const_iterator
        {
            auto it = *this;
            operator++();
            return it;
        }

        auto operator--() -> const_iterator&
        {
            _node = _node->prev;
            return *this;
        }

        auto operator--(int) const -> const_iterator
        {
            auto it = *this;
            operator--();
            return it;
        }
    };

    class iterator : public const_iterator
    {
    private:
        friend class IntrusiveList;

        using const_iterator::const_iterator;

    public:
        auto operator*() -> reference
        {
            return static_cast<reference>(*this->_node);
        }

        auto operator->() -> pointer
        {
            return static_cast<pointer>(this->_node);
        }

        auto operator++() -> iterator&
        {
            this->_node = this->_node->next;
            return *this;
        }

        auto operator++(int) -> iterator
        {
            auto it = *this;
            operator++();
            return it;
        }

        auto operator--() -> iterator&
        {
            this->_node = this->_node->prev;
            return *this;
        }

        auto operator--(int) -> iterator
        {
            auto it = *this;
            operator--();
            return it;
        }
    };

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    IntrusiveList()
    {
        clear();
    }

    IntrusiveList(const IntrusiveList&) = delete;

    IntrusiveList(IntrusiveList&& other) noexcept : IntrusiveList()
    {
        swap(other);
    }

    // Move and swap idiom
    IntrusiveList& operator=(IntrusiveList other) noexcept
    {
        swap(other);
        return *this;
    }

public: // Element access
    auto front() -> reference
    {
        return *begin();
    }

    auto front() const -> const_reference
    {
        return *cbegin();
    }

    auto back() -> reference
    {
        return *--end();
    }

    auto back() const -> const_reference
    {
        return *--cend();
    }

public: // Iterators
    auto begin() noexcept -> iterator
    {
        return iterator(_head.next);
    }

    auto begin() const noexcept -> const_iterator
    {
        return const_iterator(_head.next);
    }

    auto cbegin() const noexcept -> const_iterator
    {
        return const_iterator(_head.next);
    }

    auto end() noexcept -> iterator
    {
        return iterator(&_tail);
    }

    auto end() const noexcept -> const_iterator
    {
        return const_iterator(&_tail);
    }

    auto cend() const noexcept -> const_iterator
    {
        return const_iterator(&_tail);
    }

    auto rbegin() noexcept -> reverse_iterator
    {
        return reverse_iterator(end());
    }

    auto rbegin() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(end());
    }

    auto crbegin() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(cend());
    }

    auto rend() noexcept -> reverse_iterator
    {
        return reverse_iterator(begin());
    }

    auto rend() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(begin());
    }

    auto crend() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(cbegin());
    }

public: // Capacity
    bool empty() const noexcept
    {
        return _size == 0;
    }

    auto size() const noexcept -> size_type
    {
        return _size;
    }

public: // Modifiers
    void clear() noexcept
    {
        _size = 0;
        _head.next = &_tail;
        _tail.prev = &_head;
    }

    auto insert(const_iterator pos, reference value) -> iterator
    {
        ++_size;
        return link_new_node(pos, value);
    }

    auto insert(const_reference pos, reference value) -> iterator
    {
        return insert(const_iterator(&pos), value);
    }

    template <typename InputIt>
    void insert(const_iterator pos, InputIt first, InputIt last)
    {
        for (auto it = first; it != last; ++it)
            insert(pos, *it);
    }

    template <typename InputIt>
    void insert(const_reference pos, InputIt first, InputIt last)
    {
        insert(const_iterator(&pos), first, last);
    }

    auto erase(const_iterator pos) -> iterator
    {
        IntrusiveListNode* del_node = pos._node;
        IntrusiveListNode* ret_node = del_node->next;

        del_node->prev->next = del_node->next;
        del_node->next->prev = del_node->prev;

        --_size;

        return iterator(ret_node);
    }

    /// @brief Erases the specified element in O(1).
    auto erase(reference pos) -> iterator
    {
        return erase(const_iterator(&pos));
    }

    auto erase(const_iterator first, const_iterator last) -> iterator
    {
        // https://en.cppreference.com/w/cpp/container/list/erase
        // iterator `first` does not need to be dereferencable if `first == last`
        // : erasing an empty range is a no-op.

        // In order to follow the above statement, we need a check.
        if (first == last)
            return iterator(last._node);

        IntrusiveListNode* del_first = first._node;
        IntrusiveListNode* del_last = last._node;

        del_first->prev->next = del_last;
        del_last->prev = del_first->prev;

        while (del_first != del_last)
        {
            del_first = del_first->next;
            --_size;
        }

        return iterator(last._node);
    }

    void push_back(reference value)
    {
        insert(cend(), value);
    }

    void pop_back()
    {
        erase(--cend());
    }

    void push_front(reference value)
    {
        insert(cbegin(), value);
    }

    void pop_front()
    {
        erase(cbegin());
    }

    void swap(IntrusiveList& other) noexcept
    {
        using std::swap; // ADL

        // 1. swap the size first
        swap(_size, other._size);

        // 2. connect to other nodes if those are not empty
        if (!empty() && !other.empty())
        {
            swap(_head.next, other._head.next);
            swap(_tail.prev, other._tail.prev);

            _head.next->prev = &_head;
            _tail.prev->next = &_tail;

            other._head.next->prev = &other._head;
            other._tail.prev->next = &other._tail;
        }
        else if (!empty())
        {
            _head.next = other._head.next;
            _tail.prev = other._tail.prev;

            _head.next->prev = &_head;
            _tail.prev->next = &_tail;

            // `other` is empty: connect head <-> tail directly
            other._head.next = &other._tail;
            other._tail.prev = &other._head;
        }
        else if (!other.empty())
        {
            other._head.next = _head.next;
            other._tail.prev = _tail.prev;

            other._head.next->prev = &other._head;
            other._tail.prev->next = &other._tail;

            // `this` is empty: connect head <-> tail directly
            _head.next = &_tail;
            _tail.prev = &_head;
        }
    }

public: // Operations
    /// @brief Removes all elements that's equal to `value`.
    ///
    /// If you want to only remove the specified element from the list, use `erase()` instead.
    auto remove(const_reference value) -> size_type
    {
        return remove_if([&value](const_reference val) { return val == value; });
    }

    template <typename UnaryPredicate>
    auto remove_if(UnaryPredicate pred) -> size_type
    {
        for (auto it = cbegin(); it != cend();)
        {
            if (pred(*it))
                it = erase(it);
            else
                ++it;
        }

        return _size;
    }

private:
    auto link_new_node(const_iterator pos, IntrusiveListNode& new_node) -> iterator
    {
        new_node.next = pos._node;
        new_node.prev = pos._node->prev;
        pos._node->prev->next = &new_node;
        pos._node->prev = &new_node;

        return iterator(&new_node);
    }

private:
    size_type _size;

    IntrusiveListNode _head;
    IntrusiveListNode _tail;
};

} // namespace nb
