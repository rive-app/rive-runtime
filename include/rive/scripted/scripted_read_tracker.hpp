#ifndef _RIVE_SCRIPTED_READ_TRACKER_HPP_
#define _RIVE_SCRIPTED_READ_TRACKER_HPP_
#include "rive/component_dirt.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rive
{
class Component;
class Path;
class ScriptedObject;

/// A component a scripted object's update or advance read, and the dirt that
/// makes the read stale.
struct ScriptedRead
{
  Component* component;
  ComponentDirt dirt;
};

/// The script callback that made a set of reads.
enum class ScriptedReadPhase : uint8_t
{
  update,
  advance,
};

/// The components of one artboard that its scripted objects read in update
/// and advance. When one of them updates, what read it before then runs
/// again: an update once the update pass settles, so in the same frame and
/// with everything it read current; an advance on the next frame, as advance
/// runs before the update pass. Calling into a script from the update itself
/// would read components that are later in the dependency order and still
/// dirty.
class ScriptedReadTracker
{
public:
  explicit ScriptedReadTracker(size_t dependencyCount);

  /// The dirt that makes a read of `path`'s geometry stale: what makes
  /// Path::update rebuild it.
  static ComponentDirt pathDirt(const Path* path);

  /// Replaces what `object` read in `phase` with `reads`, read when the last
  /// component update was the one numbered `serial` (see
  /// Artboard::updateSerial). Reads of components outside the dependency
  /// order are dropped, as they never update. `wroteAfterRead` when it
  /// wrote something after reading it (see ScriptedObject::wroteAfterRead):
  /// it stays due, to run again with that read current. Returns whether it
  /// still tracks anything `object` read.
  bool track(
      ScriptedObject* object,
      ScriptedReadPhase phase,
      uint64_t serial,
      const std::vector<ScriptedRead>& reads,
      bool wroteAfterRead = false);
  void untrack(ScriptedObject* object);

  /// Whether a script read the component at `order` in the dependency
  /// order, the update loop's cheap check before calling updated().
  bool isRead(unsigned int order) const
  {
    return order < m_readCounts.size() && m_readCounts[order] != 0;
  }

  /// `component` updated with `dirt` in the update numbered `serial`: what
  /// read it before then is due again.
  void updated(Component* component, ComponentDirt dirt, uint64_t serial);

  /// An Artboard::updateComponents call starts; each object due to update
  /// may run once more in it.
  void beginUpdates();

  /// Dirties the objects due to update that have not run again in this call,
  /// so the update loop runs their updates, and wakes the advance of those
  /// due to advance. Returns whether it did either.
  bool runDue();

  /// Whether an object is still due, as one left for the next call because
  /// it already ran again in this one, like one that writes what it reads.
  bool hasDue() const;

  /// The dependency order changed under the reads: they are dropped, and
  /// every object runs again to read afresh.
  void dependencyOrderChanged(size_t dependencyCount);

private:
  struct TrackedRead
  {
    // Compared, never dereferenced: an untrack can run while the artboard
    // is deleting its components.
    Component* component;
    unsigned int order;
    ComponentDirt dirt;
  };

  struct PhaseReads
  {
    // The last component update before the reads.
    uint64_t serial = 0;
    // Sorted by component, one entry each.
    std::vector<TrackedRead> reads;
    bool due = false;
  };

  struct Reader
  {
    ScriptedObject* object = nullptr;
    PhaseReads update;
    PhaseReads advance;
    // Updated again in this updateComponents call.
    bool ranAgain = false;
    PhaseReads& phase(ScriptedReadPhase value)
    {
      return value == ScriptedReadPhase::update ? update : advance;
    }
  };

  Reader* find(ScriptedObject* object);
  void count(const PhaseReads& phase, int delta);
  static bool isStale(
      const PhaseReads& phase,
      Component* component,
      ComponentDirt dirt,
      uint64_t serial);

  std::vector<Reader> m_readers;
  // How many read sets read each component, by dependency order.
  std::vector<uint16_t> m_readCounts;
};
} // namespace rive

#endif
