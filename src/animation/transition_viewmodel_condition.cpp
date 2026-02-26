#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/importers/state_transition_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/component_dirt.hpp"

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
    if (leftComparator() &&
        leftComparator()->is<TransitionPropertyArtboardComparator>() &&
        rightComparator() &&
        rightComparator()->is<TransitionValueNumberComparator>())
    {
        return true;
    }
    if (stateMachineInstance->dataContext())
    {
        return true;
    }
    return false;
}

bool TransitionViewModelCondition::evaluate(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    if (canEvaluate(stateMachineInstance))
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
    if (leftComparator() && rightComparator())
    {
        if (leftComparator()->is<TransitionPropertyArtboardComparator>())
        {

            auto leftProperty =
                leftComparator()->as<TransitionPropertyArtboardComparator>();
            if (rightComparator()->is<TransitionPropertyViewModelComparator>())
            {
                auto rightBindableProperty =
                    rightComparator()
                        ->as<TransitionPropertyViewModelComparator>()
                        ->bindableProperty();
                if (rightBindableProperty == nullptr)
                {
                    m_comparison = new ConditionComparisonNone();
                    return;
                }
                if (rightBindableProperty->is<BindablePropertyNumber>())
                {
                    auto leftComparand =
                        new ConditionComparandArtboardProperty(leftProperty);
                    auto rightComparand = new ConditionComparandNumberBindable(
                        rightBindableProperty->as<BindablePropertyNumber>());
                    m_comparison =
                        new ConditionComparisonNumber(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
                else if (rightBindableProperty->is<BindablePropertyInteger>())
                {
                    auto leftComparand =
                        new ConditionComparandArtboardProperty(leftProperty);
                    auto rightComparand =
                        new ConditionComparandNumberBindableInteger(
                            rightBindableProperty
                                ->as<BindablePropertyInteger>());
                    m_comparison =
                        new ConditionComparisonNumber(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
            }
            else if (rightComparator()->is<TransitionValueNumberComparator>())
            {
                auto leftComparand =
                    new ConditionComparandArtboardProperty(leftProperty);
                auto rightComparand = new ConditionComparandNumberValue(
                    rightComparator()->as<TransitionValueNumberComparator>());
                m_comparison = new ConditionComparisonNumber(leftComparand,
                                                             rightComparand,
                                                             operation(op()));
                return;
            }
        }
        auto leftBindableProperty =
            leftComparator()
                ->as<TransitionPropertyViewModelComparator>()
                ->bindableProperty();
        if (leftBindableProperty == nullptr)
        {
            m_comparison = new ConditionComparisonNone();
            return;
        }
        if (rightComparator()->is<TransitionSelfComparator>())
        {
            m_comparison = new ConditionComparisonSelf(leftBindableProperty);
            return;
        }
        switch (leftBindableProperty->coreType())
        {
            case BindablePropertyNumber::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyNumber>())
                    {
                        auto leftComparand =
                            new ConditionComparandNumberBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyNumber>());
                        auto rightComparand =
                            new ConditionComparandNumberBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyNumber>());
                        m_comparison =
                            new ConditionComparisonNumber(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                    else if (rightBindableProperty
                                 ->is<BindablePropertyInteger>())
                    {
                        auto leftComparand =
                            new ConditionComparandNumberBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyNumber>());
                        auto rightComparand =
                            new ConditionComparandNumberBindableInteger(
                                rightBindableProperty
                                    ->as<BindablePropertyInteger>());
                        m_comparison =
                            new ConditionComparisonNumber(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueNumberComparator>())
                {
                    auto leftComparand = new ConditionComparandNumberBindable(
                        leftBindableProperty->as<BindablePropertyNumber>());
                    auto rightComparand = new ConditionComparandNumberValue(
                        rightComparator()
                            ->as<TransitionValueNumberComparator>());
                    m_comparison =
                        new ConditionComparisonNumber(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
                break;
            }

            case BindablePropertyBoolean::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyBoolean>())
                    {
                        auto leftComparand =
                            new ConditionComparandBooleanBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyBoolean>());
                        auto rightComparand =
                            new ConditionComparandBooleanBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyBoolean>());
                        m_comparison =
                            new ConditionComparisonBoolean(leftComparand,
                                                           rightComparand,
                                                           operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueBooleanComparator>())
                {
                    auto leftComparand = new ConditionComparandBooleanBindable(
                        leftBindableProperty->as<BindablePropertyBoolean>());
                    auto rightComparand = new ConditionComparandBooleanValue(
                        rightComparator()
                            ->as<TransitionValueBooleanComparator>());
                    m_comparison =
                        new ConditionComparisonBoolean(leftComparand,
                                                       rightComparand,
                                                       operation(op()));
                    return;
                }
                break;
            }
            case BindablePropertyString::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyString>())
                    {
                        auto leftComparand =
                            new ConditionComparandStringBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyString>());
                        auto rightComparand =
                            new ConditionComparandStringBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyString>());
                        m_comparison =
                            new ConditionComparisonString(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueStringComparator>())
                {
                    auto leftComparand = new ConditionComparandStringBindable(
                        leftBindableProperty->as<BindablePropertyString>());
                    auto rightComparand = new ConditionComparandStringValue(
                        rightComparator()
                            ->as<TransitionValueStringComparator>());
                    m_comparison =
                        new ConditionComparisonString(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
                break;
            }
            case BindablePropertyColor::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyColor>())
                    {
                        auto leftComparand =
                            new ConditionComparandColorBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyColor>());
                        auto rightComparand =
                            new ConditionComparandColorBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyColor>());
                        m_comparison =
                            new ConditionComparisonColor(leftComparand,
                                                         rightComparand,
                                                         operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueColorComparator>())
                {
                    auto leftComparand = new ConditionComparandColorBindable(
                        leftBindableProperty->as<BindablePropertyColor>());
                    auto rightComparand = new ConditionComparandColorValue(
                        rightComparator()
                            ->as<TransitionValueColorComparator>());
                    m_comparison =
                        new ConditionComparisonColor(leftComparand,
                                                     rightComparand,
                                                     operation(op()));
                    return;
                }
                break;
            }
            case BindablePropertyEnum::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyEnum>())
                    {
                        auto leftComparand = new ConditionComparandEnumBindable(
                            leftBindableProperty->as<BindablePropertyEnum>());
                        auto rightComparand =
                            new ConditionComparandEnumBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyEnum>());
                        m_comparison =
                            new ConditionComparisonEnum(leftComparand,
                                                        rightComparand,
                                                        operation(op()));
                        return;
                    }
                }
                else if (rightComparator()->is<TransitionValueEnumComparator>())
                {
                    auto leftComparand = new ConditionComparandEnumBindable(
                        leftBindableProperty->as<BindablePropertyEnum>());
                    auto rightComparand = new ConditionComparandEnumValue(
                        rightComparator()->as<TransitionValueEnumComparator>());
                    m_comparison = new ConditionComparisonEnum(leftComparand,
                                                               rightComparand,
                                                               operation(op()));
                    return;
                }
                break;
            }
            case BindablePropertyTrigger::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyTrigger>())
                    {
                        auto leftComparand =
                            new ConditionComparandTriggerBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyTrigger>());
                        auto rightComparand =
                            new ConditionComparandTriggerBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyTrigger>());
                        m_comparison =
                            new ConditionComparisonUint32(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueTriggerComparator>())
                {
                    m_comparison =
                        new ConditionComparisonSelf(leftBindableProperty);
                    return;
                }
                break;
            }
            case BindablePropertyInteger::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyInteger>())
                    {
                        auto leftComparand =
                            new ConditionComparandIntegerBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyInteger>());
                        auto rightComparand =
                            new ConditionComparandIntegerBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyInteger>());
                        m_comparison =
                            new ConditionComparisonUint32(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                    else if (rightBindableProperty
                                 ->is<BindablePropertyNumber>())
                    {
                        auto leftComparand =
                            new ConditionComparandNumberBindableInteger(
                                leftBindableProperty
                                    ->as<BindablePropertyInteger>());
                        auto rightComparand =
                            new ConditionComparandNumberBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyNumber>());
                        m_comparison =
                            new ConditionComparisonNumber(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueNumberComparator>())
                {
                    auto leftComparand =
                        new ConditionComparandNumberBindableInteger(
                            leftBindableProperty
                                ->as<BindablePropertyInteger>());
                    auto rightComparand = new ConditionComparandNumberValue(
                        rightComparator()
                            ->as<TransitionValueNumberComparator>());
                    m_comparison =
                        new ConditionComparisonNumber(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
                break;
            }
            case BindablePropertyAsset::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyAsset>())
                    {
                        auto leftComparand =
                            new ConditionComparandAssetBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyAsset>());
                        auto rightComparand =
                            new ConditionComparandAssetBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyAsset>());
                        m_comparison =
                            new ConditionComparisonUint32(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueAssetComparator>())
                {
                    auto leftComparand = new ConditionComparandAssetBindable(
                        leftBindableProperty->as<BindablePropertyAsset>());
                    auto rightComparand = new ConditionComparandAssetValue(
                        rightComparator()
                            ->as<TransitionValueAssetComparator>());
                    m_comparison =
                        new ConditionComparisonUint32(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
                break;
            }
            case BindablePropertyArtboard::typeKey:
            {
                if (rightComparator()
                        ->is<TransitionPropertyViewModelComparator>())
                {
                    auto rightBindableProperty =
                        rightComparator()
                            ->as<TransitionPropertyViewModelComparator>()
                            ->bindableProperty();
                    if (rightBindableProperty == nullptr)
                    {
                        break;
                    }
                    if (rightBindableProperty->is<BindablePropertyArtboard>())
                    {
                        auto leftComparand =
                            new ConditionComparandArtboardBindable(
                                leftBindableProperty
                                    ->as<BindablePropertyArtboard>());
                        auto rightComparand =
                            new ConditionComparandArtboardBindable(
                                rightBindableProperty
                                    ->as<BindablePropertyArtboard>());
                        m_comparison =
                            new ConditionComparisonUint32(leftComparand,
                                                          rightComparand,
                                                          operation(op()));
                        return;
                    }
                }
                else if (rightComparator()
                             ->is<TransitionValueArtboardComparator>())
                {
                    auto leftComparand = new ConditionComparandArtboardBindable(
                        leftBindableProperty->as<BindablePropertyArtboard>());
                    auto rightComparand = new ConditionComparandArtboardValue(
                        rightComparator()
                            ->as<TransitionValueArtboardComparator>());
                    m_comparison =
                        new ConditionComparisonUint32(leftComparand,
                                                      rightComparand,
                                                      operation(op()));
                    return;
                }
                break;
            }
            default:
                break;
        }
        m_comparison = new ConditionComparisonNone();
    }
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