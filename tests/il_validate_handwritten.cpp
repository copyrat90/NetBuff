#include "NetBuff/IntrusiveList.hpp"

#include <array>
#include <compare>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <source_location>
#include <utility>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            const auto loc = std::source_location::current(); \
            std::cout << "Failed: " << #condition << "\n\t"; \
            std::cout << "at " << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            std::exit(1); \
        } \
    } while (false)

struct MyInt : public nb::IntrusiveListNode
{
    int num;

    MyInt(int num_) : num(num_)
    {
    }

    bool operator==(const MyInt& other) const
    {
        return num == other.num;
    }
};

bool list_equals(const nb::IntrusiveList<MyInt>& intru_list, std::initializer_list<int> init_list)
{
    if (intru_list.size() != init_list.size())
        return false;

    auto it_intru = intru_list.cbegin();
    auto it_init = init_list.begin();

    while (it_intru != intru_list.cend())
    {
        if (it_intru->num != *it_init)
            return false;

        ++it_intru;
        ++it_init;
    }

    return true;
}

int main()
{
    std::array<MyInt, 10> arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    nb::IntrusiveList<MyInt> list;
    TEST_ASSERT(list.empty());
    TEST_ASSERT(list.cbegin() == list.end());
    list.push_back(arr[9]);
    TEST_ASSERT(list.front() == arr[9]);
    TEST_ASSERT(list.back() == arr[9]);
    list.pop_back();
    TEST_ASSERT(list.empty());
    list.insert(list.begin(), arr.begin(), arr.begin() + 5);
    TEST_ASSERT(list_equals(list, {0, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 5);
    list.push_front(arr[5]);
    TEST_ASSERT(list_equals(list, {5, 0, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 6);
    list.insert(arr[5], arr.begin() + 6, arr.end());
    TEST_ASSERT(list_equals(list, {6, 7, 8, 9, 5, 0, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 10);
    list.erase(arr[0]);
    TEST_ASSERT(list_equals(list, {6, 7, 8, 9, 5, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 9);
    list.erase(++ ++ ++ ++list.begin());
    TEST_ASSERT(list_equals(list, {6, 7, 8, 9, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 8);
    list.erase(list.begin(), ++ ++list.begin());
    TEST_ASSERT(list_equals(list, {8, 9, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 6);
    list.pop_front();
    TEST_ASSERT(list_equals(list, {9, 1, 2, 3, 4}));
    TEST_ASSERT(list.size() == 5);
    list.remove(arr[2]);
    TEST_ASSERT(list_equals(list, {9, 1, 3, 4}));
    TEST_ASSERT(list.size() == 4);
    list.remove_if([](const MyInt& elem) { return elem == MyInt(3); });
    TEST_ASSERT(list_equals(list, {9, 1, 4}));
    TEST_ASSERT(list.size() == 3);

    nb::IntrusiveList<MyInt> list2(std::move(list));
    TEST_ASSERT(list.empty());
    TEST_ASSERT(list_equals(list2, {9, 1, 4}));
    TEST_ASSERT(list2.size() == 3);
    list2.push_front(arr[3]);
    TEST_ASSERT(list_equals(list2, {3, 9, 1, 4}));
    TEST_ASSERT(list2.size() == 4);
    list2.insert(arr[1], arr[5]);
    TEST_ASSERT(list_equals(list2, {3, 9, 5, 1, 4}));
    TEST_ASSERT(list2.size() == 5);
    list2.insert(++ ++list2.begin(), arr.begin() + 6, arr.begin() + 9);
    TEST_ASSERT(list_equals(list2, {3, 9, 6, 7, 8, 5, 1, 4}));
    TEST_ASSERT(list2.crbegin()->num == 4);
    TEST_ASSERT((--list2.rend())->num == 3);
    TEST_ASSERT(list2.size() == 8);
    list2.clear();
    TEST_ASSERT(list2.empty());

    std::cout << "All is well!" << std::endl;
}
