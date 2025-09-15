#include "common_components/common_runtime/hooks.h"
#include "common_interfaces/objects/base_object.h"
#include "common_interfaces/objects/base_state_word.h"

namespace common {

void processArrayInMark(void* state, void* objHeader);
void processObjectInMark(void* state, void* objHeader);

class KNStateWorld {
public:
    struct GCStateWord {
        common::StateWordType address_ : 60;
        common::StateWordType remainded_ : 4;
    };

    void SetForwardingPointerAfterExclusive(uintptr_t fwdPtr) {
        state_.address_ = fwdPtr;
    }

    uintptr_t GetForwardingPointerAfterExclusive() const {
        return state_.address_;
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
        reinterpret_cast<KNStateWorld*>(this)->SetForwardingPointerAfterExclusive(reinterpret_cast<uintptr_t>(fwdPtr));
    }

    BaseObject* GetForwardingPointerAfterExclusive() const {
        return reinterpret_cast<BaseObject*>(reinterpret_cast<const KNStateWorld*>(this)->GetForwardingPointerAfterExclusive());
    }
};

class KNBaseObjectOperator: public BaseObjectOperatorInterfaces {
public:
    // Get Object size.
    size_t GetSize(const BaseObject *object) const override;
    // Check is valid object.
    bool IsValidObject(const BaseObject *object) const override {
        // TODO
        return true;
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
