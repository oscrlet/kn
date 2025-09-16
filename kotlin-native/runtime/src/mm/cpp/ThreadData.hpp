/*
 * Copyright 2010-2020 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

#ifndef RUNTIME_MM_THREAD_DATA_H
#define RUNTIME_MM_THREAD_DATA_H

#include <atomic>
#include <cstdint>
#include <vector>

#include "GlobalData.hpp"
#include "GlobalsRegistry.hpp"
#include "GC.hpp"
#include "ShadowStack.hpp"
#include "SpecialRefRegistry.hpp"
#include "ThreadLocalStorage.hpp"
#include "Utils.hpp"
#include "ThreadSuspension.hpp"

#include "common_interfaces/thread/thread_holder.h"

struct ObjHeader;

namespace kotlin {
namespace mm {

// `ThreadData` is supposed to be thread local singleton.
// Pin it in memory to prevent accidental copying.
class ThreadData final : private Pinned {
public:
    explicit ThreadData(uintptr_t threadId) noexcept :
        threadId_(threadId),
        globalsThreadQueue_(GlobalsRegistry::Instance()),
        specialRefRegistry_(SpecialRefRegistry::instance()),
        gcScheduler_(GlobalData::Instance().gcScheduler(), *this),
        allocator_(GlobalData::Instance().allocator()),
        gc_(GlobalData::Instance().gc(), *this),
        suspensionData_(ThreadState::kNative, *this),
        currentThreadHolder(common::ThreadHolder::GetCurrent()) {}  // TODO: ThreadData是一个Heap对象，需要再看一下。

    ~ThreadData() = default;

    uintptr_t threadId() const noexcept { return threadId_; }

    GlobalsRegistry::ThreadQueue& globalsThreadQueue() noexcept { return globalsThreadQueue_; }

    ThreadLocalStorage& tls() noexcept { return tls_; }

    SpecialRefRegistry::ThreadQueue& specialRefRegistry() noexcept { return specialRefRegistry_; }

    ThreadState state() noexcept { return suspensionData_.state(); }

    ThreadState setState(ThreadState state) noexcept { return suspensionData_.setState(state); }

    ShadowStack& shadowStack() noexcept { return shadowStack_; }

    std::vector<std::pair<ObjHeader**, ObjHeader*>>& initializingSingletons() noexcept { return initializingSingletons_; }

    gcScheduler::GCScheduler::ThreadData& gcScheduler() noexcept { return gcScheduler_; }

    alloc::Allocator::ThreadData& allocator() noexcept { return allocator_; }

    gc::GC::ThreadData& gc() noexcept { return gc_; }

    ThreadSuspensionData& suspensionData() { return suspensionData_; }

    uintptr_t getStackTop() {
        return stackAddress;
    }

    uintptr_t getStackBottom() {
        return stackBottom;
    }

    void setStackTop(uintptr_t stack_addr) {
        stackAddress = stack_addr;
    }

    void setStackBottom(uintptr_t stack_bottom) {
        stackBottom = stack_bottom;
    }

    void Publish() noexcept {
        // TODO: These use separate locks, which is inefficient.
        globalsThreadQueue_.Publish();
        specialRefRegistry_.publish();
    }

    void ClearForTests() noexcept {
        globalsThreadQueue_.ClearForTests();
        specialRefRegistry_.clearForTests();
        allocator_.clearForTests();
    }

    common::ThreadHolder *GetCurrentThreadHolder() {
        return currentThreadHolder;
    }

private:
    const uintptr_t threadId_;
    GlobalsRegistry::ThreadQueue globalsThreadQueue_;
    ThreadLocalStorage tls_;
    SpecialRefRegistry::ThreadQueue specialRefRegistry_;
    ShadowStack shadowStack_;
    gcScheduler::GCScheduler::ThreadData gcScheduler_;
    alloc::Allocator::ThreadData allocator_;
    gc::GC::ThreadData gc_;
    std::vector<std::pair<ObjHeader**, ObjHeader*>> initializingSingletons_;
    ThreadSuspensionData suspensionData_;
    // 新增一个成员变量，用来指向crt mutator.
    common::ThreadHolder *currentThreadHolder;
    uintptr_t stackAddress;
    uintptr_t stackBottom;
};

} // namespace mm
} // namespace kotlin

#endif // RUNTIME_MM_THREAD_DATA_H
