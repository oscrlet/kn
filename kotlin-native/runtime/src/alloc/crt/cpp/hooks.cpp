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

#include <mach/mach.h>

namespace kotlin {
bool is_valid_pointer(const void* addr) {
    if (addr == NULL) return false;

    mach_port_t task = mach_task_self();
    vm_size_t size = 1;  // 尝试读取1字节
    vm_address_t data;
    mach_msg_type_number_t dataCnt;

    kern_return_t ret = vm_read(task, (vm_address_t)addr, size, &data, &dataCnt);

    if (ret == KERN_SUCCESS) {
        vm_deallocate(task, data, size);  // 释放临时内存
        return true;
    }
    return false;
}

bool collectRoot(const common::RefFieldVisitor &visitorFunc, ObjHeader* &object) noexcept {
    if (!common::Heap::IsHeapAddress(object) || !reinterpret_cast<common::BaseObject* >(object)->IsValidObject()) {
        return false;
    }
    if (object->heap()) {
        visitorFunc(reinterpret_cast<common::RefField<>&>(object));
    } else {
        // Each permanent and stack object has own entry in the root set, so it's okay to only process objects in heap.
        // Traits::processInMark(markQueue, object);
        // RuntimeAssert(!object->has_meta_object(), "Non-heap object %p may not have an extra object data", object);
        // TODO: 这里的逻辑需要补充。
    }
    return true;
}

// void addStackRange(uintptr_t begin, uintptr_t end, const common::RefFieldVisitor &visitorFunc) {
//     // Ensure begin < end
//     if (begin > end) {
//         std::swap(begin, end);
//     }

//     // Align pointers to word boundary (assuming 8-byte alignment)
//     uintptr_t aligned_begin = (reinterpret_cast<uintptr_t>(begin) + 7) & ~7;
//     uintptr_t aligned_end = reinterpret_cast<uintptr_t>(end) & ~7;
//     uintptr_t shift = sizeof(uintptr_t) * 8 - 1;
//     uintptr_t mask = (((uintptr_t)1) << shift);

//     // Scan the stack and add potential pointers
//     for (uintptr_t addr = aligned_begin; addr < aligned_end; addr += 8) {
//         uintptr_t potential_ptr;
//         memcpy(&potential_ptr, reinterpret_cast<void*>(addr), sizeof(potential_ptr));

//         // Basic heuristic: consider it a pointer if it's not null and points to a
//         // reasonable memory range This is a simplified check - a real GC would
//         // have more sophisticated checks
//         if (potential_ptr != 0 && potential_ptr > 0x1000 &&
//             potential_ptr < mask) {
//             collectRoot(visitorFunc, reinterpret_cast<ObjHeader*>(potential_ptr));
//             //RuntimeLogInfo({ kotlin::logging::Tag::kGC }, "copyObj  addStackRange stack ptr %p", (void*)potential_ptr);
//             }
//     }
// }

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
        // printf frames.
    // printf("Print Frames before collectRoots:\n");
    uintptr_t frameSize = 8 * sizeof(uintptr_t);
    FrameOverlay *currentFrame = rootSet.stack_.currentFrame_;
    //PrintFrame(thread, frameSize);
    // uintptr_t fpStart = 0;
    // uintptr_t fpEnd = 0;
    // uintptr_t fp = 0;
    // for (auto value : mm::ThreadRootSet(thread)) {  // TODO: 需要改parameter的处理逻辑.
    //     if (collectRoot(visitorFunc, (value.object))) {
    //         switch (value.source) {
    //             case mm::ThreadRootSet::Source::kStack:
    //                 fp = reinterpret_cast<uintptr_t>(value.object);
    //                 if (fpStart == 0 || fp < fpStart) {
    //                     fpStart = fp;
    //                 }
    //                 if (fpEnd == 0 || fp > fpEnd) {
    //                     fpEnd = fp;
    //                 }
    //                 break;
    //             case mm::ThreadRootSet::Source::kTLS:
    //                 break;
    //         }
    //     }
    // }

