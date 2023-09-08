#ifndef _RIVE_CORE_CALLBACK_TYPE_HPP_
#define _RIVE_CORE_CALLBACK_TYPE_HPP_

namespace rive
{
class StateMachineInstance;
class CallbackData
{
public:
    StateMachineInstance* context() const { return m_context; }
    float delaySeconds() const { return m_delaySeconds; }
    CallbackData(StateMachineInstance* context, float delaySeconds) :
        m_context(context), m_delaySeconds(delaySeconds)
    {}

private:
    StateMachineInstance* m_context;
    float m_delaySeconds;
};
} // namespace rive
#endif