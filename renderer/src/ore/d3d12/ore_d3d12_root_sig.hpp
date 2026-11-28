/*
 * Copyright 2026 Rive
 *
 * Private helper for building a per-pipeline ID3D12RootSignature from the
 * pipeline's ore::BindingMap (RFC v5 §3.2.2 / "Commit 7b").
 *
 * The rule: each `@group(g)` that has any bindings gets one or two dedicated
 * root parameters in the pipeline's root signature:
 *
 *   - One merged CBV/SRV/UAV descriptor table per group (if the group has
 *     any CBVs, SRVs, or UAVs). Ranges are packed in a canonical order
 *     (CBV → SRV → UAV) and appended contiguously into the GPU heap slice.
 *   - One sampler descriptor table per group (if the group has any samplers
 *     or comparison samplers). D3D12 requires samplers in a separate heap
 *     from CBV/SRV/UAV, so this is a distinct root parameter.
 *
 * All ranges use `RegisterSpace = 0` — the allocator emits SM5.0 HLSL with a
 * single `space=0`, and the per-group isolation comes from each group's
 * descriptor table spanning a disjoint `BaseShaderRegister` range. See RFC
 * v5 §3.2.2 for the full rationale and comparison to Dawn's SM5.1+ approach.
 *
 * `SetGraphicsRootDescriptorTable(groupRootParamIdx, ...)` called at
 * `setBindGroup(g, bg)` time is scoped to group g's root parameters only —
 * other groups' already-bound tables survive across calls. This is the
 * invariant that makes multi-group bind groups on D3D12 work at all.
 */

#pragma once

#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp" // D3D12GroupRootParams

