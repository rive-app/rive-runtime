/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/ore_context.hpp"

#include <algorithm>
#include <cstdio>

namespace rive::ore
{

std::string Context::gpuPassLabel(const RenderPassDesc& desc)
{
    if (desc.label != nullptr && desc.label[0] != '\0')
    {
        return desc.label;
    }
    char shape[96];
    if (desc.colorCount > 0 && desc.colorAttachments[0].view != nullptr)
    {
        TextureView* view = desc.colorAttachments[0].view;
        snprintf(shape,
                 sizeof(shape),
                 "%ux%u fmt%u x%u ms%u%s",
                 view->width(),
                 view->height(),
                 static_cast<unsigned>(view->texture()->format()),
                 desc.colorCount,
                 view->texture()->sampleCount(),
                 desc.depthStencil.view != nullptr ? " +depth" : "");
    }
    else if (desc.depthStencil.view != nullptr)
    {
        snprintf(shape,
                 sizeof(shape),
                 "depth %ux%u",
                 desc.depthStencil.view->width(),
                 desc.depthStencil.view->height());
    }
    else
    {
        return "empty";
    }
    return shape;
}

void Context::publishGpuPassTimings(const std::vector<GpuPassTiming>& rows)
{
    for (const GpuPassTiming& row : rows)
    {
        m_gpuProfileTotals[row.label] += row.milliseconds;
    }
    if (++m_gpuProfileFrames < kGpuProfileReportFrames)
    {
        return;
    }
    std::vector<GpuPassTiming> report;
    report.reserve(m_gpuProfileTotals.size());
    for (const auto& [label, ms] : m_gpuProfileTotals)
    {
        report.push_back({label, ms / m_gpuProfileFrames});
    }
    std::sort(report.begin(), report.end(), [](const auto& a, const auto& b) {
        return a.milliseconds > b.milliseconds;
    });
    for (const GpuPassTiming& row : report)
    {
        printf("[ore gpu] %8.3f ms  %s\n", row.milliseconds, row.label.c_str());
    }
    m_gpuProfileTotals.clear();
    m_gpuProfileFrames = 0;
}

} // namespace rive::ore
