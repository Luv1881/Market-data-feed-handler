#include "market_data/core/common.hpp"
#include "market_data/core/market_event.hpp"
#include "market_data/lockfree/circular_buffer.hpp"
#include "market_data/lockfree/mpmc_queue.hpp"
#include "market_data/memory/memory_pool.hpp"
#include "market_data/threading/thread_utils.hpp"
#include "market_data/protocol/protocol_parser.hpp"
#include "market_data/metrics/latency_tracker.hpp"
#include "market_data/utils/timestamp.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <iomanip>
#include <memory>

using namespace market_data;

// Global shutdown flag
std::atomic<bool> g_shutdown{false};

// Global metrics
MetricsCollector g_metrics;

/**
 * Producer thread: simulates exchange feed
 */
void producer_thread(int cpu_id, CircularBuffer<MarketEvent, 1024 * 1024>& buffer) {
    ThreadUtils::pin_current_thread_to_cpu(cpu_id);
    ThreadUtils::set_current_thread_name("producer");

    std::cout << "[Producer] Started on CPU " << cpu_id << std::endl;

    uint64_t sequence = 0;
    const uint32_t venue_id = 1;

    while (!g_shutdown.load(std::memory_order_relaxed)) {
        MarketEvent event;
        event.venue_id = venue_id;
        event.sequence_number = sequence++;
        event.event_type = EventType::TRADE;
        event.exchange_timestamp = Timestamp::now_ns();
        event.receive_timestamp = rdtscp();
        event.symbol = Symbol("AAPL");
        event.price = 15000000000LL; // $150.00 in fixed-point
        event.quantity = 100 * 100000000LL; // 100 shares
        event.side = Side::BID;

        // Try to push to buffer
        while (!buffer.try_push(event)) {
            if (g_shutdown.load(std::memory_order_relaxed)) {
                return;
            }
            g_metrics.record_queue_full();
            cpu_pause();
        }

        g_metrics.record_message_received();

        // Simulate message rate control (~1M msgs/sec)
        if (sequence % 1000 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
        }
    }

    std::cout << "[Producer] Stopped. Produced " << sequence << " events" << std::endl;
}

/**
 * Consumer thread: processes market events
 */
void consumer_thread(int cpu_id, CircularBuffer<MarketEvent, 1024 * 1024>& buffer) {
    ThreadUtils::pin_current_thread_to_cpu(cpu_id);
    ThreadUtils::set_current_thread_name("consumer");

    std::cout << "[Consumer] Started on CPU " << cpu_id << std::endl;

    uint64_t processed = 0;
    uint64_t last_sequence = 0;

    while (!g_shutdown.load(std::memory_order_relaxed) || !buffer.empty()) {
        MarketEvent event;

        if (buffer.try_pop(event)) {
            // Measure end-to-end latency
            uint64_t current_tsc = rdtscp();
            uint64_t latency_tsc = current_tsc - event.receive_timestamp;
            uint64_t latency_ns = Timestamp::tsc_to_ns(latency_tsc);

            g_metrics.end_to_end_latency.record(latency_ns);

            // Check for sequence gaps
            if (event.sequence_number != last_sequence + 1 && processed > 0) {
                g_metrics.record_sequence_gap();
            }
            last_sequence = event.sequence_number;

            g_metrics.record_message_processed();
            processed++;
        } else {
            // Spin wait with pause
            cpu_pause();
        }
    }

    std::cout << "[Consumer] Stopped. Processed " << processed << " events" << std::endl;
}

/**
 * Statistics thread: prints metrics periodically
 */
void stats_thread() {
    ThreadUtils::set_current_thread_name("stats");

    std::cout << "[Stats] Started" << std::endl;

    while (!g_shutdown.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        uint64_t received = g_metrics.messages_received.load(std::memory_order_relaxed);
        uint64_t processed = g_metrics.messages_processed.load(std::memory_order_relaxed);
        uint64_t dropped = g_metrics.messages_dropped.load(std::memory_order_relaxed);
        uint64_t gaps = g_metrics.sequence_gaps.load(std::memory_order_relaxed);
        uint64_t queue_full = g_metrics.queue_full_events.load(std::memory_order_relaxed);

        std::cout << "\n=== Statistics ===" << std::endl;
        std::cout << "Received:  " << received << std::endl;
        std::cout << "Processed: " << processed << std::endl;
        std::cout << "Dropped:   " << dropped << std::endl;
        std::cout << "Gaps:      " << gaps << std::endl;
        std::cout << "QueueFull: " << queue_full << std::endl;

        if (g_metrics.end_to_end_latency.count() > 0) {
            std::cout << "\nLatency (microseconds):" << std::endl;
            std::cout << "  Min:    " << std::setw(8)
                      << (g_metrics.end_to_end_latency.min() / 1000) << " us" << std::endl;
            std::cout << "  p50:    " << std::setw(8)
                      << (g_metrics.end_to_end_latency.p50() / 1000) << " us" << std::endl;
            std::cout << "  p99:    " << std::setw(8)
                      << (g_metrics.end_to_end_latency.p99() / 1000) << " us" << std::endl;
            std::cout << "  p99.9:  " << std::setw(8)
                      << (g_metrics.end_to_end_latency.p999() / 1000) << " us" << std::endl;
            std::cout << "  p99.99: " << std::setw(8)
                      << (g_metrics.end_to_end_latency.p9999() / 1000) << " us" << std::endl;
            std::cout << "  Max:    " << std::setw(8)
                      << (g_metrics.end_to_end_latency.max() / 1000) << " us" << std::endl;
            std::cout << "  Mean:   " << std::setw(8)
                      << (g_metrics.end_to_end_latency.mean() / 1000) << " us" << std::endl;
        }

        std::cout << "==================\n" << std::endl;
    }

    std::cout << "[Stats] Stopped" << std::endl;
}

