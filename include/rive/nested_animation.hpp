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
class NestedEventNotifier;

class NestedEventListener
{
public:
  NestedEventListener() = default;
  // A copy starts unregistered: the notifiers only hold the original.
  NestedEventListener(const NestedEventListener&) {}
  NestedEventListener& operator=(const NestedEventListener&) { return *this; }
  virtual ~NestedEventListener();
  virtual void notify(
      const std::vector<EventReport>& events,
      NestedArtboard* context) = 0;

  // Unregisters from every notifier this listener was added to.
  void removeFromNestedEventNotifiers();

private:
  friend class NestedEventNotifier;
  // Links are kept on both sides so whichever is destroyed first unlinks from
  // the other. A host can delete a state machine and keep its artboard, whose
  // nested machines would otherwise report to the deleted one.
  std::vector<NestedEventNotifier*> m_nestedEventNotifiers;
};

class NestedEventNotifier
{
public:
  NestedEventNotifier() = default;
  // A copy starts with no listeners: they registered with the original.
  NestedEventNotifier(const NestedEventNotifier&) {}
  NestedEventNotifier& operator=(const NestedEventNotifier&) { return *this; }
  ~NestedEventNotifier()
  {
    m_nestedArtboard = nullptr;
    for (auto listener : m_nestedEventListeners)
    {
      unlinkListener(listener);
    }
    m_nestedEventListeners.clear();
  }
  void addNestedEventListener(NestedEventListener* listener)
  {
    m_nestedEventListeners.push_back(listener);
    listener->m_nestedEventNotifiers.push_back(this);
  }
  void removeNestedEventListener(NestedEventListener* listener)
  {
    auto nested = std::remove(
        m_nestedEventListeners.begin(),
        m_nestedEventListeners.end(),
        listener);
    m_nestedEventListeners.erase(nested, m_nestedEventListeners.end());
    unlinkListener(listener);
  }
  std::vector<NestedEventListener*> nestedEventListeners()
  {
    return m_nestedEventListeners;
  }

  void setNestedArtboard(NestedArtboard* artboard)
  {
    m_nestedArtboard = artboard;
  }
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
  // Drops this notifier from the listener's side of the link.
  void unlinkListener(NestedEventListener* listener)
  {
    auto& notifiers = listener->m_nestedEventNotifiers;
    notifiers.erase(
        std::remove(notifiers.begin(), notifiers.end(), this),
        notifiers.end());
  }

  NestedArtboard* m_nestedArtboard = nullptr;
  std::vector<NestedEventListener*> m_nestedEventListeners;
};

inline NestedEventListener::~NestedEventListener()
{
  removeFromNestedEventNotifiers();
}

inline void NestedEventListener::removeFromNestedEventNotifiers()
{
  // removeNestedEventListener edits m_nestedEventNotifiers, so walk a copy.
  auto notifiers = m_nestedEventNotifiers;
  for (auto notifier : notifiers)
  {
    notifier->removeNestedEventListener(this);
  }
}

class NestedAnimation : public NestedAnimationBase
{
public:
  bool validate(CoreContext* context) override;
  StatusCode onAddedDirty(CoreContext* context) override;

  // Advance animations and apply them to the artboard.
  virtual bool advance(float elapsedSeconds, bool newFrame) = 0;

  // Initialize the animation (make instances as necessary) from the
  // source artboard.
  virtual void initializeAnimation(ArtboardInstance*) = 0;

  // Clear the nested animation dependencies
  virtual void releaseDependencies() = 0;

#ifdef WITH_RIVE_EDITOR
  void editorPrepareForDestruction() override
  {
    releaseDependencies();
    Component::editorPrepareForDestruction();
  }
  // Body in `editor_native/.../component_parent_editor.cpp`. See
  // `Component::editorParentChanged` for the lifecycle contract.
  void editorParentChanged(ContainerComponent* from, ContainerComponent* to)
      override;
#endif
};
} // namespace rive

#endif