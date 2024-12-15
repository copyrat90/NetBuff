#include "NetBuff/LockfreeObjectPool.hpp"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::ostringstream oss; \
            oss << "Failed " << #condition << "\n"; \
            const auto loc = std::source_location::current(); \
            oss << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            std::cout << oss.str() << std::flush; \
            std::exit(2); \
        } \
    } while (false)

constexpr int PHASES = 100;
constexpr int ALLOC_PER_THREAD = 100000;

struct Item
{
    std::unique_ptr<std::thread::id> tid;

    Item(std::thread::id tid_ = std::thread::id()) : tid(std::make_unique<std::thread::id>(tid_)){};

    void reset(std::thread::id tid_ = std::thread::id())
    {
        *tid = tid_;
    }
};

std::optional<nb::LockfreeObjectPool<Item, true>> destroy_pool;
std::optional<nb::LockfreeObjectPool<Item, false>> no_destroy_pool;

std::atomic<int> g_phase;
std::atomic<bool> exit_flag;

static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);

enum class AllocStrategy
{
    ALLOC_ALL_DEALLOC_ALL,
    PING_PONG_ALLOC_DEALLOC,

    TOTAL
};

template <bool Destroy, typename TPool>
void do_work(std::thread::id tid, TPool& pool, std::vector<Item*>& items, AllocStrategy strategy, std::mt19937&)
{
    assert(pool.has_value());

    switch (strategy)
    {
    case AllocStrategy::ALLOC_ALL_DEALLOC_ALL:
        for (int i = 0; i < ALLOC_PER_THREAD; ++i)
        {
            items.push_back(&pool->construct(tid));
            if constexpr (!Destroy)
                items[i]->reset(tid);
        }

        std::this_thread::yield();
        for (int i = 0; i < ALLOC_PER_THREAD; ++i)
            TEST_ASSERT(*items[i]->tid == tid);

        for (int i = 0; i < ALLOC_PER_THREAD; ++i)
        {
            items[i]->reset();
            pool->destroy(*items[i]);
        }
        break;

    case AllocStrategy::PING_PONG_ALLOC_DEALLOC:
        for (int i = 0; i < ALLOC_PER_THREAD; ++i)
        {
            items.push_back(&pool->construct(tid));
            if constexpr (!Destroy)
                items.back()->reset(tid);

            std::this_thread::yield();
            TEST_ASSERT(*items.back()->tid == tid);

            items.back()->reset();
            pool->destroy(*items.back());
        }
        break;

    default:
        throw std::logic_error(std::format("Invalid strategy = {}", static_cast<int>(strategy)));
    }
}

void worker(std::atomic<int>& phase_done)
{
    std::thread::id tid = std::this_thread::get_id();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution dist(0, static_cast<int>(AllocStrategy::TOTAL) - 1);
    AllocStrategy strategy = static_cast<AllocStrategy>(dist(rng));

    std::vector<Item*> items;
    items.reserve(ALLOC_PER_THREAD);

    for (int phase = 1; phase <= PHASES; ++phase)
    {
        g_phase.wait(phase - 1);

        if (phase % 2)
            do_work<true>(tid, destroy_pool, items, strategy, rng);
        else
            do_work<false>(tid, no_destroy_pool, items, strategy, rng);

        strategy = static_cast<AllocStrategy>(dist(rng));
        items.clear();

        phase_done.store(phase);
        phase_done.notify_one();
    }
}

int main()
{
    std::mt19937 rng(std::random_device{}());

    const unsigned cores = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2;
    std::cout << "Preparing " << cores << " concurrent threads...\n";
    std::vector<std::thread> threads;
    threads.reserve(cores);
    std::deque<std::atomic<int>> phase_done_per_threads(cores);
    for (unsigned i = 0; i < cores; ++i)
        threads.emplace_back(worker, std::ref(phase_done_per_threads[i]));

    std::cout << "Starting tests...\n";

    std::ostringstream err_oss;
    std::string err_str;

    for (int phase = 1; phase <= PHASES; ++phase)
    {
        const bool capacity_check = std::uniform_int_distribution(0, 1)(rng);
        const std::size_t init_capacity = (capacity_check) ? cores * ALLOC_PER_THREAD : 0;
        std::cout << "phase #" << phase << " (capacity check: " << capacity_check << ")\n";

        // clear `err_oss`
        err_oss.clear();
        err_oss.str("");

        // reset to fresh pool
        if (phase % 2)
        {
            destroy_pool.emplace(init_capacity);
            destroy_pool->set_err_stream(&err_oss);
        }
        else
        {
            no_destroy_pool.emplace(init_capacity);
            no_destroy_pool->set_err_stream(&err_oss);
        }

        // start this phase
        g_phase.store(phase);
        g_phase.notify_all();

        // wait for all threads to be done for this phase
        for (unsigned i = 0; i < cores; ++i)
            phase_done_per_threads[i].wait(phase - 1);

        // check if somethings gone wrong
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (phase % 2)
        {
            if (capacity_check)
                TEST_ASSERT(destroy_pool->monitor_capacity() == init_capacity);
        }
        else
        {
            if (capacity_check)
                TEST_ASSERT(no_destroy_pool->monitor_capacity() == init_capacity);
        }

        // clean up this phase
        if (phase % 2)
            destroy_pool.reset();
        else
            no_destroy_pool.reset();

        // check error stream
        err_str = err_oss.str();
        if (!err_str.empty())
        {
            std::cout << err_str << std::endl;
            TEST_ASSERT(err_str.empty());
        }
    }

    for (auto& t : threads)
        t.join();

    std::cout << "All is well!" << std::endl;
}
