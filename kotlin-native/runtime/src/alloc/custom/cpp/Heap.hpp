/*
 * Copyright 2022 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

#ifndef CUSTOM_ALLOC_CPP_HEAP_HPP_
#define CUSTOM_ALLOC_CPP_HEAP_HPP_

#include <atomic>
#include <mutex>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <iostream>


#include "AtomicStack.hpp"
#include "ExtraObjectPage.hpp"
#include "ExtraObjectData.hpp"
#include "GCStatistics.hpp"
#include "Memory.h"
#include "SingleObjectPage.hpp"
#include "NextFitPage.hpp"
#include "PageStore.hpp"
#include "FixedBlockPage.hpp"
#include "GCApi.hpp"

namespace kotlin::alloc {

class HeapUsageTracer {
public:
    // Singleton
    static HeapUsageTracer& Instance() {
        static HeapUsageTracer instance;
        return instance;
    }

    HeapUsageTracer() {
        start();
    }

    void start() {
        stopRequested_ = false;
        // Only start if not already running
        if (tracingThread_.joinable()) return;
        tracingThread_ = std::thread([this] { run(); });
    }

    void stop() {
        stopRequested_ = true;
        cv_.notify_all();
        if (tracingThread_.joinable())
            tracingThread_.join();
        dump();
    }

    void markGC(bool status) {
        if (!events_.empty())
            events_.back().gc = (status);
    }

    void dump(std::ostream& out = std::cout) {
        size_t max = 0;
        for (const auto& e : events_) if (e.bytes > max) max = e.bytes;
        for (const auto& e : events_) {
            out << e.bytes << ","
                << (e.gc ? "true\n" : "false\n");
        }
    }

    ~HeapUsageTracer() { stop(); }

private:
    struct Event {
        size_t bytes;
        bool gc;
    };

    void run() {
        auto next = std::chrono::steady_clock::now();
        while (!stopRequested_) {
            next += std::chrono::microseconds(100);
            size_t bytes = GetAllocatedBytes();
            {
                events_.push_back({bytes, false});
            }
            std::unique_lock<std::mutex> lock(cvMutex_);
            cv_.wait_until(lock, next, [this]() { return stopRequested_.load(); });
        }
    }

    std::vector<Event> events_;
    // std::mutex mutex_;
    std::atomic<bool> stopRequested_{false};
    std::thread tracingThread_;
    std::condition_variable cv_;
    std::mutex cvMutex_;
};

class Heap {
public:
    // Called once by the GC thread after all mutators have been suspended
    void PrepareForGC() noexcept;

    // Sweep through all remaining pages, freeing those blocks where CanReclaim
    // returns true. If multiple sweepers are active, each page will only be
    // seen by one sweeper.
    FinalizerQueue Sweep(gc::GCHandle gcHandle) noexcept;

    FixedBlockPage* GetFixedBlockPage(uint32_t cellCount, FinalizerQueue& finalizerQueue) noexcept;
    NextFitPage* GetNextFitPage(uint32_t cellCount, FinalizerQueue& finalizerQueue) noexcept;
    SingleObjectPage* GetSingleObjectPage(uint64_t cellCount, FinalizerQueue& finalizerQueue) noexcept;
    ExtraObjectPage* GetExtraObjectPage(FinalizerQueue& finalizerQueue) noexcept;

    void AddToFinalizerQueue(FinalizerQueue queue) noexcept;
    FinalizerQueue ExtractFinalizerQueue() noexcept;

    // Test method
    std::vector<ObjHeader*> GetAllocatedObjects() noexcept;
    void ClearForTests() noexcept;

    auto& allocatedSizeTracker() noexcept { return allocatedSizeTracker_; }

    template <typename T>
    void TraverseAllocatedObjects(T process) noexcept(noexcept(process(std::declval<ObjHeader*>()))) {
        for (int blockSize = 0; blockSize <= FixedBlockPage::MAX_BLOCK_SIZE; ++blockSize) {
            fixedBlockPages_[blockSize].TraversePages([process](auto *page) {
                page->TraverseAllocatedBlocks([process](auto *block) {
                    process(reinterpret_cast<CustomHeapObject*>(block)->object());
                });
            });
        }
        nextFitPages_.TraversePages([process](auto *page) {
            page->TraverseAllocatedBlocks([process](auto *block) {
                process(reinterpret_cast<CustomHeapObject*>(block)->object());
            });
        });
        singleObjectPages_.TraversePages([process](auto *page) {
            page->TraverseAllocatedBlocks([process](auto *block) {
                process(reinterpret_cast<CustomHeapObject*>(block)->object());
            });
        });
    }

    template <typename T>
    void TraverseAllocatedExtraObjects(T process) noexcept(noexcept(process(std::declval<kotlin::mm::ExtraObjectData*>()))) {
        extraObjectPages_.TraversePages([process](auto *page) {
            page->TraverseAllocatedObjects(process);
        });
    }

    void Dump() {
        if (dumpEnabled == false) return;
        for (int blockSize = 0; blockSize <= FixedBlockPage::MAX_BLOCK_SIZE; ++blockSize) {
            if (fixedBlockPages_[blockSize].GetPages().empty()) {
                continue;
            }
            std::cout << blockSize << ": ";
            fixedBlockPages_[blockSize].TraversePages([](auto *page) {
                page->Dump(std::cout);
            });
            std::cout << std::endl;
        }
        std::cout << "nextFitPages" << ": ";
        nextFitPages_.TraversePages([](auto *page) {
            page->Dump(std::cout);
        });
        std::cout << std::endl;
        std::cout << "singleObjectPages" << ": ";
        singleObjectPages_.TraversePages([](auto *page) {
            page->Dump(std::cout);
        });
        std::cout << std::endl;
        std::cout << "extraObjectPages" << ": ";
        extraObjectPages_.TraversePages([](auto *page) {
            page->Dump(std::cout);
        });
        std::cout << std::endl;
    }

    void markGC(bool status) {
        heapUsageTracer_.markGC(status);
    }

private:
    PageStore<FixedBlockPage> fixedBlockPages_[FixedBlockPage::MAX_BLOCK_SIZE + 1];
    PageStore<NextFitPage> nextFitPages_;
    PageStore<SingleObjectPage> singleObjectPages_;
    PageStore<ExtraObjectPage> extraObjectPages_;

    FinalizerQueue pendingFinalizerQueue_;
    std::mutex pendingFinalizerQueueMutex_;

    std::atomic<std::size_t> concurrentSweepersCount_ = 0;

    AllocatedSizeTracker::Heap allocatedSizeTracker_{};
    HeapUsageTracer& heapUsageTracer_ = HeapUsageTracer::Instance();
    bool dumpEnabled = false;
};

} // namespace kotlin::alloc

#endif
