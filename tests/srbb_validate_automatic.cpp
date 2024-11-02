#include "NetBuff/SpscRingByteBuffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <vector>

#define TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            const auto loc = std::source_location::current(); \
            std::cout << "Failed " << #condition << "\n\t"; \
            std::cout << "at " << loc.file_name() << ":" << loc.line() << ":" << loc.column() << "\n"; \
            print_buffers(); \
            std::cout << std::flush; \
            std::exit(2); \
        } \
    } while (false)

constexpr std::size_t MAX_BUFFER_SIZE = (std::size_t(1) << 28);
constexpr std::size_t MAX_CHUNK_SIZE = (std::size_t(1) << 8);

std::atomic_flag ready_flag = ATOMIC_FLAG_INIT;

nb::SpscRingByteBuffer<> ring;

std::vector<std::byte> buffer_input;
std::vector<std::byte> buffer_output;

void print_buffers()
{
    std::cout << "buffer_input: " << std::hex;
    for (const std::byte b : buffer_input)
    {
        const auto num = static_cast<unsigned>(b);
        std::cout << std::setw(2) << std::setfill('0') << num << ' ';
    }
    std::cout << "\n";

    std::cout << "buffer_output: ";
    for (const std::byte b : buffer_output)
    {
        const auto num = static_cast<unsigned>(b);
        std::cout << std::setw(2) << std::setfill('0') << num << ' ';
    }
    std::cout << std::dec << "\n";
};

void produce(unsigned seed)
{
    std::mt19937 rng(seed);
    ready_flag.wait(false);

    for (std::size_t pos = 0; pos < MAX_BUFFER_SIZE;)
    {
        const auto chunk_size =
            std::uniform_int_distribution<std::size_t>(1, std::min(MAX_CHUNK_SIZE, MAX_BUFFER_SIZE - pos))(rng);

        if (ring.try_write(buffer_input.data() + pos, chunk_size))
            pos += chunk_size;
    }
}

void consume(unsigned seed)
{
    std::mt19937 rng(seed);
    ready_flag.wait(false);

    for (std::size_t pos = 0; pos < MAX_BUFFER_SIZE;)
    {
        const auto chunk_size =
            std::uniform_int_distribution<std::size_t>(1, std::min(MAX_CHUNK_SIZE, MAX_BUFFER_SIZE - pos))(rng);

        if (ring.try_read(buffer_output.data() + pos, chunk_size))
            pos += chunk_size;
    }
}

int main()
{
    // prepare rng
    unsigned input_seed, producer_seed, consumer_seed;
    {
        std::random_device rd;
        input_seed = rd();
        producer_seed = rd();
        consumer_seed = rd();
    }
    std::cout << "input seed: " << input_seed << "\nproducer_seed: " << producer_seed
              << "\nconsumer_seed: " << consumer_seed << std::endl;
    std::mt19937 rng(input_seed);
    std::uniform_int_distribution<std::uint64_t> u64_dist;

    // prepare buffers
    if (!ring.try_resize(MAX_CHUNK_SIZE))
    {
        std::cout << "Ring alloc failed\n";
        throw std::runtime_error("Ring alloc failed");
    }
    buffer_input.resize(MAX_BUFFER_SIZE);
    buffer_output.resize(MAX_BUFFER_SIZE);

    // fill `buffer_input` with random bytes
    static_assert(MAX_BUFFER_SIZE % 8 == 0);
    for (std::size_t i = 0; i < MAX_BUFFER_SIZE / 8; ++i)
    {
        auto& val = reinterpret_cast<std::uint64_t*>(buffer_input.data())[i];
        val = u64_dist(rng);
    }

    // run producer & consumer threads
    std::thread producer(produce, producer_seed);
    std::thread consumer(consume, consumer_seed);
    ready_flag.test_and_set();
    ready_flag.notify_all();
    producer.join();
    consumer.join();

    TEST_ASSERT(buffer_input == buffer_output);

    std::cout << "All is well!" << std::endl;
}
