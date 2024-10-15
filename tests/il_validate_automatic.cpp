#include "NetBuff/IntrusiveList.hpp"

#include <cstddef>
#include <cstdlib>
#include <format>
#include <iostream>
#include <iterator>
#include <list>
#include <random>
#include <source_location>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            const auto loc = std::source_location::current(); \
            std::cout << "Failed " << #condition << " on command #" << c << "\n\t"; \
            std::cout << "at " << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            print_lists(); \
            print_commands(); \
            std::cout << std::flush; \
            std::exit(1); \
        } \
    } while (false)

constexpr int COMMANDS = 1'000'000;

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

struct Command
{
    enum class Kind
    {
        INSERT,
        ERASE,
    };

    Kind kind;
    int index;
    int value;
};

int main()
{
    unsigned seed = []() -> unsigned {
        std::random_device rd;
        return rd();
    }();

    std::cout << "seed=" << seed << "\n";

    std::mt19937 rng(seed);

    std::uniform_int_distribution yes_or_no(0, 1);

    std::vector<Command> commands;
    commands.reserve(COMMANDS);

    std::list<MyInt> normal_list;
    nb::IntrusiveList<MyInt> intru_list;

    auto list_insert = [&commands, &normal_list, &intru_list](const int index, const int value) {
        commands.push_back(Command{Command::Kind::INSERT, index, value});

        auto normal_it = normal_list.begin();
        auto intru_it = intru_list.begin();
        std::advance(normal_it, index);
        std::advance(intru_it, index);

        auto normal_inserted_it = normal_list.insert(normal_it, value);
        intru_list.insert(intru_it, *normal_inserted_it);
    };

    auto list_erase = [&commands, &normal_list, &intru_list](const int index) {
        commands.push_back(Command{Command::Kind::ERASE, index, 0});

        auto normal_it = normal_list.begin();
        auto intru_it = intru_list.begin();
        std::advance(normal_it, index);
        std::advance(intru_it, index);

        intru_list.erase(intru_it);
        normal_list.erase(normal_it);
    };

    auto list_equals = [&normal_list, &intru_list] {
        if (normal_list.size() != intru_list.size())
            return false;

        auto it_normal = normal_list.cbegin();
        auto it_intru = intru_list.cbegin();

        while (it_normal != normal_list.cend())
        {
            if (*it_normal != *it_intru)
                return false;

            ++it_normal;
            ++it_intru;
        }

        return true;
    };

    auto print_commands = [&commands] {
        std::cout << "Commands\n";
        for (const auto& cmd : commands)
        {
            if (cmd.kind == Command::Kind::INSERT)
                std::cout << "insert(idx=" << cmd.index << ", val=" << cmd.value << ")\n";
            else // ERASE
                std::cout << "erase(idx=" << cmd.index << ")\n";
        }
    };

    auto print_lists = [&normal_list, &intru_list] {
        std::cout << "std::list\n[";
        for (const auto& data : normal_list)
            std::cout << data.num << ", ";
        std::cout << "]\n";

        std::cout << "nb::IntrusiveList\n[";
        for (const auto& data : intru_list)
            std::cout << data.num << ", ";
        std::cout << "]\n";
    };

    for (int c = 0; c < COMMANDS; ++c)
    {
        if (normal_list.empty())
            list_insert(0, c);
        else
        {
            // insert
            if (yes_or_no(rng))
            {
                std::uniform_int_distribution index_range(0, static_cast<int>(normal_list.size()));
                const int idx = index_range(rng);
                list_insert(idx, c);
            }
            // erase
            else
            {
                std::uniform_int_distribution index_range(0, static_cast<int>(normal_list.size() - 1));
                const int idx = index_range(rng);
                list_erase(idx);
            }
        }

        TEST_ASSERT(list_equals());
    }

    std::cout << "All is well!" << std::endl;
}
