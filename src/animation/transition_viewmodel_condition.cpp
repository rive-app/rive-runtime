#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/transition_property_component_comparator.hpp"
#include "rive/artboard.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/generated/custom_property_enum_base.hpp"
#include "rive/generated/custom_property_trigger_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_artboard_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_asset_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_enum_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_trigger_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/importers/state_transition_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/component_dirt.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

#include <vector>

using namespace rive;

class ConditionComparisonNone : public ConditionComparison
{
public:
    ConditionComparisonNone() : ConditionComparison(nullptr) {}
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return false;
    }
};

class ConditionComparisonSelf : public ConditionComparison
{
public:
    ConditionComparisonSelf(BindableProperty* property) :
        ConditionComparison(nullptr), m_bindableProperty(property)
    {}
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        auto dataBind =
            stateMachineInstance->bindableDataBindToTarget(bindableInstance);
        if (dataBind != nullptr)
        {
            auto source = dataBind->source();
            if (source != nullptr && source->hasChanged() &&
                !source->isUsedInLayer(layerInstance))
            {
                return true;
            }
        }
        return false;
    }

private:
    BindableProperty* m_bindableProperty;
};
class ConditionComparisonNumber : public ConditionComparison
{
public:
    ConditionComparisonNumber(ConditionComparandNumber* left,
                              ConditionComparandNumber* right,
                              ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonNumber()
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return compareNumbers(m_leftComparand->value(stateMachineInstance),
                              m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandNumber* m_leftComparand = nullptr;
    ConditionComparandNumber* m_rightComparand = nullptr;
};
class ConditionComparisonBoolean : public ConditionComparison
{
public:
    ConditionComparisonBoolean(ConditionComparandBoolean* left,
                               ConditionComparandBoolean* right,
                               ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonBoolean() override
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return compareBooleans(m_leftComparand->value(stateMachineInstance),
                               m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandBoolean* m_leftComparand = nullptr;
    ConditionComparandBoolean* m_rightComparand = nullptr;
};

class ConditionComparisonString : public ConditionComparison
{
public:
    ConditionComparisonString(ConditionComparandString* left,
                              ConditionComparandString* right,
                              ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonString()
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return compareStrings(m_leftComparand->value(stateMachineInstance),
                              m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandString* m_leftComparand = nullptr;
    ConditionComparandString* m_rightComparand = nullptr;
};

class ConditionComparisonColor : public ConditionComparison
{
public:
    ConditionComparisonColor(ConditionComparandColor* left,
                             ConditionComparandColor* right,
                             ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonColor()
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return compareColors(m_leftComparand->value(stateMachineInstance),
                             m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandColor* m_leftComparand = nullptr;
    ConditionComparandColor* m_rightComparand = nullptr;
};

class ConditionComparisonEnum : public ConditionComparison
{
public:
    ConditionComparisonEnum(ConditionComparandUint32* left,
                            ConditionComparandUint32* right,
                            ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonEnum()
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return compareUints32(m_leftComparand->value(stateMachineInstance),
                              m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandUint32* m_leftComparand = nullptr;
    ConditionComparandUint32* m_rightComparand = nullptr;
};

class ConditionComparisonUint32 : public ConditionComparison
{
public:
    ConditionComparisonUint32(ConditionComparandUint32* left,
                              ConditionComparandUint32* right,
                              ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonUint32()
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return compareUints32(m_leftComparand->value(stateMachineInstance),
                              m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandUint32* m_leftComparand = nullptr;
    ConditionComparandUint32* m_rightComparand = nullptr;
};

class ConditionComparisonViewModel : public ConditionComparison
{
public:
    ConditionComparisonViewModel(ConditionComparandViewModel* left,
                                 ConditionComparandViewModel* right,
                                 ConditionOperation* operation) :
        ConditionComparison(operation),
        m_leftComparand(left),
        m_rightComparand(right)
    {}
    ~ConditionComparisonViewModel()
    {
        delete m_leftComparand;
        delete m_rightComparand;
    }
    bool compare(const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override
    {
        return comparePointers(m_leftComparand->value(stateMachineInstance),
                               m_rightComparand->value(stateMachineInstance));
    }

private:
    ConditionComparandViewModel* m_leftComparand = nullptr;
    ConditionComparandViewModel* m_rightComparand = nullptr;
};

namespace
{

Core* resolveComponentTarget(const StateMachineInstance* smi,
                             const TransitionPropertyComponentComparator* comp)
{
    if (smi == nullptr || comp == nullptr)
    {
        return nullptr;
    }
    Artboard* artboard = smi->artboard();
    if (artboard == nullptr)
    {
        return nullptr;
    }
    Core* target = artboard->resolve(comp->objectId());
    if (target == nullptr ||
        !CoreRegistry::objectSupportsProperty(target, comp->propertyKey()))
    {
        return nullptr;
    }
    return target;
}

enum class ComponentComparandKind
{
    NumberDouble,
    NumberFromUint,
    Boolean,
    String,
    Color,
    Enum,
    Trigger,
    Asset,
    Artboard,
    ViewModel,
};

bool describeComponentSide(const TransitionPropertyComponentComparator* comp,
                           ComponentComparandKind* outKind)
{
    int fieldId = CoreRegistry::propertyFieldId((int)comp->propertyKey());
    if (fieldId < 0)
    {
        return false;
    }
    const uint32_t pk = comp->propertyKey();
    switch (fieldId)
    {
        case CoreDoubleType::id:
            *outKind = ComponentComparandKind::NumberDouble;
            return true;
        case CoreBoolType::id:
            *outKind = ComponentComparandKind::Boolean;
            return true;
        case CoreStringType::id:
            *outKind = ComponentComparandKind::String;
            return true;
        case CoreColorType::id:
            *outKind = ComponentComparandKind::Color;
            return true;
        case CoreUintType::id:
            if (pk == CustomPropertyEnumBase::propertyValuePropertyKey ||
                pk == ViewModelInstanceEnumBase::propertyValuePropertyKey)
            {
                *outKind = ComponentComparandKind::Enum;
                return true;
            }
            if (pk == CustomPropertyTriggerBase::propertyValuePropertyKey ||
                pk == ViewModelInstanceTriggerBase::propertyValuePropertyKey)
            {
                *outKind = ComponentComparandKind::Trigger;
                return true;
            }
            if (pk == ViewModelInstanceAssetBase::propertyValuePropertyKey)
            {
                *outKind = ComponentComparandKind::Asset;
                return true;
            }
            if (pk == ViewModelInstanceArtboardBase::propertyValuePropertyKey)
            {
                *outKind = ComponentComparandKind::Artboard;
                return true;
            }
            if (pk == ViewModelInstanceViewModelBase::propertyValuePropertyKey)
            {
                *outKind = ComponentComparandKind::ViewModel;
                return true;
            }
            *outKind = ComponentComparandKind::NumberFromUint;
            return true;
        default:
            return false;
    }
}

bool componentKindsCompatible(ComponentComparandKind a,
                              ComponentComparandKind b)
{
    auto isNumberKind = [](ComponentComparandKind k) {
        return k == ComponentComparandKind::NumberDouble ||
               k == ComponentComparandKind::NumberFromUint;
    };
    if (isNumberKind(a) && isNumberKind(b))
    {
        return true;
    }
    return a == b;
}

ConditionComparandNumber* makeComponentNumberComparand(
    TransitionPropertyComponentComparator* comp,
    ComponentComparandKind kind)
{
    if (kind == ComponentComparandKind::NumberDouble)
    {
        return new ConditionComparandComponentCoreNumber(comp);
    }
    return new ConditionComparandComponentCoreUintAsNumber(comp);
}

bool describeViewModelBindableKind(BindableProperty* bindable,
                                   ComponentComparandKind* outKind)
{
    if (bindable == nullptr)
    {
        return false;
    }
    switch (bindable->coreType())
    {
        case BindablePropertyNumber::typeKey:
            *outKind = ComponentComparandKind::NumberDouble;
            return true;
        case BindablePropertyInteger::typeKey:
            *outKind = ComponentComparandKind::NumberFromUint;
            return true;
        case BindablePropertyBoolean::typeKey:
            *outKind = ComponentComparandKind::Boolean;
            return true;
        case BindablePropertyString::typeKey:
            *outKind = ComponentComparandKind::String;
            return true;
        case BindablePropertyColor::typeKey:
            *outKind = ComponentComparandKind::Color;
            return true;
        case BindablePropertyEnum::typeKey:
            *outKind = ComponentComparandKind::Enum;
            return true;
        case BindablePropertyTrigger::typeKey:
            *outKind = ComponentComparandKind::Trigger;
            return true;
        case BindablePropertyAsset::typeKey:
            *outKind = ComponentComparandKind::Asset;
            return true;
        case BindablePropertyArtboard::typeKey:
            *outKind = ComponentComparandKind::Artboard;
            return true;
        case BindablePropertyViewModel::typeKey:
            *outKind = ComponentComparandKind::ViewModel;
            return true;
        default:
            return false;
    }
}

enum class ComparatorSide
{
    Left,
    Right,
};

// Artboard properties and literal values only participate on the left / right
// respectively (matches historical initialize() support).
void appendComparableKinds(TransitionComparator* comparator,
                           ComparatorSide side,
                           std::vector<ComponentComparandKind>& out)
{
    if (comparator->is<TransitionPropertyArtboardComparator>())
    {
        if (side == ComparatorSide::Left)
        {
            out.push_back(ComponentComparandKind::NumberDouble);
        }
        return;
    }
    if (comparator->is<TransitionPropertyComponentComparator>())
    {
        ComponentComparandKind k;
        if (describeComponentSide(
                comparator->as<TransitionPropertyComponentComparator>(),
                &k))
        {
            out.push_back(k);
        }
        return;
    }
    if (comparator->is<TransitionPropertyViewModelComparator>())
    {
        ComponentComparandKind k;
        if (describeViewModelBindableKind(
                comparator->as<TransitionPropertyViewModelComparator>()
                    ->bindableProperty(),
                &k))
        {
            out.push_back(k);
        }
        return;
    }
    if (side != ComparatorSide::Right)
    {
        return;
    }
    if (comparator->is<TransitionValueNumberComparator>())
    {
        out.push_back(ComponentComparandKind::NumberDouble);
        return;
    }
    if (comparator->is<TransitionValueBooleanComparator>())
    {
        out.push_back(ComponentComparandKind::Boolean);
        return;
    }
    if (comparator->is<TransitionValueStringComparator>())
    {
        out.push_back(ComponentComparandKind::String);
        return;
    }
    if (comparator->is<TransitionValueColorComparator>())
    {
        out.push_back(ComponentComparandKind::Color);
        return;
    }
    if (comparator->is<TransitionValueEnumComparator>())
    {
        out.push_back(ComponentComparandKind::Enum);
        return;
    }
    if (comparator->is<TransitionValueAssetComparator>())
    {
        out.push_back(ComponentComparandKind::Asset);
        return;
    }
    if (comparator->is<TransitionValueArtboardComparator>())
    {
        out.push_back(ComponentComparandKind::Artboard);
        return;
    }
    // Uint32 comparison vs component triggers; VM trigger + value trigger uses
    // ConditionComparisonSelf and is handled before kind intersection.
    if (comparator->is<TransitionValueTriggerComparator>())
    {
        out.push_back(ComponentComparandKind::Trigger);
        return;
    }
}

// Picks the first (lk, rk) pair such that componentKindsCompatible(lk, rk),
// scanning leftKinds in order then rightKinds (deterministic tie-break).
bool intersectCompatibleKinds(
    const std::vector<ComponentComparandKind>& leftKinds,
    const std::vector<ComponentComparandKind>& rightKinds,
    ComponentComparandKind* outLeftKind,
    ComponentComparandKind* outRightKind)
{
    for (ComponentComparandKind lk : leftKinds)
    {
        for (ComponentComparandKind rk : rightKinds)
        {
            if (componentKindsCompatible(lk, rk))
            {
                *outLeftKind = lk;
                *outRightKind = rk;
                return true;
            }
        }
    }
    return false;
}

enum class ComparisonShape
{
    Number,
    Boolean,
    String,
    Color,
    Enum,
    Uint32,
    ViewModel,
};

// Maps compatible kinds to ConditionComparison wrapper. Numeric: any
// NumberDouble in the pair uses float compare (Number); both NumberFromUint
// uses exact uint32 compare (Uint32), including component+component generic
// uint fields and VM integer pairs.
bool resolveComparisonShape(ComponentComparandKind lk,
                            ComponentComparandKind rk,
                            ComparisonShape* outShape)
{
    if (!componentKindsCompatible(lk, rk))
    {
        return false;
    }
    auto isNumberKind = [](ComponentComparandKind k) {
        return k == ComponentComparandKind::NumberDouble ||
               k == ComponentComparandKind::NumberFromUint;
    };
    if (isNumberKind(lk) && isNumberKind(rk))
    {
        if (lk == ComponentComparandKind::NumberFromUint &&
            rk == ComponentComparandKind::NumberFromUint)
        {
            *outShape = ComparisonShape::Uint32;
            return true;
        }
        *outShape = ComparisonShape::Number;
        return true;
    }
    if (lk != rk)
    {
        return false;
    }
    switch (lk)
    {
        case ComponentComparandKind::Boolean:
            *outShape = ComparisonShape::Boolean;
            return true;
        case ComponentComparandKind::String:
            *outShape = ComparisonShape::String;
            return true;
        case ComponentComparandKind::Color:
            *outShape = ComparisonShape::Color;
            return true;
        case ComponentComparandKind::Enum:
            *outShape = ComparisonShape::Enum;
            return true;
        case ComponentComparandKind::Trigger:
        case ComponentComparandKind::Asset:
        case ComponentComparandKind::Artboard:
            *outShape = ComparisonShape::Uint32;
            return true;
        case ComponentComparandKind::ViewModel:
            *outShape = ComparisonShape::ViewModel;
            return true;
        default:
            return false;
    }
}

struct ComparandSlot
{
    ConditionComparandNumber* number = nullptr;
    ConditionComparandBoolean* boolean = nullptr;
    ConditionComparandString* string = nullptr;
    ConditionComparandColor* color = nullptr;
    ConditionComparandUint32* uint32 = nullptr;
    ConditionComparandViewModel* viewModel = nullptr;
};

void clearComparandSlot(ComparandSlot& s)
{
    delete s.number;
    delete s.boolean;
    delete s.string;
    delete s.color;
    delete s.uint32;
    delete s.viewModel;
    s = ComparandSlot{};
}

// Builds one comparand for `c` and kind `k` given the resolved comparison.
bool makeComparand(TransitionComparator* c,
                   ComponentComparandKind k,
                   ComparisonShape shape,
                   ComparandSlot* slot)
{
    switch (shape)
    {
        case ComparisonShape::Number:
        {
            if (c->is<TransitionPropertyArtboardComparator>())
            {
                slot->number = new ConditionComparandArtboardProperty(
                    c->as<TransitionPropertyArtboardComparator>());
                return true;
            }
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr)
                {
                    return false;
                }
                if (bp->is<BindablePropertyNumber>())
                {
                    slot->number = new ConditionComparandNumberBindable(
                        bp->as<BindablePropertyNumber>());
                    return true;
                }
                if (bp->is<BindablePropertyInteger>())
                {
                    slot->number = new ConditionComparandNumberBindableInteger(
                        bp->as<BindablePropertyInteger>());
                    return true;
                }
                return false;
            }
            if (c->is<TransitionValueNumberComparator>())
            {
                slot->number = new ConditionComparandNumberValue(
                    c->as<TransitionValueNumberComparator>());
                return true;
            }
            if (c->is<TransitionPropertyComponentComparator>())
            {
                auto* comp = c->as<TransitionPropertyComponentComparator>();
                if (k == ComponentComparandKind::NumberDouble)
                {
                    slot->number =
                        new ConditionComparandComponentCoreNumber(comp);
                    return true;
                }
                if (k == ComponentComparandKind::NumberFromUint)
                {
                    slot->number = makeComponentNumberComparand(comp, k);
                    return true;
                }
                return false;
            }
            return false;
        }
        case ComparisonShape::Boolean:
        {
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr || !bp->is<BindablePropertyBoolean>())
                {
                    return false;
                }
                slot->boolean = new ConditionComparandBooleanBindable(
                    bp->as<BindablePropertyBoolean>());
                return true;
            }
            if (c->is<TransitionValueBooleanComparator>())
            {
                slot->boolean = new ConditionComparandBooleanValue(
                    c->as<TransitionValueBooleanComparator>());
                return true;
            }
            if (c->is<TransitionPropertyComponentComparator>())
            {
                slot->boolean = new ConditionComparandComponentCoreBoolean(
                    c->as<TransitionPropertyComponentComparator>());
                return true;
            }
            return false;
        }
        case ComparisonShape::String:
        {
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr || !bp->is<BindablePropertyString>())
                {
                    return false;
                }
                slot->string = new ConditionComparandStringBindable(
                    bp->as<BindablePropertyString>());
                return true;
            }
            if (c->is<TransitionValueStringComparator>())
            {
                slot->string = new ConditionComparandStringValue(
                    c->as<TransitionValueStringComparator>());
                return true;
            }
            if (c->is<TransitionPropertyComponentComparator>())
            {
                slot->string = new ConditionComparandComponentCoreString(
                    c->as<TransitionPropertyComponentComparator>());
                return true;
            }
            return false;
        }
        case ComparisonShape::Color:
        {
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr || !bp->is<BindablePropertyColor>())
                {
                    return false;
                }
                slot->color = new ConditionComparandColorBindable(
                    bp->as<BindablePropertyColor>());
                return true;
            }
            if (c->is<TransitionValueColorComparator>())
            {
                slot->color = new ConditionComparandColorValue(
                    c->as<TransitionValueColorComparator>());
                return true;
            }
            if (c->is<TransitionPropertyComponentComparator>())
            {
                slot->color = new ConditionComparandComponentCoreColor(
                    c->as<TransitionPropertyComponentComparator>());
                return true;
            }
            return false;
        }
        case ComparisonShape::Enum:
        {
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr || !bp->is<BindablePropertyEnum>())
                {
                    return false;
                }
                slot->uint32 = new ConditionComparandEnumBindable(
                    bp->as<BindablePropertyEnum>());
                return true;
            }
            if (c->is<TransitionValueEnumComparator>())
            {
                slot->uint32 = new ConditionComparandEnumValue(
                    c->as<TransitionValueEnumComparator>());
                return true;
            }
            if (c->is<TransitionPropertyComponentComparator>())
            {
                slot->uint32 = new ConditionComparandComponentCoreUint(
                    c->as<TransitionPropertyComponentComparator>());
                return true;
            }
            return false;
        }
        case ComparisonShape::Uint32:
        {
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr)
                {
                    return false;
                }
                if (k == ComponentComparandKind::NumberFromUint &&
                    bp->is<BindablePropertyInteger>())
                {
                    slot->uint32 = new ConditionComparandIntegerBindable(
                        bp->as<BindablePropertyInteger>());
                    return true;
                }
                if (k == ComponentComparandKind::Trigger &&
                    bp->is<BindablePropertyTrigger>())
                {
                    slot->uint32 = new ConditionComparandTriggerBindable(
                        bp->as<BindablePropertyTrigger>());
                    return true;
                }
                if (k == ComponentComparandKind::Asset &&
                    bp->is<BindablePropertyAsset>())
                {
                    slot->uint32 = new ConditionComparandAssetBindable(
                        bp->as<BindablePropertyAsset>());
                    return true;
                }
                if (k == ComponentComparandKind::Artboard &&
                    bp->is<BindablePropertyArtboard>())
                {
                    slot->uint32 = new ConditionComparandArtboardBindable(
                        bp->as<BindablePropertyArtboard>());
                    return true;
                }
                return false;
            }
            if (c->is<TransitionValueTriggerComparator>())
            {
                slot->uint32 = new ConditionComparandTriggerValue(
                    c->as<TransitionValueTriggerComparator>());
                return true;
            }
            if (c->is<TransitionValueAssetComparator>())
            {
                slot->uint32 = new ConditionComparandAssetValue(
                    c->as<TransitionValueAssetComparator>());
                return true;
            }
            if (c->is<TransitionValueArtboardComparator>())
            {
                slot->uint32 = new ConditionComparandArtboardValue(
                    c->as<TransitionValueArtboardComparator>());
                return true;
            }
            if (c->is<TransitionPropertyComponentComparator>())
            {
                slot->uint32 = new ConditionComparandComponentCoreUint(
                    c->as<TransitionPropertyComponentComparator>());
                return true;
            }
            return false;
        }
        case ComparisonShape::ViewModel:
        {
            if (k != ComponentComparandKind::ViewModel)
            {
                return false;
            }
            if (c->is<TransitionPropertyViewModelComparator>())
            {
                auto* bp = c->as<TransitionPropertyViewModelComparator>()
                               ->bindableProperty();
                if (bp == nullptr || !bp->is<BindablePropertyViewModel>())
                {
                    return false;
                }
                slot->viewModel = new ConditionComparandViewModelBindable(
                    bp->as<BindablePropertyViewModel>());
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

ConditionComparison* wrapComparison(ComparisonShape shape,
                                    ComparandSlot& leftSlot,
                                    ComparandSlot& rightSlot,
                                    ConditionOperation* op)
{
    switch (shape)
    {
        case ComparisonShape::Number:
            if (leftSlot.number == nullptr || rightSlot.number == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonNumber(leftSlot.number,
                                                 rightSlot.number,
                                                 op);
        case ComparisonShape::Boolean:
            if (leftSlot.boolean == nullptr || rightSlot.boolean == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonBoolean(leftSlot.boolean,
                                                  rightSlot.boolean,
                                                  op);
        case ComparisonShape::String:
            if (leftSlot.string == nullptr || rightSlot.string == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonString(leftSlot.string,
                                                 rightSlot.string,
                                                 op);
        case ComparisonShape::Color:
            if (leftSlot.color == nullptr || rightSlot.color == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonColor(leftSlot.color,
                                                rightSlot.color,
                                                op);
        case ComparisonShape::Enum:
            if (leftSlot.uint32 == nullptr || rightSlot.uint32 == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonEnum(leftSlot.uint32,
                                               rightSlot.uint32,
                                               op);
        case ComparisonShape::Uint32:
            if (leftSlot.uint32 == nullptr || rightSlot.uint32 == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonUint32(leftSlot.uint32,
                                                 rightSlot.uint32,
                                                 op);
        case ComparisonShape::ViewModel:
            if (leftSlot.viewModel == nullptr || rightSlot.viewModel == nullptr)
            {
                return nullptr;
            }
            return new ConditionComparisonViewModel(leftSlot.viewModel,
                                                    rightSlot.viewModel,
                                                    op);
        default:
            return nullptr;
    }
}

bool buildComparandsFromIntersect(TransitionComparator* left,
                                  TransitionComparator* right,
                                  ComponentComparandKind lk,
                                  ComponentComparandKind rk,
                                  ConditionOperation* op,
                                  ConditionComparison** outComparison)
{
    ComparisonShape shape;
    if (!resolveComparisonShape(lk, rk, &shape))
    {
        return false;
    }
    ComparandSlot leftSlot, rightSlot;
    if (!makeComparand(left, lk, shape, &leftSlot) ||
        !makeComparand(right, rk, shape, &rightSlot))
    {
        clearComparandSlot(leftSlot);
        clearComparandSlot(rightSlot);
        return false;
    }
    ConditionComparison* wrapped =
        wrapComparison(shape, leftSlot, rightSlot, op);
    if (wrapped == nullptr)
    {
        clearComparandSlot(leftSlot);
        clearComparandSlot(rightSlot);
        return false;
    }
    *outComparison = wrapped;
    return true;
}

} // namespace

TransitionViewModelCondition::~TransitionViewModelCondition()
{
    if (m_leftComparator != nullptr)
    {
        delete m_leftComparator;
        m_leftComparator = nullptr;
    }
    if (m_rightComparator != nullptr)
    {
        delete m_rightComparator;
        m_rightComparator = nullptr;
    }
    if (m_comparison != nullptr)
    {
        delete m_comparison;
        m_comparison = nullptr;
    }
}

bool TransitionViewModelCondition::canEvaluate(
    const StateMachineInstance* stateMachineInstance) const
{
    if (!leftComparator() || !rightComparator())
    {
        return false;
    }
    if (!stateMachineInstance->dataContext() &&
        (rightComparator()->is<TransitionPropertyViewModelComparator>() ||
         leftComparator()->is<TransitionPropertyViewModelComparator>()))
    {
        return false;
    }
    return true;
}

bool TransitionViewModelCondition::evaluate(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    if (canEvaluate(stateMachineInstance) && m_comparison != nullptr)
    {
        return m_comparison->compare(stateMachineInstance, layerInstance);
    }
    return false;
}

void TransitionViewModelCondition::useInLayer(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    if (leftComparator() != nullptr)
    {
        return leftComparator()->useInLayer(stateMachineInstance,
                                            layerInstance);
    }
}

ConditionOperation* TransitionViewModelCondition::operation(
    TransitionConditionOp op)
{
    switch (op)
    {
        case TransitionConditionOp::equal:
            return new ConditionOperationEqual();
        case TransitionConditionOp::notEqual:
            return new ConditionOperationNotEqual();
        case TransitionConditionOp::lessThanOrEqual:
            return new ConditionOperationLessThanOrEqual();
        case TransitionConditionOp::lessThan:
            return new ConditionOperationLessThan();
        case TransitionConditionOp::greaterThanOrEqual:
            return new ConditionOperationGreaterThanOrEqual();
        case TransitionConditionOp::greaterThan:
            return new ConditionOperationGreaterThan();
    }
    return new ConditionOperation();
}

void TransitionViewModelCondition::initialize()
{
    TransitionComparator* left = leftComparator();
    TransitionComparator* right = rightComparator();
    if (left == nullptr || right == nullptr)
    {
        return;
    }

    // Asymmetric: Self compares the left bindable against its data bind source,
    // not a typed comparand intersection.
    if (right->is<TransitionSelfComparator>())
    {
        if (left->is<TransitionPropertyViewModelComparator>())
        {
            auto* leftBindable =
                left->as<TransitionPropertyViewModelComparator>()
                    ->bindableProperty();
            if (leftBindable != nullptr)
            {
                m_comparison = new ConditionComparisonSelf(leftBindable);
                return;
            }
        }
        m_comparison = new ConditionComparisonNone();
        return;
    }

    // Asymmetric: value trigger on the right uses ConditionComparisonSelf with
    // a VM trigger on the left (not uint32 vs a value comparand).
    if (left->is<TransitionPropertyViewModelComparator>() &&
        right->is<TransitionValueTriggerComparator>())
    {
        auto* lbp = left->as<TransitionPropertyViewModelComparator>()
                        ->bindableProperty();
        if (lbp != nullptr && lbp->is<BindablePropertyTrigger>())
        {
            m_comparison = new ConditionComparisonSelf(lbp);
            return;
        }
    }

    std::vector<ComponentComparandKind> leftKinds;
    std::vector<ComponentComparandKind> rightKinds;
    appendComparableKinds(left, ComparatorSide::Left, leftKinds);
    appendComparableKinds(right, ComparatorSide::Right, rightKinds);

    ComponentComparandKind lk{};
    ComponentComparandKind rk{};
    if (!intersectCompatibleKinds(leftKinds, rightKinds, &lk, &rk))
    {
        m_comparison = new ConditionComparisonNone();
        return;
    }

    ConditionOperation* condOp = operation(op());
    ConditionComparison* cmp = nullptr;
    if (buildComparandsFromIntersect(left, right, lk, rk, condOp, &cmp))
    {
        m_comparison = cmp;
        return;
    }
    delete condOp;
    m_comparison = new ConditionComparisonNone();
}

ConditionComparandComponentCoreNumber::ConditionComparandComponentCoreNumber(
    TransitionPropertyComponentComparator* comparator) :
    m_comparator(comparator)
{}

float ConditionComparandComponentCoreNumber::value(
    const StateMachineInstance* stateMachineInstance)
{
    Core* target = resolveComponentTarget(stateMachineInstance, m_comparator);
    if (target == nullptr)
    {
        return 0.0f;
    }
    return CoreRegistry::getDouble(target, (int)m_comparator->propertyKey());
}

ConditionComparandComponentCoreUintAsNumber::
    ConditionComparandComponentCoreUintAsNumber(
        TransitionPropertyComponentComparator* comparator) :
    m_comparator(comparator)
{}

float ConditionComparandComponentCoreUintAsNumber::value(
    const StateMachineInstance* stateMachineInstance)
{
    Core* target = resolveComponentTarget(stateMachineInstance, m_comparator);
    if (target == nullptr)
    {
        return 0.0f;
    }
    return (float)CoreRegistry::getUint(target,
                                        (int)m_comparator->propertyKey());
}

ConditionComparandComponentCoreBoolean::ConditionComparandComponentCoreBoolean(
    TransitionPropertyComponentComparator* comparator) :
    m_comparator(comparator)
{}

bool ConditionComparandComponentCoreBoolean::value(
    const StateMachineInstance* stateMachineInstance)
{
    Core* target = resolveComponentTarget(stateMachineInstance, m_comparator);
    if (target == nullptr)
    {
        return false;
    }
    return CoreRegistry::getBool(target, (int)m_comparator->propertyKey());
}

ConditionComparandComponentCoreString::ConditionComparandComponentCoreString(
    TransitionPropertyComponentComparator* comparator) :
    m_comparator(comparator)
{}

std::string ConditionComparandComponentCoreString::value(
    const StateMachineInstance* stateMachineInstance)
{
    Core* target = resolveComponentTarget(stateMachineInstance, m_comparator);
    if (target == nullptr)
    {
        return std::string{};
    }
    return CoreRegistry::getString(target, (int)m_comparator->propertyKey());
}

ConditionComparandComponentCoreColor::ConditionComparandComponentCoreColor(
    TransitionPropertyComponentComparator* comparator) :
    m_comparator(comparator)
{}

int ConditionComparandComponentCoreColor::value(
    const StateMachineInstance* stateMachineInstance)
{
    Core* target = resolveComponentTarget(stateMachineInstance, m_comparator);
    if (target == nullptr)
    {
        return 0;
    }
    return CoreRegistry::getColor(target, (int)m_comparator->propertyKey());
}

ConditionComparandComponentCoreUint::ConditionComparandComponentCoreUint(
    TransitionPropertyComponentComparator* comparator) :
    m_comparator(comparator)
{}

uint32_t ConditionComparandComponentCoreUint::value(
    const StateMachineInstance* stateMachineInstance)
{
    Core* target = resolveComponentTarget(stateMachineInstance, m_comparator);
    if (target == nullptr)
    {
        return 0;
    }
    return CoreRegistry::getUint(target, (int)m_comparator->propertyKey());
}

bool ConditionComparison::compareNumbers(float left, float right)
{
    return m_operation->compareNumbers(left, right);
}

bool ConditionComparison::compareStrings(const std::string& left,
                                         const std::string& right)
{
    return m_operation->compareStrings(left, right);
}

bool ConditionComparison::compareBooleans(bool left, bool right)
{
    return m_operation->compareBooleans(left, right);
}

bool ConditionComparison::compareColors(int left, int right)
{
    return m_operation->compareInts(left, right);
}

bool ConditionComparison::compareUints32(uint32_t left, uint32_t right)
{
    return m_operation->compareUints32(left, right);
}

bool ConditionComparison::comparePointers(const void* left, const void* right)
{
    return m_operation->compareBooleans(left == right, true);
}