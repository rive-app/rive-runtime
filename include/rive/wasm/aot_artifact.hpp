#ifndef _RIVE_AOT_ARTIFACT_HPP_
#define _RIVE_AOT_ARTIFACT_HPP_

#include "rive/span.hpp"
#include <cstdint>
#include <cstdio>
#include <string>

namespace rive
{
// wamrc targets a linked device can load artifacts for.
constexpr const char* kAotTargetAndroidAarch64 = "android-aarch64";
constexpr const char* kAotTargetAndroidX64 = "android-x86_64";

inline bool isAotTarget(const std::string& target)
{
    return target == kAotTargetAndroidAarch64 || target == kAotTargetAndroidX64;
}

// Content hash of a wasm module, which names its artifacts.
inline uint64_t wasmModuleKey(Span<const uint8_t> module)
{
    uint64_t key = 0xcbf29ce484222325ull;
    for (uint8_t byte : module)
    {
        key = (key ^ byte) * 0x100000001b3ull;
    }
    return key;
}

// Species is empty for the -O3 artifact.
inline std::string aotArtifactName(uint64_t moduleKey, const char* species = "")
{
    char name[64];
    snprintf(name,
             sizeof(name),
             "%016llx%s.aot",
             (unsigned long long)moduleKey,
             species);
    return name;
}

// The pristine module wamrc compiles artifacts from.
inline std::string wasmModuleName(uint64_t moduleKey)
{
    char name[64];
    snprintf(name, sizeof(name), "%016llx.wasm", (unsigned long long)moduleKey);
    return name;
}
} // namespace rive

#endif
