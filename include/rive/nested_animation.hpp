#ifndef _RIVE_NESTED_ANIMATION_HPP_
#define _RIVE_NESTED_ANIMATION_HPP_
#include "rive/event.hpp"
#include "rive/event_report.hpp"
#include "rive/generated/nested_animation_base.hpp"
#include "rive/nested_artboard.hpp"
#include <stdio.h>
namespace rive
{
class ArtboardInstance;

class NestedEventListener
{
public:
    virtual ~NestedEventListener() {}
    virtual void notify(const std::vector<EventReport>& events, NestedArtboard* context) = 0;
};

class NestedEventNotifier
{
public:
    ~NestedEventNotifier()
    {
        m_nestedArtboard = nullptr;
        m_nestedEventListeners.clear();
    }
    void addNestedEventListener(NestedEventListener* listener)
    {
        m_nestedEventListeners.push_back(listener);
    }
    std::vector<NestedEventListener*> nestedEventListeners() { return m_nestedEventListeners; }

    void setNestedArtboard(NestedArtboard* artboard) { m_nestedArtboard = artboard; }
    NestedArtboard* nestedArtboard() { return m_nestedArtboard; }

    void notifyListeners(const std::vector<Event*>& events)
    {
        std::vector<EventReport> eventReports;
        for (auto event : events)
        {
            eventReports.push_back(EventReport(event, 0));
        }
        for (auto listener : m_nestedEventListeners)
        {
            listener->notify(eventReports, m_nestedArtboard);
        }
    }

private:
    NestedArtboard* m_nestedArtboard = nullptr;
    std::vector<NestedEventListener*> m_nestedEventListeners;
};

class NestedAnimation : public NestedAnimationBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;

    // Advance animations and apply them to the artboard.
    virtual bool advance(float elapsedSeconds) = 0;

    // Initialize the animation (make instances as necessary) from the
    // source artboard.
    virtual void initializeAnimation(ArtboardInstance*) = 0;
};
} // namespace rive

#endif