#include <d3d12.h>
#include <wrl/client.h>
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace rive::ore
{

// D3D12-side alias for Ore's central `kMaxBindGroups`.
constexpr uint32_t kD3D12MaxGroups = kMaxBindGroups;

// The registers one kind takes in a group. An entry point may use only some
// of a group's bindings, so the table covers the ones between them too.
struct D3D12SlotSpan
{
  uint8_t min = 0xFF;
  uint8_t max = 0;
  void add(uint8_t slot)
  {
    min = std::min(min, slot);
    max = std::max(max, slot);
  }
  uint8_t base() const { return min == 0xFF ? 0 : min; }
  uint8_t count() const
  {
    return min == 0xFF ? 0 : static_cast<uint8_t>(max - min + 1);
  }
};

// Walk a `BindGroupLayoutDesc`'s entries and populate the layout's
// `D3D12GroupRootParams` shape (count + lowest register per kind).
// HLSL SM5 numbers its registers alike in every stage, so the register
// can come from whichever stage's nativeSlot field is set.
inline void tallyGroupFromLayoutDesc(
    const BindGroupLayoutDesc& desc,
    D3D12GroupRootParams* out)
{
  *out = D3D12GroupRootParams{};
  D3D12SlotSpan cbv, srv, uav, sampler;

  for (uint32_t i = 0; i < desc.entryCount; ++i)
  {
    const BindGroupLayoutEntry& e = desc.entries[i];
    const uint32_t slot32 = stageSharedSlot(e);
    if (slot32 == BindGroupLayoutEntry::kNativeSlotAbsent)
      continue;
    uint8_t slot = static_cast<uint8_t>(slot32);

    switch (e.kind)
    {
      case BindingKind::uniformBuffer:
        cbv.add(slot);
        break;
      case BindingKind::sampledTexture:
      case BindingKind::storageBufferRO:
        srv.add(slot);
        break;
      case BindingKind::storageBufferRW:
      case BindingKind::storageTexture:
        uav.add(slot);
        break;
      case BindingKind::sampler:
      case BindingKind::comparisonSampler:
        sampler.add(slot);
        break;
    }
  }
  out->cbvCount = cbv.count();
  out->srvCount = srv.count();
  out->uavCount = uav.count();
  out->samplerCount = sampler.count();
  out->cbvBaseGlobalSlot = cbv.base();
  out->srvBaseGlobalSlot = srv.base();
  out->uavBaseGlobalSlot = uav.base();
  out->samplerBaseGlobalSlot = sampler.base();
  out->cbvSrvUavRootParamIdx = -1;
  out->samplerRootParamIdx = -1;
}

// Build a per-pipeline `ID3D12RootSignature` from user-supplied
// BindGroupLayouts, and populate `groupOut[kD3D12MaxGroups]` with the
// composed root-parameter indices + per-kind ranges (mirrored from each
// layout's pre-computed shape). Returns `nullptr` on failure.
// A compute pipeline passes `outWorkgroupCountParam` and gets the root
// parameter its dispatches write their workgroup counts to.
inline Microsoft::WRL::ComPtr<ID3D12RootSignature> buildPerPipelineRootSig(
    ID3D12Device* device,
    BindGroupLayout* const* layouts,
    uint32_t layoutCount,
    D3D12GroupRootParams groupOut[kD3D12MaxGroups],
    HRESULT* outHr = nullptr,
    const char** outErrMsg = nullptr,
    int8_t* outWorkgroupCountParam = nullptr)
{
  // ── Step 1: build root parameters + ranges from layouts ──
  constexpr uint32_t kMaxParams = 2 * kD3D12MaxGroups + 1;
  D3D12_ROOT_PARAMETER rootParams[kMaxParams]{};
  D3D12_DESCRIPTOR_RANGE ranges[kMaxParams * 3]{};
  uint32_t paramCount = 0;
  uint32_t rangeCount = 0;

  for (uint32_t g = 0; g < kD3D12MaxGroups; ++g)
  {
    // Default-initialize this group's pipeline-side params.
    groupOut[g] = D3D12GroupRootParams{};

    // Skip groups with no layout supplied.
    if (g >= layoutCount || layouts == nullptr || layouts[g] == nullptr)
      continue;

    auto layout = lite_rtti_cast<BindGroupLayoutD3D12*>(layouts[g]);
    assert(layout && "BindGroupLayout must be a BindGroupLayoutD3D12");
    const D3D12GroupRootParams& src = layout->d3dGroupParams();
    D3D12GroupRootParams& out = groupOut[g];

    // Mirror counts + base global slots from the layout. Indices get
    // assigned below based on insertion order in the root sig.
    out.cbvCount = src.cbvCount;
    out.srvCount = src.srvCount;
    out.uavCount = src.uavCount;
    out.samplerCount = src.samplerCount;
    out.cbvBaseGlobalSlot = src.cbvBaseGlobalSlot;
    out.srvBaseGlobalSlot = src.srvBaseGlobalSlot;
    out.uavBaseGlobalSlot = src.uavBaseGlobalSlot;
    out.samplerBaseGlobalSlot = src.samplerBaseGlobalSlot;

    // Merged CBV / SRV / UAV table for this group.
    if (src.cbvCount || src.srvCount || src.uavCount)
    {
      out.cbvSrvUavRootParamIdx = static_cast<int8_t>(paramCount);

      uint32_t firstRange = rangeCount;
      uint32_t numRanges = 0;
      if (src.cbvCount)
      {
        D3D12_DESCRIPTOR_RANGE& r = ranges[rangeCount++];
        r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        r.NumDescriptors = src.cbvCount;
        r.BaseShaderRegister = src.cbvBaseGlobalSlot;
        r.RegisterSpace = 0;
        r.OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        ++numRanges;
      }
      if (src.srvCount)
      {
        D3D12_DESCRIPTOR_RANGE& r = ranges[rangeCount++];
        r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        r.NumDescriptors = src.srvCount;
        r.BaseShaderRegister = src.srvBaseGlobalSlot;
        r.RegisterSpace = 0;
        r.OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        ++numRanges;
      }
      if (src.uavCount)
      {
        D3D12_DESCRIPTOR_RANGE& r = ranges[rangeCount++];
        r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        r.NumDescriptors = src.uavCount;
        r.BaseShaderRegister = src.uavBaseGlobalSlot;
        r.RegisterSpace = 0;
        r.OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        ++numRanges;
      }

      D3D12_ROOT_PARAMETER& p = rootParams[paramCount++];
      p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      p.DescriptorTable.NumDescriptorRanges = numRanges;
      p.DescriptorTable.pDescriptorRanges = &ranges[firstRange];
      p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    // Sampler table for this group (separate heap type).
    if (src.samplerCount)
    {
      out.samplerRootParamIdx = static_cast<int8_t>(paramCount);

      D3D12_DESCRIPTOR_RANGE& r = ranges[rangeCount++];
      r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
      r.NumDescriptors = src.samplerCount;
      r.BaseShaderRegister = src.samplerBaseGlobalSlot;
      r.RegisterSpace = 0;
      r.OffsetInDescriptorsFromTableStart = 0;

      D3D12_ROOT_PARAMETER& p = rootParams[paramCount++];
      p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      p.DescriptorTable.NumDescriptorRanges = 1;
      p.DescriptorTable.pDescriptorRanges = &r;
      p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
  }

  const bool compute = outWorkgroupCountParam != nullptr;
  if (compute)
  {
    *outWorkgroupCountParam = static_cast<int8_t>(paramCount);
    D3D12_ROOT_PARAMETER& p = rootParams[paramCount++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p.Constants.ShaderRegister = kHlslWorkgroupCountRegister;
    p.Constants.RegisterSpace = 0;
    p.Constants.Num32BitValues = 3;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  }

  // ── Step 3: serialize + create ──
  D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
  rsDesc.NumParameters = paramCount;
  rsDesc.pParameters = paramCount > 0 ? rootParams : nullptr;
  rsDesc.Flags =
      compute ? D3D12_ROOT_SIGNATURE_FLAG_NONE
              : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
  HRESULT hr = D3D12SerializeRootSignature(
      &rsDesc,
      D3D_ROOT_SIGNATURE_VERSION_1,
      sigBlob.GetAddressOf(),
      errBlob.GetAddressOf());
  if (FAILED(hr))
  {
    if (outHr)
      *outHr = hr;
    if (outErrMsg)
      *outErrMsg = errBlob
                       ? static_cast<const char*>(errBlob->GetBufferPointer())
                       : "D3D12SerializeRootSignature failed";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
  hr = device->CreateRootSignature(
      0,
      sigBlob->GetBufferPointer(),
      sigBlob->GetBufferSize(),
      IID_PPV_ARGS(rootSig.GetAddressOf()));
  if (FAILED(hr))
  {
    if (outHr)
      *outHr = hr;
    if (outErrMsg)
      *outErrMsg = "CreateRootSignature failed";
    return nullptr;
  }
  return rootSig;
}

} // namespace rive::ore
