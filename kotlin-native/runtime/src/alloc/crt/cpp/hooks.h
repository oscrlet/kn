#include "common_components/common_runtime/hooks.h"
#include "common_interfaces/objects/base_object.h"
#include "common_interfaces/objects/base_state_word.h"

namespace common {

void processArrayInMark(void* state, void* objHeader);
void processObjectInMark(void* state, void* objHeader);

class KNStateWord {
public:
    struct GCStateWord {
        common::StateWordType address_ : 59;
        common::StateWordType valid_ : 1;
        common::StateWordType remainded_ : 4;
    };

    void SetForwardingPointerAfterExclusive(uintptr_t fwdPtr) {
        state_.address_ = fwdPtr;
    }

    uintptr_t GetForwardingPointerAfterExclusive() const {
        return state_.address_;
    }

    bool IsValid() const {
        return state_.valid_ == 1;
    }

    void SetValid(bool valid) {
        state_.valid_ = valid;
    }
private:
    union {
        GCStateWord state_;
        MAddress header_;
    };
};

class KNBaseObject: public BaseObject {
public:
    void SetForwardingPointerAfterExclusive(BaseObject *fwdPtr) {
        reinterpret_cast<KNStateWord*>(this)->SetForwardingPointerAfterExclusive(reinterpret_cast<uintptr_t>(fwdPtr));
    }
    BaseObject* GetForwardingPointerAfterExclusive() const {
        return reinterpret_cast<BaseObject*>(reinterpret_cast<const KNStateWord*>(this)->GetForwardingPointerAfterExclusive());
    }
    bool IsValid() const {
        return reinterpret_cast<const KNStateWord*>(this)->IsValid();
    }
    void SetValid(bool valid) {
        reinterpret_cast<KNStateWord*>(this)->SetValid(valid);
    }
};

class KNBaseObjectOperator: public BaseObjectOperatorInterfaces {
public:
    // Get Object size.
    size_t GetSize(const BaseObject *object) const override;
    // Check is valid object.
    bool IsValidObject(const BaseObject *object) const override {
        return reinterpret_cast<const KNBaseObject*>(object)->IsValid();
    }

    // Iterate object field.
    void ForEachRefField(const BaseObject *object, const RefFieldVisitor &visitor) const override;

    // Get forwarding pointer.
    BaseObject *GetForwardingPointer(const BaseObject *object) const override {
        // TODO:
        return reinterpret_cast<const KNBaseObject*>(object)->GetForwardingPointerAfterExclusive();
    }
    // Set forwarding pointer.
    void SetForwardingPointerAfterExclusive(BaseObject *object, BaseObject *fwdPtr) override {
        reinterpret_cast<KNBaseObject*>(object)->SetForwardingPointerAfterExclusive(fwdPtr);
    }
    virtual ~KNBaseObjectOperator() = default;
};

}; // common
