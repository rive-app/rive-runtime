/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_rstb_entry_container.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#include <atomic>
#include <vector>

namespace rive
{
class ShaderAsset;
} // namespace rive

namespace rive::ore
{
class BindGroup;
class BindGroupLayout;
class Pipeline;
class ShaderModule;
} // namespace rive::ore

namespace rive::ore::cmd
{

// Sees what scripts record, so a tool can replay the GPU work on another
// device, as rive-cli does on Dawn. Calls come from the recording thread.
class CreationObserver
{
public:
  virtual ~CreationObserver() = default;
  // Each time modules are built from an asset, with the entry names its
  // variant for the device's target carries.
  virtual void onShaderAsset(
      const ShaderAsset&,
      const std::vector<RstbEntryView>&)
  {}
  virtual void onShaderModule(const ShaderModule*, const ShaderModuleDesc&) {}
  virtual void onBindGroupLayout(
      const BindGroupLayout*,
      const BindGroupLayoutDesc&)
  {}
  virtual void onPipeline(const Pipeline*, const PipelineDesc&) {}
  virtual void onBindGroup(const BindGroup*, const BindGroupDesc&) {}
};

inline std::atomic<CreationObserver*>& creationObserverSlot()
{
  static std::atomic<CreationObserver*> slot{nullptr};
  return slot;
}

inline CreationObserver* creationObserver()
{
  return creationObserverSlot().load(std::memory_order_acquire);
}

inline void setCreationObserver(CreationObserver* observer)
{
  creationObserverSlot().store(observer, std::memory_order_release);
}

} // namespace rive::ore::cmd
