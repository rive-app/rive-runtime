/*
 * Copyright 2025 Rive
 */

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_headless_frame_synchronizer.hpp"
#include "logging.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
VulkanHeadlessFrameSynchronizer::VulkanHeadlessFrameSynchronizer(
    VulkanInstance& instance,
    VulkanDevice& device,
    rive::rcp<rive::gpu::VulkanContext> vk,
    const Options& opts) :
    Super(instance,
          device,
          std::move(vk),
          {
              .initialFrameNumber = opts.initialFrameNumber,
              .externalGPUSynchronization = false,
          }),
    m_imageFormat(opts.imageFormat),
    m_imageUsageFlags(opts.imageUsageFlags),
    m_width(opts.width),
    m_height(opts.height),
    m_image(context()->makeImage({
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_imageFormat,
        .extent = {m_width, m_height, 1},
        .usage = m_imageUsageFlags,
    })),
    m_imageView(context()->makeImageView(m_image))
{
    assert(opts.width > 0 && opts.height > 0 &&
           "Offscreen frame dimensions must be set");

    // Load all of the functions we care about
#define LOAD(name) LOAD_REQUIRED_MEMBER_INSTANCE_FUNC(name, instance);
    RIVE_VK_OFFSCREEN_FRAME_INSTANCE_COMMANDS(LOAD);
#undef LOAD
}

VulkanHeadlessFrameSynchronizer::~VulkanHeadlessFrameSynchronizer()
{
    // Don't do anything until everything is flushed through.
    m_vkDeviceWaitIdle(vkDevice());
}

bool VulkanHeadlessFrameSynchronizer::isFrameStarted() const
{
    return m_isInFrame;
}

void VulkanHeadlessFrameSynchronizer::beginFrame()
{
    assert(!isFrameStarted());

    Super::waitForFenceAndBeginFrame();

    m_isInFrame = true;
}

void VulkanHeadlessFrameSynchronizer::queueImageCopy(
    rive::gpu::vkutil::ImageAccess* inOutLastAccess,
    rive::IAABB optPixelReadBounds)
{
    if (optPixelReadBounds.empty())
    {
        // Empty bounds means we want to just copy the entire texture
        optPixelReadBounds = rive::IAABB::MakeWH(m_width, m_height);
    }

    Super::queueImageCopy(*m_image,
                          m_imageFormat,
                          inOutLastAccess,
                          optPixelReadBounds);
}

void VulkanHeadlessFrameSynchronizer::endFrame(
    const rive::gpu::vkutil::ImageAccess& lastAccess)
{
    assert(isFrameStarted());

    m_lastImageAccess = lastAccess;

    Super::endFrame();

    m_isInFrame = false;
}

} // namespace rive_vkb