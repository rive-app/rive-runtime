/*
 * Copyright 2026 Rive
 */

#pragma once

#include <mutex>
#include <unordered_set>

// A recording context can die before stragglers recorded against it. The live
// set lets late destructors no-op instead of writing into a freed recorder;
// the mutex serializes them against teardown.
namespace rive::cmd
{

inline std::mutex& recorderRegistryMutex()
{
    static std::mutex m;
    return m;
}
inline std::unordered_set<const void*>& liveRecorders()
{
    static std::unordered_set<const void*> s;
    return s;
}
inline void registerRecorder(const void* recorder)
{
    std::lock_guard<std::mutex> lock(recorderRegistryMutex());
    liveRecorders().insert(recorder);
}
inline void unregisterRecorder(const void* recorder)
{
    std::lock_guard<std::mutex> lock(recorderRegistryMutex());
    liveRecorders().erase(recorder);
}

} // namespace rive::cmd
