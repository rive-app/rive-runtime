#include "rive/scripted/scripted_read_tracker.hpp"
#include "rive/component.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include <algorithm>
#include <functional>

using namespace rive;

ScriptedReadTracker::ScriptedReadTracker(size_t dependencyCount)
    : m_readCounts(dependencyCount, 0)
{}

ComponentDirt ScriptedReadTracker::pathDirt(const Path* path)
{
  auto dirt = ComponentDirt::Path | ComponentDirt::NSlicer;
  // A deformed path rebuilds when it moves too; its shape holds the deformer.
  auto shape = path->shape();
  if (shape != nullptr && shape->deformer() != nullptr)
  {
    dirt |= ComponentDirt::WorldTransform;
  }
  return dirt;
}

ComponentDirt ScriptedReadTracker::boundsDirt()
{
  return ComponentDirt::Path | ComponentDirt::NSlicer |
         ComponentDirt::WorldTransform | ComponentDirt::Collapsed;
}

Component* ScriptedReadTracker::boundsComposer(Component* node)
{
  return node->is<Shape>() ? node->as<Shape>()->pathComposer() : nullptr;
}

ScriptedReadTracker::Reader* ScriptedReadTracker::find(ScriptedObject* object)
{
  for (auto& reader : m_readers)
  {
    if (reader.object == object)
    {
      return &reader;
    }
  }
  return nullptr;
}

void ScriptedReadTracker::count(const PhaseReads& phase, int delta)
{
  for (auto& read : phase.reads)
  {
    m_readCounts[read.order] = (uint16_t)(m_readCounts[read.order] + delta);
  }
}

bool ScriptedReadTracker::track(
    ScriptedObject* object,
    ScriptedReadPhase phase,
    uint64_t serial,
    const std::vector<ScriptedRead>& reads,
    bool wroteAfterRead)
{
  Reader* reader = find(object);
  if (reader == nullptr)
  {
    if (reads.empty())
    {
      return false;
    }
    m_readers.emplace_back();
    reader = &m_readers.back();
    reader->object = object;
  }
  auto& phaseReads = reader->phase(phase);
  count(phaseReads, -1);
  auto& tracked = phaseReads.reads;
  tracked.clear();
  for (auto& read : reads)
  {
    unsigned int order = read.component->graphOrder();
    if (order < m_readCounts.size())
    {
      tracked.push_back({read.component, order, read.dirt});
    }
  }
  // One entry per component, wanting what each of its reads did.
  std::less<Component*> before;
  std::sort(
      tracked.begin(),
      tracked.end(),
      [&](const TrackedRead& a, const TrackedRead& b) {
        return before(a.component, b.component);
      });
  size_t unique = 0;
  for (size_t i = 0; i < tracked.size(); i++)
  {
    if (unique > 0 && tracked[unique - 1].component == tracked[i].component)
    {
      tracked[unique - 1].dirt |= tracked[i].dirt;
    }
    else
    {
      tracked[unique++] = tracked[i];
    }
  }
  tracked.resize(unique);
  phaseReads.serial = serial;
  // It read current whatever updated before it ran, but not what it wrote
  // after reading it.
  phaseReads.due = wroteAfterRead;
  count(phaseReads, 1);
  // Kept while a phase is due, like one whose reads a new dependency order
  // dropped, so that run still happens.
  if (reader->update.reads.empty() && reader->advance.reads.empty() &&
      !reader->update.due && !reader->advance.due)
  {
    untrack(object);
    return false;
  }
  return true;
}

void ScriptedReadTracker::untrack(ScriptedObject* object)
{
  for (auto itr = m_readers.begin(); itr != m_readers.end(); itr++)
  {
    if (itr->object == object)
    {
      count(itr->update, -1);
      count(itr->advance, -1);
      m_readers.erase(itr);
      return;
    }
  }
}

bool ScriptedReadTracker::isStale(
    const PhaseReads& phase,
    Component* component,
    ComponentDirt dirt,
    uint64_t serial)
{
  // Reads made after this update read it current: an update's own, as a
  // scripted object's update runs inside its own component update, after
  // the component's work.
  if (phase.due || phase.serial >= serial)
  {
    return false;
  }
  std::less<Component*> before;
  auto itr = std::lower_bound(
      phase.reads.begin(),
      phase.reads.end(),
      component,
      [&](const TrackedRead& read, Component* c) {
        return before(read.component, c);
      });
  return itr != phase.reads.end() && itr->component == component &&
         (itr->dirt & dirt) != ComponentDirt::None;
}

void ScriptedReadTracker::updated(
    Component* component,
    ComponentDirt dirt,
    uint64_t serial)
{
  for (auto& reader : m_readers)
  {
    if (isStale(reader.update, component, dirt, serial))
    {
      reader.update.due = true;
    }
    if (isStale(reader.advance, component, dirt, serial))
    {
      reader.advance.due = true;
    }
  }
}

void ScriptedReadTracker::beginUpdates()
{
  for (auto& reader : m_readers)
  {
    reader.ranAgain = false;
  }
}

bool ScriptedReadTracker::runDue()
{
  bool ran = false;
  for (auto& reader : m_readers)
  {
    // An advance already ran this frame, before the update pass, so it
    // wakes for the next one.
    if (reader.advance.due)
    {
      reader.advance.due = false;
      reader.object->readsWokeAdvance();
      ran = true;
    }
    if (!reader.update.due || reader.ranAgain)
    {
      continue;
    }
    // Cleared here rather than when it runs, so an object whose update does
    // not run, like one whose script failed, does not stay due forever.
    reader.update.due = false;
    reader.ranAgain = true;
    // Paint too, so it draws again and any cache holding its drawing does.
    reader.object->addScriptedDirt(
        ComponentDirt::ScriptUpdate | ComponentDirt::Paint);
    ran = true;
  }
  return ran;
}

bool ScriptedReadTracker::hasDue() const
{
  for (auto& reader : m_readers)
  {
    if (reader.update.due || reader.advance.due)
    {
      return true;
    }
  }
  return false;
}

void ScriptedReadTracker::dependencyOrderChanged(size_t dependencyCount)
{
  m_readCounts.assign(dependencyCount, 0);
  for (auto& reader : m_readers)
  {
    for (auto phase : {&reader.update, &reader.advance})
    {
      // Due already stays due: an earlier reorder may have dropped what it
      // read before it ran again.
      phase->due = phase->due || !phase->reads.empty();
      phase->reads.clear();
      phase->serial = 0;
    }
  }
}
