/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hooks.h"

#include "common_components/common_runtime/hooks.h"
#include "common_components/heap/collector/collector.h"
#include "common_interfaces/objects/base_object.h"
#include "common_components/heap/heap.h"

// TOOD: 下面的include要进行重写
#include "ExtraObjectData.hpp"
#include "FinalizerHooks.hpp"
#include "GlobalData.hpp"
#include "GCStatistics.hpp"
#include "Logging.hpp"
#include "Memory.h"
#include "ObjectOps.hpp"
#include "ObjectTraversal.hpp"
#include "RootSet.hpp"
#include "Runtime.h"
#include "SpecialRefRegistry.hpp"
#include "ThreadData.hpp"
#include "Types.h"

#include <cstdint>
#include <stdio.h>
#include <sstream>


namespace kotlin {
bool collectRoot(const common::RefFieldVisitor &visitorFunc, ObjHeader* &object) noexcept {
    auto refField = reinterpret_cast<common::RefField<>&>(object);
    if (!common::Heap::IsHeapAddress(object) || !refField.GetTargetObject()->IsValidObject()) {
        return false;
    }
    // printf("The collectRoot is %p\n", object);
    visitorFunc(reinterpret_cast<common::RefField<>&>(object));
    // Each permanent and stack object has own entry in the root set, so it's okay to only process objects in heap.
    // Traits::processInMark(markQueue, object);
    // RuntimeAssert(!object->has_meta_object(), "Non-heap object %p may not have an extra object data", object);
    // TODO: 这里的逻辑需要补充。
    return true;
}

void PrintFrame(kotlin::mm::ThreadData& thread, int frameSize) {
    // auto rootSet2 = kotlin::mm::ThreadRootSet(thread);
    // FrameOverlay * currentFrame = rootSet2.stack_.currentFrame_;
    // while(currentFrame != nullptr) {
    //     // printf("**************\n");
    //     for(int i = 0; i < frameSize; i++) {
    //         printf("%p(%p) ", *(reinterpret_cast<ObjHeader**>(currentFrame) + i), (reinterpret_cast<ObjHeader**>(currentFrame) + i));
    //     }
    //     printf("\n");
    //     currentFrame = currentFrame->previous;
    // }
}

void collectRootSetForThread(const common::RefFieldVisitor &visitorFunc, kotlin::mm::ThreadData& thread) {
    // TODO: Remove useless mm::ThreadRootSet abstraction.
    auto rootSet = kotlin::mm::ThreadRootSet(thread);
    uintptr_t frameSize = 50;
    FrameOverlay *currentFrame = rootSet.stack_.currentFrame_;
    currentFrame = rootSet.stack_.currentFrame_;
    assert(currentFrame);
    uintptr_t minFrame =  UINTPTR_MAX;
    uintptr_t maxFrame = 0;
    while (currentFrame != nullptr) {
        if ((uintptr_t)currentFrame < minFrame) {
            minFrame = (uintptr_t)currentFrame;
        }
        if ((uintptr_t)currentFrame > maxFrame) {
            maxFrame = (uintptr_t)currentFrame;
        }
        currentFrame = currentFrame->previous;
    }
    minFrame -= (frameSize * sizeof(uintptr_t));
    maxFrame += (frameSize * sizeof(uintptr_t));
    for (auto i = (uintptr_t)minFrame; i <= (uintptr_t)maxFrame; i += sizeof(uintptr_t)) {
        ObjHeader** tmpObj = (reinterpret_cast<ObjHeader**>(i));
        collectRoot(visitorFunc, *tmpObj);
    }
}

void collectRootSetGlobals(const common::RefFieldVisitor &visitorFunc) {
    // TODO: Remove useless mm::GlobalRootSet abstraction.
    for (auto value : kotlin::mm::GlobalRootSet()) {
        collectRoot(visitorFunc, value.object);
    }
}

// 重写KN中的CollectRootSet逻辑.
void collectRootSet(const common::RefFieldVisitor &visitorFunc) {
    for (auto& thread : mm::GlobalData::Instance().threadRegistry().LockForIter()) {
        thread.Publish();
        collectRootSetForThread(visitorFunc, thread);
    }
   collectRootSetGlobals(visitorFunc);
}
}; // namespace kotlin

