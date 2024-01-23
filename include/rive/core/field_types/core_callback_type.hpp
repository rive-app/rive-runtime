#ifndef _RIVE_CORE_CALLBACK_TYPE_HPP_
#define _RIVE_CORE_CALLBACK_TYPE_HPP_

namespace rive
{
class Event;
class CallbackContext
{
public:
    virtual ~CallbackContext() {}
    virtual void reportEvent(Event* event, float secondsDelay = 0.0f) {}
};

class CallbackData
{
public:
    CallbackContext* context() const { return m_context; }
    float delaySeconds() const { return m_delaySeconds; }
    CallbackData(CallbackContext* context, float delaySeconds) :
        m_context(context), m_delaySeconds(delaySeconds)
    {}

private:
    CallbackContext* m_context;
    float m_delaySeconds;
};
} // namespace rive
#endif