    // if (fpStart > 0 && fpEnd > 0) {
    //     fpStart = (fpStart & (-4096));
    //     if (fpStart > 4096) {
    //         fpStart -= 4096;
    //     }

    //     fpEnd += 1024;
    //     fpEnd = (fpEnd & (-4096));
    //     fpEnd += 4096;
    //     //取栈底
    //     if (thread.getStackBottom() > fpEnd) {
    //         fpEnd = thread.getStackBottom();
    //     }
    //     addStackRange(fpStart, fpEnd, visitorFunc);
    // }

    // 加 50
    // printf("Print Frames during colllectRoots:\n");
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

// void collectRootSetGlobals(const common::RefFieldVisitor &visitorFunc) {
//     // TODO: Remove useless mm::GlobalRootSet abstraction.
//     auto rootSet = kotlin::mm::GlobalRootSet();
//     for (auto item = rootSet.begin(); item != rootSet.end(); ++item) {
//       //  printf("Run in collectRootSetGlobals: object: %p\n", (*item).object);
//         if (collectRoot2(visitorFunc, (*item).object)) {
//           //  printf("Run in collectRootSetGlobals: object: %p\n", (*item).object);
//         }
//     }
// }

// 重写KN中的CollectRootSet逻辑.
void collectRootSet(const common::RefFieldVisitor &visitorFunc) {
    for (auto& thread : mm::GlobalData::Instance().threadRegistry().LockForIter()) {
        // 这里的thread.Publish()还不知道是干什么的，先保留原有的逻辑.
        thread.Publish();
        collectRootSetForThread(visitorFunc, thread);
    }
    // collectRootSetGlobals(visitorFunc);
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
   const ObjHeader* objHeader = reinterpret_cast<const ObjHeader*>(object);
   if (objHeader->type_info() && objHeader->type_info()->IsArray()) {
       return kotlin::alloc::crtAllocatedHeapSize(const_cast<ObjHeader*>(objHeader));
   } else {
       return kotlin::alloc::allocatedHeapSize(const_cast<ObjHeader*>(reinterpret_cast<const ObjHeader*>(object)));
   }
}

void processFieldInMark(const RefFieldVisitor &visitor, ObjHeader* object, ObjHeader* &field) noexcept {
    if (common::Heap::IsHeapAddress(field)) {
        if (reinterpret_cast<BaseObject*>(field)->GetSize() != 0) {
            bool flag = true;
            (void)flag;
        }
        visitor(reinterpret_cast<common::RefField<>&>(field));
    }
}

void processArrayInMark(const RefFieldVisitor &visitor, ObjHeader *object) {
    ArrayHeader *arrayHeader = reinterpret_cast<ArrayHeader*>(object);
    kotlin::traverseArrayOfObjectsElements(arrayHeader, [=] (auto elemAccessor) noexcept {
       if (ObjHeader** elem = elemAccessor.direct().location()) {
            if (*elem) {
                processFieldInMark(visitor, arrayHeader->obj(), *elem);
            }
        }
    });
}

void processObjectInMark(const RefFieldVisitor &visitor, ObjHeader *object) {
    kotlin::traverseClassObjectFields(object, [=] (auto fieldAccessor) noexcept {
        if (ObjHeader** field = fieldAccessor.direct().location()) {
            if (*field) {
                processFieldInMark(visitor, object, *field);
            }
        }
    });
}

void KNBaseObjectOperator::ForEachRefField(const BaseObject *crtObject, const RefFieldVisitor &visitor) const {
    ObjHeader *object = const_cast<ObjHeader*>(reinterpret_cast<const ObjHeader*>(crtObject));
    auto process = object->type_info()->processObjectInMark;
    if (process == Kotlin_processArrayInMark) {
        processArrayInMark(visitor, object);
    } else if (process == Kotlin_processObjectInMark) {
        processObjectInMark(visitor, object);
    }
}

} // namespace common
