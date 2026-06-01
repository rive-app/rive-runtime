#ifndef _RIVE_PROFILE_HPP_
#define _RIVE_PROFILE_HPP_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rive
{

/// Bitmask for which profile data to log. Combine flags to enable multiple.
enum ProfileLogFlags : uint32_t
{
    ProfileLogNone = 0,
    ProfileLogTransitionRecords = 1u << 0,
    ProfileLogListenerPerformChanges = 1u << 1,
};

/// Segment of the artboard path (root to state machine artboard).
/// type: 0 = NestedArtboard, 1 = ArtboardComponentList. index is used only when
/// type is ArtboardComponentList (logical index in list); otherwise -1.
struct ArtboardPathSegment
{
    uint8_t type = 0;
    uint32_t nameId = 0;
    int32_t index = -1;
};

/// Record for a single state machine transition (used when profiling).
/// String fields are stored as indices into the profiler string table;
/// see getStringTable() and resolveStringId(). path is root-to-leaf host chain.
struct TransitionRecord
{
    uint32_t artboardId = 0;
    uint32_t smId = 0;
    uint32_t layerId = 0;
    uint32_t fromStateId = 0;
    uint32_t toStateId = 0;
    uint64_t tick = 0;
    std::vector<ArtboardPathSegment> path;
};

/// Record for a single listener performChanges call (used when profiling).
/// artboardId, smId, listenerNameId index into the profiler string table.
/// listenerType and hitEvent are raw ListenerType enum values; consumer maps to
/// string.
struct ListenerPerformChangeRecord
{
    uint32_t artboardId = 0;
    uint32_t smId = 0;
    uint32_t listenerNameId = 0;
    uint32_t listenerType = 0;
    uint32_t hitEvent = 0;
    uint32_t pointerId = 0;
    uint64_t tick = 0;
};

/// Singleton that wraps all MicroProfile calls and collects transition records
/// as they occur during advance/apply (when RIVE_MICROPROFILE is defined).
class RiveProfile
{
public:
    using FlushCallback =
        std::function<void(const std::vector<TransitionRecord>&)>;
    using ListenerPerformChangeFlushCallback =
        std::function<void(const std::vector<ListenerPerformChangeRecord>&)>;

    static RiveProfile& instance();

    RiveProfile(const RiveProfile&) = delete;
    RiveProfile& operator=(const RiveProfile&) = delete;

    // MicroProfile lifecycle (no-ops when RIVE_MICROPROFILE is not defined)
    void init();
    void frame();
    void endFrame();

    /// Profiling session (used by native binding for start/stop/dump).
    void start();
    void stop();
    bool isActive() const;

    /// Append header (once) and new frame events to \a buffer. Call after
    /// flushTransitionRecords() so the binding can merge transition data.
    void flushFrameDataTo(std::vector<uint8_t>& buffer);

    /// Record a state machine transition. Data is collected as it runs; no
    /// second pass over instances is required. No-op when RIVE_MICROPROFILE is
    /// undefined, the profiling session is inactive, no transition flush
    /// callback is set, or ProfileLogTransitionRecords is not set.
    /// If artboardForPath is non-null, the full artboard path is built and
    /// stored on the record (only when recording).
    void recordTransition(std::string artboardName,
                          std::string smName,
                          std::string layerName,
                          std::string fromStateName,
                          std::string toStateName,
                          class Artboard* artboardForPath = nullptr);

    /// Set the callback invoked when flushTransitionRecords() is called with
    /// all collected records for the current frame. The binding uses this to
    /// serialize records into the event buffer.
    void setFlushCallback(FlushCallback callback);

    /// Invoke the flush callback with accumulated transition records and clear
    /// the buffer. Call after advanceAndApply. No-op when callback is not set.
    void flushTransitionRecords();

    /// Record a listener performChanges call. No-op when RIVE_MICROPROFILE is
    /// undefined, the profiling session is inactive, no listener flush callback
    /// is set, or ProfileLogListenerPerformChanges is not set. listenerType and
    /// hitEvent are stored as raw uint (ListenerType enum); consumer maps to
    /// string.
    void recordListenerPerformChange(std::string artboardName,
                                     std::string smName,
                                     std::string listenerName,
                                     uint32_t listenerType,
                                     uint32_t hitEvent,
                                     uint32_t pointerId);

    /// Set the callback invoked when flushListenerPerformChangeRecords() is
    /// called with all collected listener records for the current frame.
    void setListenerPerformChangeFlushCallback(
        ListenerPerformChangeFlushCallback callback);

    /// Invoke the listener flush callback with accumulated records and clear
    /// the buffer. Call after flushTransitionRecords() when profiling.
    void flushListenerPerformChangeRecords();

    /// Set which profile data to log (bitmask of ProfileLogFlags). Combine
    /// flags to enable multiple log types.
    void setLogFlags(uint32_t flags) { m_logFlags = flags; }
    uint32_t logFlags() const { return m_logFlags; }

    /// Resolve a string to a stable id in the profiler string table. Used by
    /// transition records and any future record type that needs to log strings.
    /// Returns existing id if the string was seen before, otherwise assigns new
    /// id.
    uint32_t resolveStringId(const std::string& str);

    /// Return the current profiler string table (id index -> string). The
    /// binding uses this to emit 0x04 string table records; other record types
    /// use ids that index into this table.
    const std::vector<std::string>& getStringTable() const;

private:
    RiveProfile() = default;

    FlushCallback m_flushCallback;
    ListenerPerformChangeFlushCallback m_listenerPerformChangeFlushCallback;
    std::vector<TransitionRecord> m_transitionRecords;
    std::vector<ListenerPerformChangeRecord> m_listenerPerformChangeRecords;
    std::unordered_map<std::string, uint32_t> m_stringTable;
    std::vector<std::string> m_stringList;
    bool m_profilingActive = false;
    uint32_t m_logFlags = 3;
    bool m_headerWritten = false;
    uint64_t m_lastFlushedFrameIndex = 0;
};

} // namespace rive

#endif