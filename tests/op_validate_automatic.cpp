#include "NetBuff/ObjectPool.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <source_location>
#include <unordered_set>
#include <utility>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cout << "Failed " << #condition << "\n"; \
            const auto loc = std::source_location::current(); \
            std::cout << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            std::cout << std::flush; \
            std::exit(2); \
        } \
    } while (false)

constexpr int ITEMS = 10'000'000;
constexpr int ADD_ITEM_MULTIPLIER = 5;

struct Data
{
    int num;

    bool operator==(const Data& other) const
    {
        return num == other.num;
    }
};

template <int kind>
struct Item
{
    Data data;

    Item(Data data_) : data(data_)
    {
        ++alive_count;
    }

    ~Item()
    {
        --alive_count;
    }

    inline static int alive_count = 0;
};

struct SetItem
{
    Data data;
    Item<0>* no_destroy_item;
    Item<1>* destroy_item;

    bool operator==(const SetItem& other) const
    {
        return no_destroy_item == other.no_destroy_item;
    }
};

namespace std
{

template <>
struct hash<SetItem>
{
    auto operator()(const SetItem& item) const -> std::size_t
    {
        return hash<Item<0>*>{}(item.no_destroy_item);
    }
};

} // namespace std

int main()
{
    unsigned seed = []() -> unsigned {
        std::random_device rd;
        return rd();
    }();

    std::cout << "seed=" << seed << "\n";

    std::mt19937 rng(seed);

    std::uniform_int_distribution add_dist(0, ADD_ITEM_MULTIPLIER);
    std::uniform_int_distribution all_int_dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

    {
        nb::ObjectPool<Item<0>, false> no_destroy_pool;
        nb::ObjectPool<Item<1>, true> destroy_pool;

        std::unordered_set<SetItem> item_set;
        item_set.reserve(ITEMS);

        int item_add_count = 0;

        auto add_item = [&] {
            int num = all_int_dist(rng);
            const Data data{num};

            SetItem item{
                .data = data,
                .no_destroy_item = &no_destroy_pool.construct(data),
                .destroy_item = &destroy_pool.construct(data),
            };

            // no destroy needs manual init
            item.no_destroy_item->data = data;

            item_set.insert(std::move(item));

            ++item_add_count;
        };

        auto remove_item = [&] {
            assert(!item_set.empty());
            auto it = item_set.begin();

            TEST_ASSERT(it->data == it->no_destroy_item->data);
            TEST_ASSERT(it->data == it->destroy_item->data);

            no_destroy_pool.destroy(*it->no_destroy_item);
            destroy_pool.destroy(*it->destroy_item);

            item_set.erase(it);
        };

        while (item_add_count < ITEMS || !item_set.empty())
        {
            if (item_add_count < ITEMS && !item_set.empty())
            {
                if (add_dist(rng))
                    add_item();
                else
                    remove_item();
            }
            else if (item_add_count < ITEMS)
                add_item();
            else // !item_set.empty()
                remove_item();
        }

        TEST_ASSERT(Item<0>::alive_count != 0);
        TEST_ASSERT(Item<1>::alive_count == 0);
    }
    TEST_ASSERT(Item<0>::alive_count == 0);

    std::cout << "All is well!" << std::endl;
}
