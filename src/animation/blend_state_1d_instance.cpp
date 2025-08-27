#include "rive/animation/blend_state_1d_input.hpp"
#include "rive/animation/blend_state_1d_instance.hpp"
#include "rive/animation/blend_state_1d_viewmodel.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/animation/layer_state_flags.hpp"

using namespace rive;

BlendState1DInstance::BlendState1DInstance(const BlendState1D* blendState,
                                           ArtboardInstance* instance) :
    BlendStateInstance<BlendState1D, BlendAnimation1D>(blendState, instance)
{

    if ((static_cast<LayerStateFlags>(blendState->flags()) &
         LayerStateFlags::Reset) == LayerStateFlags::Reset)
    {
        auto animations = std::vector<const LinearAnimation*>();
        for (auto blendAnimation : blendState->animations())
        {
            animations.push_back(blendAnimation->animation());
        }
        m_AnimationReset =
            AnimationResetFactory::fromAnimations(animations, instance, true);
    }
}

BlendState1DInstance::~BlendState1DInstance()
{
    if (m_AnimationReset != nullptr)
    {
        AnimationResetFactory::release(std::move(m_AnimationReset));
    }
}

int BlendState1DInstance::animationIndex(float value)
{
    int idx = 0;
    int mid = 0;
    float closestValue = 0;
    int start = 0;
    int end = static_cast<int>(m_AnimationInstances.size()) - 1;

    while (start <= end)
    {
        mid = (start + end) >> 1;
        closestValue = m_AnimationInstances[mid].blendAnimation()->value();
        if (closestValue < value)
        {
            start = mid + 1;
        }
        else if (closestValue > value)
        {
            end = mid - 1;
        }
        else
        {
            idx = start = mid;
            break;
        }

        idx = start;
    }
    return idx;
}

void BlendState1DInstance::apply(ArtboardInstance* instance, float mix)
{
    if (m_AnimationReset != nullptr)
    {
        m_AnimationReset->apply(instance);
    }
    BlendStateInstance::apply(instance, mix);
}

void BlendState1DInstance::advance(float seconds,
                                   StateMachineInstance* stateMachineInstance)
{
    BlendStateInstance<BlendState1D, BlendAnimation1D>::advance(
        seconds,
        stateMachineInstance);

    float value = 0.0f;
    if (state()->is<BlendState1DInput>())
    {
        auto blendState = state()->as<BlendState1DInput>();

        if (blendState->hasValidInputId())
        {
            // TODO: https://github.com/rive-app/rive-cpp/issues/229
            auto inputInstance =
                stateMachineInstance->input(blendState->inputId());
            auto numberInput = static_cast<const SMINumber*>(inputInstance);
            value = numberInput->value();
        }
    }
    else if (state()->is<BlendState1DViewModel>())
    {
        auto blendState = state()->as<BlendState1DViewModel>();
        auto bindablePropertyInstance =
            stateMachineInstance->bindablePropertyInstance(
                blendState->bindableProperty());
        if (bindablePropertyInstance->is<BindablePropertyNumber>())
        {
            value = bindablePropertyInstance->as<BindablePropertyNumber>()
                        ->propertyValue();
        }
    }
    int index = animationIndex(value);
    auto animationsCount = static_cast<int>(m_AnimationInstances.size());
    m_To = index >= 0 && index < animationsCount ? &m_AnimationInstances[index]
                                                 : nullptr;
    m_From = index - 1 >= 0 && index - 1 < animationsCount
                 ? &m_AnimationInstances[index - 1]
                 : nullptr;

    float mix, mixFrom;
    auto toValue = m_To == nullptr ? 0.0f : m_To->blendAnimation()->value();
    auto fromValue =
        m_From == nullptr ? 0.0f : m_From->blendAnimation()->value();

    if (m_To == nullptr || m_From == nullptr || toValue == fromValue)
    {
        mix = mixFrom = 1.0f;
    }
    else
    {
        mix = (value - fromValue) / (toValue - fromValue);
        mixFrom = 1.0f - mix;
    }

    for (auto& animation : m_AnimationInstances)
    {
        auto animationValue = animation.blendAnimation()->value();
        if (m_To != nullptr && animationValue == toValue)
        {
            animation.mix(mix);
        }
        else if (m_From != nullptr && animationValue == fromValue)
        {
            animation.mix(mixFrom);
        }
        else
        {
            animation.mix(0.0f);
        }
    }
}
