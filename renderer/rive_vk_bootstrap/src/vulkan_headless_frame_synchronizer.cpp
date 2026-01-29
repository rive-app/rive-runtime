/*
 * Copyright 2025 Rive
 */

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_headless_frame_synchronizer.hpp"
#include "logging.hpp"
#include "vulkan_error_handling.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
std::unique_ptr<VulkanHeadlessFrameSynchronizer>
VulkanHeadlessFrameSynchronizer::Create(VulkanInstance& instance,
                                        VulkanDevice& device,
                                        rive::rcp<rive::gpu::VulkanContext> vk,
                                        const Options& opts)
{
    bool success = true;

    // Private constructor, can't use make_unique
    auto outSynchronizer = std::unique_ptr<VulkanHeadlessFrameSynchronizer>{
        new VulkanHeadlessFrameSynchronizer(instance,
                                            device,
                                            std::move(vk),
                                            opts,
                                            &success)};

    CONFIRM_OR_RETURN_VALUE(success, nullptr);
    return outSynchronizer;
}

VulkanHeadlessFrameSynchronizer::VulkanHeadlessFrameSynchronizer(
    VulkanInstance& instance,
    VulkanDevice& device,
    rive::rcp<rive::gpu::VulkanContext>&& vk,
    const Options& opts,
    bool* successOut) :
    Super(instance,
          device,
          std::move(vk),
          {
              .initialFrameNumber = opts.initialFrameNumber,
              .externalGPUSynchronization = false,
          },
          successOut),
    m_imageFormat(opts.imageFormat),
    m_imageUsageFlags(opts.imageUsageFlags),
    m_width(opts.width),
    m_height(opts.height),
    m_image(context()->makeImage(
        {
            .imageType = VK_IMAGE_TYPE_2D,
            .format = m_imageFormat,
            .extent = {m_width, m_height, 1},
            .usage = m_imageUsageFlags,
        },
        "Vulkan Headless Frame Synchronizer Image")),
    m_imageView(
        context()->makeImageView(m_image,
                                 "Vulkan Headless Frame Synchronizer Image"))
{
    assert(opts.width > 0 && opts.height > 0 &&
           "Offscreen frame dimensions must be set");

    // the super class sets this value when it constructs, so check it before
    // re-setting it to false and continuing.
    if (!*successOut)
    {
        return;
    }

    *successOut = false;

    // Load all of the functions we care about
#define LOAD(name) LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this, name, instance);
    RIVE_VK_OFFSCREEN_FRAME_INSTANCE_COMMANDS(LOAD);
#undef LOAD

    *successOut = true;
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

VkResult VulkanHeadlessFrameSynchronizer::beginFrame()
{
    assert(!isFrameStarted());

    VK_RETURN_RESULT_ON_ERROR(Super::waitForFenceAndBeginFrame());
    m_isInFrame = true;

    return VK_SUCCESS;
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

VkResult VulkanHeadlessFrameSynchronizer::endFrame(
    const rive::gpu::vkutil::ImageAccess& lastAccess)
{
    assert(isFrameStarted());

    m_isInFrame = false;
    m_lastImageAccess = lastAccess;

    VK_RETURN_RESULT_ON_ERROR(Super::endFrame());

    return VK_SUCCESS;
}

} // namespace rive_vkb