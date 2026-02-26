#ifndef _RIVE_TRANSITION_VIEW_MODEL_CONDITION_HPP_
#define _RIVE_TRANSITION_VIEW_MODEL_CONDITION_HPP_
#include "rive/generated/animation/transition_viewmodel_condition_base.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_property_artboard_comparator.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_condition_op.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/data_bind/bindable_property_artboard.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/animation/transition_self_comparator.hpp"
#include "rive/animation/transition_value_artboard_comparator.hpp"
#include "rive/animation/transition_value_asset_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"
#include "rive/animation/artboard_property.hpp"
#include <stdio.h>
namespace rive
{

class ConditionOperation
{
public:
    virtual ~ConditionOperation() {}
    virtual bool compareNumbers(float a, float b) { return false; };
    virtual bool compareBooleans(bool a, bool b) { return false; }
    virtual bool compareStrings(const std::string& a, const std::string& b)
    {
        return false;
    }
    virtual bool compareInts(int a, int b) { return false; }
    virtual bool compareUints32(uint32_t a, uint32_t b) { return false; }

protected:
    template <typename T> bool equal(T left, T right) { return left == right; }
    template <typename T> bool notEqual(T left, T right)
    {
        return left != right;
    }
    template <typename T> bool lessThanOrEqual(T left, T right)
    {
        return left <= right;
    }
    template <typename T> bool lessThan(T left, T right)
    {
        return left < right;
    }
    template <typename T> bool greaterThanOrEqual(T left, T right)
    {
        return left >= right;
    }
    template <typename T> bool greaterThan(T left, T right)
    {
        return left > right;
    }
};
class ConditionOperationEqual : public ConditionOperation
{
public:
    bool compareNumbers(float a, float b) override
    {
        return equal<float>(a, b);
    }
    bool compareBooleans(bool a, bool b) override { return equal<bool>(a, b); }
    bool compareStrings(const std::string& a, const std::string& b) override
    {
        return equal<std::string>(a, b);
    }
    bool compareInts(int a, int b) override { return equal<int>(a, b); }
    bool compareUints32(uint32_t a, uint32_t b) override
    {
        return equal<uint32_t>(a, b);
    }
};
class ConditionOperationNotEqual : public ConditionOperation
{
public:
    bool compareNumbers(float a, float b) override
    {
        return notEqual<float>(a, b);
    }
    bool compareBooleans(bool a, bool b) override
    {
        return notEqual<bool>(a, b);
    }
    bool compareStrings(const std::string& a, const std::string& b) override
    {
        return notEqual<std::string>(a, b);
    }
    bool compareInts(int a, int b) override { return notEqual<int>(a, b); }
    bool compareUints32(uint32_t a, uint32_t b) override
    {
        return notEqual<uint32_t>(a, b);
    }
};
class ConditionOperationLessThanOrEqual : public ConditionOperation
{
public:
    bool compareNumbers(float a, float b) override
    {
        return lessThanOrEqual<float>(a, b);
    }
};
class ConditionOperationLessThan : public ConditionOperation
{
public:
    bool compareNumbers(float a, float b) override
    {
        return lessThan<float>(a, b);
    }
};
class ConditionOperationGreaterThanOrEqual : public ConditionOperation
{
public:
    bool compareNumbers(float a, float b) override
    {
        return greaterThanOrEqual<float>(a, b);
    }
};
class ConditionOperationGreaterThan : public ConditionOperation
{
public:
    bool compareNumbers(float a, float b) override
    {
        return greaterThan<float>(a, b);
    }
};
class ConditionComparand
{};

class ConditionComparandNumber : public ConditionComparand
{
public:
    virtual ~ConditionComparandNumber() {}
    virtual float value(const StateMachineInstance* stateMachineInstance) = 0;
};
class ConditionComparandNumberBindable : public ConditionComparandNumber
{
public:
    ConditionComparandNumberBindable(BindablePropertyNumber* property) :
        m_bindableProperty(property)
    {}
    float value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyNumber>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyNumber* m_bindableProperty;
};
class ConditionComparandArtboardProperty : public ConditionComparandNumber
{
public:
    ConditionComparandArtboardProperty(
        TransitionPropertyArtboardComparator* property) :
        m_transitionPropertyArtboardComparator(property)
    {}
    float value(const StateMachineInstance* stateMachineInstance) override
    {

        auto artboard = stateMachineInstance->artboard();
        if (artboard != nullptr)
        {
            auto property = static_cast<ArtboardProperty>(
                m_transitionPropertyArtboardComparator->propertyType());
            switch (property)
            {
                case ArtboardProperty::width:
                    return artboard->layoutWidth();
                    break;
                case ArtboardProperty::height:
                    return artboard->layoutHeight();
                    break;
                case ArtboardProperty::ratio:
                    return artboard->layoutWidth() / artboard->layoutHeight();
                    break;

                default:
                    break;
            }
        }
        return 0;
    }

private:
    TransitionPropertyArtboardComparator*
        m_transitionPropertyArtboardComparator;
};

class ConditionComparandNumberBindableInteger : public ConditionComparandNumber
{
public:
    ConditionComparandNumberBindableInteger(BindablePropertyInteger* property) :
        m_bindableProperty(property)
    {}
    float value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {

            return (float)bindableInstance->as<BindablePropertyInteger>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyInteger* m_bindableProperty;
};
class ConditionComparandNumberValue : public ConditionComparandNumber
{
public:
    ConditionComparandNumberValue(TransitionValueNumberComparator* value) :
        m_value(value)
    {}
    float value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueNumberComparator* m_value;
};

class ConditionComparandBoolean : public ConditionComparand
{
public:
    virtual ~ConditionComparandBoolean() {}
    virtual bool value(const StateMachineInstance* stateMachineInstance) = 0;
};
class ConditionComparandBooleanBindable : public ConditionComparandBoolean
{
public:
    ConditionComparandBooleanBindable(BindablePropertyBoolean* property) :
        m_bindableProperty(property)
    {}
    bool value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyBoolean>()
                ->propertyValue();
        }
        return false;
    }

private:
    BindablePropertyBoolean* m_bindableProperty;
};
class ConditionComparandBooleanValue : public ConditionComparandBoolean
{
public:
    ConditionComparandBooleanValue(TransitionValueBooleanComparator* value) :
        m_value(value)
    {}
    bool value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueBooleanComparator* m_value;
};