namespace common {
void SetBaseAddress(uintptr_t base) {}
void JitFortUnProt(size_t size, void* base) {}
void FillFreeObject(void *object, size_t size) {

}
void VisitDynamicGlobalRoots(const RefFieldVisitor &visitorFunc) {}
void VisitDynamicLocalRoots(const RefFieldVisitor &visitor) {}

void VisitBaseRoots(const RefFieldVisitor &visitorFunc) {
    kotlin::collectRootSet(visitorFunc);
}

void VisitDynamicConcurrentRoots(const RefFieldVisitor &visitorFunc) {}
void VisitDynamicWeakGlobalRoots(const common::WeakRefFieldVisitor &visitorFunc) {}
void VisitDynamicWeakGlobalRootsOld(const common::WeakRefFieldVisitor &visitorFunc) {}
void VisitDynamicWeakLocalRoots(const WeakRefFieldVisitor &visitorFunc) {}
void VisitDynamicPreforwardRoots(const RefFieldVisitor &visitorFunc) {}
void VisitDynamicThreadRoot(const RefFieldVisitor &visitorFunc, void *vm) {}
void VisitDynamicWeakThreadRoot(const WeakRefFieldVisitor &visitorFunc, void *vm) {}
void VisitDynamicThreadPreforwardRoot(const RefFieldVisitor &visitorFunc, void *vm) {}
void InvokeSharedNativePointerCallbacks() {}
void SweepThreadLocalJitFort() {}
void MarkThreadLocalJitFortInstalled(void* thread, void* machineCode) {}
void SynchronizeGCPhaseToJSThread(void *jsThread, GCPhase gcPhase) {}
void JSGCCallback(void *ecmaVM) {}
bool IsPostForked() { return true; }
void VisitJSThread(void *jsThread, CommonRootVisitor visitor) {}

bool IsMachineCodeObject(uintptr_t objPtr)
{
    return false;
}

size_t KNBaseObjectOperator::GetSize(const BaseObject *object) const {
    // NOTE: On bytedance the size of a string is a bit different.
    // But for blue-zone kotlin we can just delegate it to allocatedHeapSize
    return kotlin::alloc::allocatedHeapSize(const_cast<ObjHeader*>(reinterpret_cast<const ObjHeader*>(object)));
}

void processFieldInMark(const RefFieldVisitor &visitor, ObjHeader* object, ObjHeader* &field) noexcept {
    if (common::Heap::IsHeapAddress(field)) {
        visitor(reinterpret_cast<common::RefField<>&>(field));
    }
}

void processArrayInMark(const RefFieldVisitor &visitor, ObjHeader *object) {
    std::abort();
    // auto *objHeader = reinterpret_cast<ObjHeader*>(object);
    // // NB: note that we should not use traverseArrayOfObjectsElements because we should avoid 
    // // traversing primitive fields in the array
    // kotlin::traverseObjectFields(objHeader, [=] (auto elemAccessor) noexcept {
    //    if (ObjHeader** elem = elemAccessor.direct().location()) {
    //         if (*elem) {
    //             processFieldInMark(visitor, objHeader, *elem);
    //         }
    //     }
    // });
}

void processObjectInMark(const RefFieldVisitor &visitor, ObjHeader *object) {
    std::abort();
    // kotlin::traverseClassObjectFields(object, [=] (auto fieldAccessor) noexcept {
    //     if (ObjHeader** field = fieldAccessor.direct().location()) {
    //         if (*field) {
    //             processFieldInMark(visitor, object, *field);
    //         }
    //     }
    // });
}

void KNBaseObjectOperator::ForEachRefField(const BaseObject *crtObject, const RefFieldVisitor &visitor) const {
    auto* objHeader = const_cast<ObjHeader*>(reinterpret_cast<const ObjHeader*>(crtObject));
    kotlin::traverseObjectFields(objHeader, [=](auto elemAccessor) noexcept {
        if (ObjHeader** elem = elemAccessor.direct().location()) {
            if (*elem) {
                processFieldInMark(visitor, objHeader, *elem);
            }
        }
    });
}

} // namespace common
