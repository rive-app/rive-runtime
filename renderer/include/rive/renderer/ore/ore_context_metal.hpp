/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#import <Metal/Metal.h>

#include <atomic>
#include <memory>

namespace rive::ore
{

class RenderPassMetal;
class BindGroupMetal;
class TextureMetal;

class BufferMetal;

class ContextMetal : public Context
{
public:
  static std::unique_ptr<ContextMetal> Make(
      id<MTLDevice> device,
      id<MTLCommandQueue> queue);

  ~ContextMetal() override;

  rcp<Buffer> makeBuffer(const BufferDesc& desc) override;
  rcp<Texture> makeTexture(const TextureDesc& desc) override;
  rcp<TextureView> makeTextureViewImpl(const TextureViewDesc& desc) override;
  rcp<Sampler> makeSampler(const SamplerDesc& desc) override;
  rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) override;
  rcp<BindGroupLayout> makeBindGroupLayout(
      const BindGroupLayoutDesc& desc) override;
  rcp<Pipeline> makePipeline(
      const PipelineDesc& desc,
      std::string* outError = nullptr) override;
  rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) override;

  std::unique_ptr<RenderPass> beginRenderPass(
      const RenderPassDesc& desc,
      std::string* outError = nullptr) override;

  rcp<ComputePipeline> makeComputePipelineImpl(
      const ComputePipelineDesc& desc,
      std::string* outError) override;
  std::unique_ptr<ComputePass> beginComputePass(
      const ComputePassDesc& desc,
      std::string* outError = nullptr) override;

  void beginFrame(const FrameDescriptor&) override;
  void endFrame() override;
  void waitForGPU() override;

  // Metal drains the recorded frame in endFrame before commit.
  bool usesDeferredFrameReplay() const override { return true; }

  rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
  rcp<TextureView> wrapRenderTarget(gpu::RenderTarget* target) override;
  rcp<TextureView> wrapRiveTexture(
      gpu::Texture* gpuTex,
      uint32_t width,
      uint32_t height) override;

  ShaderTarget shaderTarget() const override { return ShaderTarget::msl; }

  // Buffer versioning serials. currentSerial is the command buffer being
  // recorded. completedSerial is the highest the GPU has finished, so a
  // backing at or below it can be recycled. See ore_buffer_metal.mm.
  uint64_t currentSerial() const { return m_currentSerial; }
  uint64_t completedSerial() const
  {
    return m_completedSerial->load(std::memory_order_relaxed);
  }
  id<MTLDevice> device() const { return m_mtlDevice; }

  ContextMetal(const ContextMetal&) = delete;
  ContextMetal& operator=(const ContextMetal&) = delete;

private:
  friend class RenderPassMetal;
  friend class ComputePassMetal;
  friend class BufferMetal;
  friend class BindGroupMetal;
  friend class TextureMetal;

  ContextMetal() : Context(nullptr) {}

  // Metal implementation helpers — defined in ore_context_metal.mm.
  // The public make*/begin*/wrap* overrides delegate to these.
  void mtlPopulateFeatures(id<MTLDevice> device);
  rcp<Buffer> mtlMakeBuffer(const BufferDesc& desc);
  rcp<Texture> mtlMakeTexture(const TextureDesc& desc);
  rcp<TextureView> mtlMakeTextureView(const TextureViewDesc& desc);
  rcp<Sampler> mtlMakeSampler(const SamplerDesc& desc);
  rcp<ShaderModule> mtlMakeShaderModule(const ShaderModuleDesc& desc);
  rcp<Pipeline> mtlMakePipeline(
      const PipelineDesc& desc,
      std::string* outError);
  rcp<BindGroup> mtlMakeBindGroup(const BindGroupDesc& desc);
  std::unique_ptr<RenderPass> mtlBeginRenderPass(
      const RenderPassDesc& desc,
      std::string* outError);
  rcp<TextureView> mtlWrapTarget(gpu::RenderTarget* target);

  id<MTLDevice> m_mtlDevice = nil;
  id<MTLCommandQueue> m_mtlQueue = nil;
  id<MTLCommandBuffer> m_mtlCommandBuffer = nil;

  // A write into a storage buffer's memory would land ahead of passes
  // encoded but not yet run, so its bytes are copied in command order.
  void writeStorageBuffer(
      BufferMetal* buffer,
      const void* data,
      uint32_t size,
      uint32_t offset);

  void replayPendingFrame();

  // Metal allows one encoder at a time on a command buffer.
  uint32_t m_openEncoders = 0;
  void encoderEnded()
  {
    if (m_openEncoders > 0)
      m_openEncoders--;
  }

  std::vector<rcp<BindGroup>> m_deferredBindGroups;

  // Serial of the command buffer being recorded, bumped each frame.
  uint64_t m_currentSerial = 0;
  // Highest serial the GPU has finished. Written on the completion handler
  // thread, read on the recording thread, hence atomic. shared_ptr so the
  // handler stays valid even if the context dies first.
  std::shared_ptr<std::atomic<uint64_t>>
      m_completedSerial = std::make_shared<std::atomic<uint64_t>>(0);
};

} // namespace rive::ore