class ConditionComparandString : public ConditionComparand
{
public:
    virtual ~ConditionComparandString() {}
    virtual std::string value(
        const StateMachineInstance* stateMachineInstance) = 0;
};
class ConditionComparandStringBindable : public ConditionComparandString
{
public:
    ConditionComparandStringBindable(BindablePropertyString* property) :
        m_bindableProperty(property)
    {}
    std::string value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyString>()
                ->propertyValue();
        }
        return std::string{};
    }

private:
    BindablePropertyString* m_bindableProperty;
};
class ConditionComparandStringValue : public ConditionComparandString
{
public:
    ConditionComparandStringValue(TransitionValueStringComparator* value) :
        m_value(value)
    {}
    std::string value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueStringComparator* m_value;
};

class ConditionComparandColor : public ConditionComparand
{
public:
    virtual ~ConditionComparandColor() {}
    virtual int value(const StateMachineInstance* stateMachineInstance) = 0;
};
class ConditionComparandColorBindable : public ConditionComparandColor
{
public:
    ConditionComparandColorBindable(BindablePropertyColor* property) :
        m_bindableProperty(property)
    {}
    int value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyColor>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyColor* m_bindableProperty;
};
class ConditionComparandColorValue : public ConditionComparandColor
{
public:
    ConditionComparandColorValue(TransitionValueColorComparator* value) :
        m_value(value)
    {}
    int value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueColorComparator* m_value;
};

