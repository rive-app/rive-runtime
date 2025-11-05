#include "webgpu/webgpu_wagyu.h"

#define WGPU_WAGYU_HEADER_VERSION_MAJOR 1
#define WGPU_WAGYU_HEADER_VERSION_MINOR 0

#define WASM_EXPORT(NAME) __attribute__((used, export_name(#NAME))) NAME

typedef struct WGPUWagyuVersionInfo
{
    uint32_t webgpuMajor;
    uint32_t webgpuMinor;
    uint32_t wagyuExtensionLevel;
} WGPUWagyuVersionInfo;

void WASM_EXPORT(wgpuWagyuGetCompiledVersion)(WGPUWagyuVersionInfo *versionInfo)
{
    if (versionInfo) {
        versionInfo->webgpuMajor         = WGPU_WAGYU_HEADER_VERSION_MAJOR;
        versionInfo->webgpuMinor         = WGPU_WAGYU_HEADER_VERSION_MINOR;
        versionInfo->wagyuExtensionLevel = WGPU_WAGYU_EXTENSION_LEVEL;
    }
}
