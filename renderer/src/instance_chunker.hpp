/*
 * Copyright 2025 Rive
 */

#pragma once

#include <stdint.h>
#include <algorithm>
#include <cassert>

namespace rive::gpu
{
// Some Mali and PowerVR devices crash when issuing draw commands with a large
// instance count. This class breaks large instance ranges up into chunks that
// can be safely issued on affected GPUs.
class InstanceChunker
{
public:
    struct Chunk
    {
        uint32_t instanceCount;
        uint32_t baseInstance;
    };

    struct Iterator
    {
    public:
        uint32_t currentChunkInstanceCount() const
        {
            return std::min(endInstance - currentInstance,
                            maxSupportedInstancesPerDrawCommand);
        }

        Chunk operator*() const
        {
            return {currentChunkInstanceCount(), currentInstance};
        }

        Iterator& operator++()
        {
            currentInstance += currentChunkInstanceCount();
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            assert(endInstance == other.endInstance);
            assert(maxSupportedInstancesPerDrawCommand ==
                   other.maxSupportedInstancesPerDrawCommand);
            return currentInstance == other.currentInstance;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        uint32_t currentInstance;
        uint32_t endInstance;
        uint32_t maxSupportedInstancesPerDrawCommand;
    };

    InstanceChunker(uint32_t instanceCount,
                    uint32_t baseInstance,
                    uint32_t maxSupportedInstancesPerDrawCommand) :
        m_startInstance(baseInstance),
        m_endInstance(baseInstance + instanceCount),
        m_maxSupportedInstancesPerDrawCommand(
            maxSupportedInstancesPerDrawCommand)
    {}

    Iterator begin() const
    {
        return {m_startInstance,
                m_endInstance,
                m_maxSupportedInstancesPerDrawCommand};
    }

    Iterator end() const
    {
        return {m_endInstance,
                m_endInstance,
                m_maxSupportedInstancesPerDrawCommand};
    }

private:
    uint32_t m_startInstance;
    uint32_t m_endInstance;
    uint32_t m_maxSupportedInstancesPerDrawCommand;
};
} // namespace rive::gpu