class ConditionComparandUint32 : public ConditionComparand
{
public:
    virtual ~ConditionComparandUint32() {}
    virtual uint32_t value(
        const StateMachineInstance* stateMachineInstance) = 0;
};
class ConditionComparandEnumBindable : public ConditionComparandUint32
{
public:
    ConditionComparandEnumBindable(BindablePropertyEnum* property) :
        m_bindableProperty(property)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyEnum>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyEnum* m_bindableProperty;
};
class ConditionComparandEnumValue : public ConditionComparandUint32
{
public:
    ConditionComparandEnumValue(TransitionValueEnumComparator* value) :
        m_value(value)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueEnumComparator* m_value;
};
class ConditionComparandTriggerBindable : public ConditionComparandUint32
{
public:
    ConditionComparandTriggerBindable(BindablePropertyTrigger* property) :
        m_bindableProperty(property)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyTrigger>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyTrigger* m_bindableProperty;
};
class ConditionComparandTriggerValue : public ConditionComparandUint32
{
public:
    ConditionComparandTriggerValue(TransitionValueTriggerComparator* value) :
        m_value(value)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueTriggerComparator* m_value;
};
class ConditionComparandIntegerBindable : public ConditionComparandUint32
{
public:
    ConditionComparandIntegerBindable(BindablePropertyInteger* property) :
        m_bindableProperty(property)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyInteger>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyInteger* m_bindableProperty;
};
class ConditionComparandAssetBindable : public ConditionComparandUint32
{
public:
    ConditionComparandAssetBindable(BindablePropertyAsset* property) :
        m_bindableProperty(property)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyAsset>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyAsset* m_bindableProperty;
};
class ConditionComparandAssetValue : public ConditionComparandUint32
{
public:
    ConditionComparandAssetValue(TransitionValueAssetComparator* value) :
        m_value(value)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueAssetComparator* m_value;
};
class ConditionComparandArtboardBindable : public ConditionComparandUint32
{
public:
    ConditionComparandArtboardBindable(BindablePropertyArtboard* property) :
        m_bindableProperty(property)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        if (bindableInstance)
        {
            return bindableInstance->as<BindablePropertyArtboard>()
                ->propertyValue();
        }
        return 0;
    }

private:
    BindablePropertyArtboard* m_bindableProperty;
};
class ConditionComparandArtboardValue : public ConditionComparandUint32
{
public:
    ConditionComparandArtboardValue(TransitionValueArtboardComparator* value) :
        m_value(value)
    {}
    uint32_t value(const StateMachineInstance* stateMachineInstance) override
    {
        return m_value->value();
    }

private:
    TransitionValueArtboardComparator* m_value;
};

class ConditionComparison
{
public:
    ConditionComparison(ConditionOperation* op) : m_operation(op) {}
    virtual ~ConditionComparison() { delete m_operation; }
    virtual bool compare(const StateMachineInstance* stateMachineInstance,
                         StateMachineLayerInstance* layerInstance) = 0;

    bool compareNumbers(float left, float right);
    bool compareBooleans(bool left, bool right);
    bool compareStrings(const std::string& left, const std::string& right);
    bool compareColors(int left, int right);
    bool compareUints32(uint32_t left, uint32_t right);

protected:
    ConditionOperation* m_operation;
};
class TransitionViewModelCondition : public TransitionViewModelConditionBase
{
protected:
    TransitionComparator* m_leftComparator;
    TransitionComparator* m_rightComparator;
    ConditionComparison* m_comparison = nullptr;

public:
    ~TransitionViewModelCondition();
    bool evaluate(const StateMachineInstance* stateMachineInstance,
                  StateMachineLayerInstance* layerInstance) const override;
    void useInLayer(const StateMachineInstance* stateMachineInstance,
                    StateMachineLayerInstance* layerInstance) const override;
    TransitionComparator* leftComparator() const { return m_leftComparator; };
    TransitionComparator* rightComparator() const { return m_rightComparator; };
    void comparator(TransitionComparator* value)
    {
        if (m_leftComparator == nullptr)
        {
            m_leftComparator = value;
        }
        else
        {
            m_rightComparator = value;
        }
    };
    TransitionConditionOp op() const
    {
        return (TransitionConditionOp)opValue();
    }
    ConditionOperation* operation(TransitionConditionOp op);
    void initialize();

private:
    bool canEvaluate(const StateMachineInstance*) const;
};
} // namespace rive

#endif