#ifndef _RIVE_EVENT_REPORT_HPP_
#define _RIVE_EVENT_REPORT_HPP_

namespace rive
{
class Event;

class EventReport
{
public:
    EventReport(Event* event, float secondsDelay) : m_event(event), m_secondsDelay(secondsDelay) {}
    Event* event() const { return m_event; }
    float secondsDelay() const { return m_secondsDelay; }

private:
    Event* m_event;
    float m_secondsDelay;
};
} // namespace rive

#endif