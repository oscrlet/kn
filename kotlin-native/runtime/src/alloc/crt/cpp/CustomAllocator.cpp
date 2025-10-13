/*
 * Copyright 2022 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

#include "CustomAllocator.hpp"

#if defined(__aarch64__)
#include <arm/types.h>
#endif
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <unistd.h>
#include <new>

#include "Common.h"
#include "CustomLogging.hpp"
#include "ExtraObjectData.hpp"
#include "ExtraObjectPage.hpp"
#include "GC.hpp"
#include "GCScheduler.hpp"
#include "KAssert.h"
#include "SingleObjectPage.hpp"
#include "NextFitPage.hpp"
#include "Memory.h"
#include "FixedBlockPage.hpp"
#include "GCApi.hpp"
#include "heap/allocator/region_space.h"
#include "hooks.h"

#include "common_interfaces/base_runtime.h"
#include "common_interfaces/thread/thread_holder.h"
#include "common_interfaces/heap/heap_allocator.h"
#include "common_components/heap/heap.h"
#include "common_components/heap/allocator/region_desc.h"
#include "macros.h"
#include "mutator/thread_local.h"

namespace common {
#ifndef __aarch64__
uintptr_t threadLocalReg = 0;
#endif
} // namespace common

namespace kotlin::alloc {

CustomAllocator::CustomAllocator(Heap& heap) noexcept : heap_(heap), nextFitPage_(nullptr), extraObjectPage_(nullptr) {
    CustomAllocInfo("CustomAllocator::CustomAllocator(heap)");
    memset(fixedBlockPages_, 0, sizeof(fixedBlockPages_));
}

CustomAllocator::~CustomAllocator() {
    heap_.AddToFinalizerQueue(std::move(finalizerQueue_));
}

#ifdef ENABLE_GC_FASTPATH
static NO_INLINE common::Address AllocFromCMCSlowPath(size_t size) {
    common::ThreadLocalRegisterAccessor tlr { .raw = common::threadLocalReg };
    auto allocPtr = common::HeapAllocator::AllocateInYoungOrHuge(size, common::LanguageType::DYNAMIC);
    auto mutator = reinterpret_cast<common::ThreadLocalData*>(tlr.data.threadLocalData)->mutator;
    common::UpdateThreadLocalDataReg(mutator);
    return allocPtr;
}
#endif // ENABLE_GC_FASTPATH

static ALWAYS_INLINE common::Address AllocFromCMC(size_t size) {
#ifndef ENABLE_GC_FASTPATH
    return common::HeapAllocator::AllocateInYoungOrHuge(size, common::LanguageType::DYNAMIC);
#else
    common::Address allocPtr;
    uintptr_t regionEnd;
    uintptr_t *regionPtr;
    size_t allocSize = common::RegionSpace::ToAllocatedSize(size);
    common::ThreadLocalRegisterAccessor tlr { .raw = common::threadLocalReg };
#ifdef __aarch64__
    asm volatile(
        "ldr %2, [%3]\n"    // get Alloc Buffer
        "ldr %2, [%2]\n"     // get region ptr
        "ldr %0, [%2]\n"     // get allocPtr
        "ldr %1, [%2, #8]\n" // get regionEnd
        : "=r"(allocPtr), "=r"(regionEnd), "=r"(regionPtr)
        : "r"(tlr.data.threadLocalData)
    );
#endif // __aarch64__
    auto endOfAlloc = allocPtr + allocSize;
    if (UNLIKELY(endOfAlloc > regionEnd)) {
        return AllocFromCMCSlowPath(size);
    }
#ifndef NDEBUG
    static size_t count = 0;
    ++count;
    std::cout << "FastAlloc: " << count << " times\n";
    allocPtr += allocSize;
    auto slowAlloc = common::HeapAllocator::AllocateInYoungOrHuge(size, common::LanguageType::DYNAMIC);

    if (allocPtr != slowAlloc) {
        std::cout << "FastAlloc: " << std::hex << allocPtr << " SlowAlloc " << slowAlloc << std::dec << " mismatch\n";
        std::cout << "allocBase: " << std::hex << allocPtr - allocSize << " allocEnd: " << regionEnd << std::dec
            << " size: " << size << " allocSize " << allocSize  << "\n";
        std::abort();
    }
    return slowAlloc;
#endif // NDEBUG
    *regionPtr = endOfAlloc;
    return allocPtr;
#endif // ENABLE_GC_FASTPATH
}

ALWAYS_INLINE ObjHeader* CustomAllocator::CreateObject(const TypeInfo* typeInfo) noexcept {
    RuntimeAssert(!typeInfo->IsArray(), "Must not be an array");
    auto descriptor = CustomHeapObject::descriptorFrom(typeInfo);
    // 在这里接入Common Runtime 的Allocate.
    auto& heapObject = *descriptor.construct(reinterpret_cast<uint8_t*>(AllocFromCMC(descriptor.size())));
    // auto& heapObject = *descriptor.construct(Allocate(descriptor.size()));
    ObjHeader* object = heapObject.object();
    if (typeInfo->flags_ & TF_HAS_FINALIZER) {
        auto* extraObject = CreateExtraObjectDataForObject(object, typeInfo);
        object->typeInfoOrMeta_ = reinterpret_cast<TypeInfo*>(extraObject);
        CustomAllocDebug("CustomAllocator: %p gets extraObject %p", object, extraObject);
        CustomAllocDebug("CustomAllocator: %p->BaseObject == %p", extraObject, extraObject->GetBaseObject());
    } else {
        object->typeInfoOrMeta_ = const_cast<TypeInfo*>(typeInfo);
    }
    reinterpret_cast<common::KNBaseObject *>(object)->SetValid(true);
    return object;
}

ALWAYS_INLINE ArrayHeader* CustomAllocator::CreateArray(const TypeInfo* typeInfo, uint32_t count) noexcept {
    CustomAllocDebug("CustomAllocator@%p::CreateArray(%d)", this ,count);
    RuntimeAssert(typeInfo->IsArray(), "Must be an array");
    auto descriptor = CustomHeapArray::descriptorFrom(typeInfo, count);
    auto& heapArray = *descriptor.construct(reinterpret_cast<uint8_t*>(AllocFromCMC(descriptor.size())));
    // auto& heapArray = *descriptor.construct(Allocate(descriptor.size()));
    ArrayHeader* array = heapArray.array();
    array->typeInfoOrMeta_ = const_cast<TypeInfo*>(typeInfo);
    array->count_ = count;
    reinterpret_cast<common::KNBaseObject *>(array)->SetValid(true);
    return array;
}

ALWAYS_INLINE mm::ExtraObjectData* CustomAllocator::CreateExtraObjectDataForObject(
        ObjHeader* baseObject, const TypeInfo* info) noexcept {
    auto* extraObjectMemory = AllocateExtraObject();
    return new (extraObjectMemory) mm::ExtraObjectData(baseObject, info);
}

FinalizerQueue CustomAllocator::ExtractFinalizerQueue() noexcept {
    return std::move(finalizerQueue_);
}

void CustomAllocator::PrepareForGC() noexcept {
    CustomAllocInfo("CustomAllocator@%p::PrepareForGC()", this);
    nextFitPage_ = nullptr;
    memset(fixedBlockPages_, 0, sizeof(fixedBlockPages_));
    extraObjectPage_ = nullptr;
}

// static
size_t CustomAllocator::GetAllocatedHeapSize(ObjHeader* object) noexcept {
    return CustomHeapObject::from(object).size();
}

ALWAYS_INLINE uint8_t* CustomAllocator::Allocate(uint64_t size) noexcept {
    RuntimeAssert(size, "CustomAllocator::Allocate cannot allocate 0 bytes");
    CustomAllocDebug("CustomAllocator::Allocate(%" PRIu64 ")", size);
    uint64_t cellCount = (size + sizeof(Cell) - 1) / sizeof(Cell);
    if (cellCount <= FixedBlockPage::MAX_BLOCK_SIZE) {
        return AllocateInFixedBlockPage(cellCount);
    } else if (cellCount > NextFitPage::maxBlockSize()) {
        return AllocateInSingleObjectPage(cellCount);
    } else {
        return AllocateInNextFitPage(cellCount);
    }
}

uint8_t* CustomAllocator::AllocateInSingleObjectPage(uint64_t cellCount) noexcept {
    CustomAllocDebug("CustomAllocator::AllocateInSingleObjectPage(%" PRIu64 ")", cellCount);
    uint8_t* block = heap_.GetSingleObjectPage(cellCount, finalizerQueue_)->TryAllocate();
    return block;
}

ALWAYS_INLINE uint8_t* CustomAllocator::AllocateInNextFitPage(uint32_t cellCount) noexcept {
    CustomAllocDebug("CustomAllocator::AllocateInNextFitPage(%u)", cellCount);
    if (nextFitPage_) {
        uint8_t* block = nextFitPage_->TryAllocate(cellCount);
        if (block) return block;
    }
    return AllocateInNextFitPageSlowPath(cellCount);
}

NO_INLINE uint8_t* CustomAllocator::AllocateInNextFitPageSlowPath(uint32_t cellCount) noexcept {
    CustomAllocDebug("Failed to allocate in curPage");
    while (true) {
        nextFitPage_ = heap_.GetNextFitPage(cellCount, finalizerQueue_);
        uint8_t* block = nextFitPage_->TryAllocate(cellCount);
        if (block) return block;
    }
}

ALWAYS_INLINE uint8_t* CustomAllocator::AllocateInFixedBlockPage(uint32_t cellCount) noexcept {
    CustomAllocDebug("CustomAllocator::AllocateInFixedBlockPage(%u)", cellCount);
    FixedBlockPage* page = fixedBlockPages_[cellCount];
    if (page) {
        uint8_t* block = page->TryAllocate(cellCount);
        if (block) return block;
    }
    return AllocateInFixedBlockPageSlowPath(page, cellCount);
}

NO_INLINE uint8_t* CustomAllocator::AllocateInFixedBlockPageSlowPath(FixedBlockPage* overflownPage, uint32_t cellCount) noexcept {
    CustomAllocDebug("Failed to allocate in current FixedBlockPage(%p)", overflownPage);
    if (overflownPage != nullptr) {
        overflownPage->OnPageOverflow();
    }
    while (auto* page = heap_.GetFixedBlockPage(cellCount, finalizerQueue_)) {
        uint8_t* block = page->TryAllocate(cellCount);
        if (block) {
            fixedBlockPages_[cellCount] = page;
            return block;
        }
    }
    return nullptr;
}

ALWAYS_INLINE uint8_t* CustomAllocator::AllocateExtraObject() noexcept {
    CustomAllocDebug("CustomAllocator::AllocateExtraObject()");
    ExtraObjectPage* page = extraObjectPage_;
    if (page) {
        uint8_t* block = page->TryAllocate();
        if (block) return block;
    }
    return AllocateExtraObjectSlowPath();
}

NO_INLINE uint8_t* CustomAllocator::AllocateExtraObjectSlowPath() noexcept {
    CustomAllocDebug("Failed to allocate in current ExtraObjectPage");
    while (ExtraObjectPage* page = heap_.GetExtraObjectPage(finalizerQueue_)) {
        uint8_t* block = page->TryAllocate();
        if (block) {
            extraObjectPage_ = page;
            return block;
        }
    }
    return nullptr;
}

} // namespace kotlin::alloc
