#ifndef _RIVE_HOST_DATA_CONTEXT_CONSUMER_HPP_
#define _RIVE_HOST_DATA_CONTEXT_CONSUMER_HPP_
#include "rive/refcnt.hpp"

namespace rive
{
class Artboard;
class Core;
class DataContext;

/// A component that instances something bound under the data context of the
/// artboard it lives in (a Text's global text styles), told by that artboard
/// when its context changes and after its own binds applied, so values the
/// host writes reach the instance in the same frame.
class HostDataContextConsumer
{
public:
  virtual ~HostDataContextConsumer() {}
  static HostDataContextConsumer* from(Core* component);

  /// The hosting artboard's data context was (re)set, or cleared (null).
  virtual void hostDataContext(rcp<DataContext> value) = 0;
  /// The hosting artboard's data context relinked its instances.
  virtual void relinkHostDataContext() = 0;
  /// The hosting artboard applied its binds: apply the instance's.
  virtual void updateHostedDataBinds() = 0;
  /// The hosting artboard went quiet (see Artboard::markHostedQuiet): marks
  /// the artboards this instanced quiet with it, so that whatever may change
  /// one of them wakes the host.
  virtual void markInstancesQuiet(Artboard* host) {}
  /// Whether every artboard markInstancesQuiet marked is still marked. For
  /// the TESTING verifier, but in every build: a virtual only some builds
  /// declare would give their callers different tables.
  virtual bool instancesMarkedQuiet() const { return true; }
};
} // namespace rive

#endif