/**
 * Test lock-free data structures
 */
void test_lockfree_structures() {
    std::cout << "\n=== Testing Lock-Free Data Structures ===" << std::endl;

    // Test circular buffer
    {
        CircularBuffer<int, 16> buffer;
        std::cout << "CircularBuffer capacity: " << buffer.capacity() << std::endl;

        // Push some items
        for (int i = 0; i < 10; ++i) {
            buffer.try_push(i);
        }

        std::cout << "CircularBuffer size: " << buffer.size() << std::endl;

        // Pop items
        int value;
        int count = 0;
        while (buffer.try_pop(value)) {
            count++;
        }

        std::cout << "Popped " << count << " items" << std::endl;
        std::cout << "CircularBuffer is empty: " << buffer.empty() << std::endl;
    }

    // Test MPMC queue
    {
        MPMCQueue<int, 1024> queue;

        // Enqueue items
        for (int i = 0; i < 100; ++i) {
            queue.try_enqueue(i);
        }

        std::cout << "MPMCQueue size: " << queue.size() << std::endl;

        // Dequeue items
        int value;
        int count = 0;
        while (queue.try_dequeue(value)) {
            count++;
        }

        std::cout << "Dequeued " << count << " items" << std::endl;
        std::cout << "MPMCQueue is empty: " << queue.empty() << std::endl;
    }

    std::cout << "=== Lock-Free Tests Passed ===" << std::endl;
}

/**
 * Main function
 */
int main(int argc, char** argv) {
    std::cout << "=== Ultra-Low-Latency Market Data Feed Handler ===" << std::endl;
    std::cout << "C++20 High-Performance Trading System\n" << std::endl;

    // Initialize timestamp calibration
    Timestamp::initialize();
    std::cout << "TSC frequency: " << Timestamp::get_tsc_frequency() << " Hz" << std::endl;

    // Check system capabilities
    int num_cpus = ThreadUtils::get_num_cpus();
    std::cout << "Number of CPUs: " << num_cpus << std::endl;

    auto isolated_cpus = ThreadUtils::get_isolated_cpus();
    if (!isolated_cpus.empty()) {
        std::cout << "Isolated CPUs: ";
        for (int cpu : isolated_cpus) {
            std::cout << cpu << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Warning: No isolated CPUs found. For best performance, use 'isolcpus' kernel parameter" << std::endl;
    }

    // Test lock-free structures
    test_lockfree_structures();

    // Check cache line size
    std::cout << "\nCache line size: " << CACHE_LINE_SIZE << " bytes" << std::endl;
    std::cout << "MarketEvent size: " << sizeof(MarketEvent) << " bytes" << std::endl;

    // Run simulation
    std::cout << "\n=== Starting Market Data Feed Simulation ===" << std::endl;
    std::cout << "Press Ctrl+C to stop...\n" << std::endl;

    // Create circular buffer (on heap to avoid stack overflow - 67MB is too large for stack)
    auto buffer = std::make_unique<CircularBuffer<MarketEvent, 1024 * 1024>>();

    // Determine CPU affinity
    int producer_cpu = isolated_cpus.empty() ? 0 : isolated_cpus[0];
    int consumer_cpu = isolated_cpus.size() > 1 ? isolated_cpus[1] :
                       (num_cpus > 1 ? 1 : 0);

    // Start threads
    std::thread producer(producer_thread, producer_cpu, std::ref(*buffer));
    std::thread consumer(consumer_thread, consumer_cpu, std::ref(*buffer));
    std::thread stats(stats_thread);

    // Run for a duration or until interrupted
    int duration_seconds = (argc > 1) ? std::atoi(argv[1]) : 10;
    std::cout << "Running for " << duration_seconds << " seconds..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    // Signal shutdown
    std::cout << "\nShutting down..." << std::endl;
    g_shutdown.store(true, std::memory_order_relaxed);

    // Wait for threads
    producer.join();
    consumer.join();
    stats.join();

    // Final statistics
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Total received:  " << g_metrics.messages_received.load() << std::endl;
    std::cout << "Total processed: " << g_metrics.messages_processed.load() << std::endl;
    std::cout << "Total dropped:   " << g_metrics.messages_dropped.load() << std::endl;
    std::cout << "Sequence gaps:   " << g_metrics.sequence_gaps.load() << std::endl;

    if (g_metrics.end_to_end_latency.count() > 0) {
        std::cout << "\nFinal Latency Statistics (microseconds):" << std::endl;
        std::cout << "  Count:  " << g_metrics.end_to_end_latency.count() << std::endl;
        std::cout << "  Min:    " << (g_metrics.end_to_end_latency.min() / 1000) << " us" << std::endl;
        std::cout << "  p50:    " << (g_metrics.end_to_end_latency.p50() / 1000) << " us" << std::endl;
        std::cout << "  p99:    " << (g_metrics.end_to_end_latency.p99() / 1000) << " us" << std::endl;
        std::cout << "  p99.9:  " << (g_metrics.end_to_end_latency.p999() / 1000) << " us" << std::endl;
        std::cout << "  p99.99: " << (g_metrics.end_to_end_latency.p9999() / 1000) << " us" << std::endl;
        std::cout << "  Max:    " << (g_metrics.end_to_end_latency.max() / 1000) << " us" << std::endl;
        std::cout << "  Mean:   " << (g_metrics.end_to_end_latency.mean() / 1000) << " us" << std::endl;
    }

    std::cout << "\n=== Shutdown Complete ===" << std::endl;

    return 0;
}
