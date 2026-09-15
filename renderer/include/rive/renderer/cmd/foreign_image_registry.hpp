/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer.hpp"
#include "rive/renderer/cmd/render_handle.hpp"
#include <cassert>
#include <unordered_map>
#include <vector>

// Foreign image registry: any RenderImage a deferred drawImage references that
// is not a decoded DeferredRenderImage gets a flagged id here on first sight,
// so the stream carries an id, not a pointer. Entries are retained so they
// live to replay; the registry is per frame, keeping the set bounded.
namespace rive::cmd
{

class ForeignImageRegistry
{
public:
    // The flagged draw id for a foreign image; registers it on first sight.
    RenderHandle imageDrawId(RenderImage* image)
    {
        auto it = m_imageToId.find(image);
        RenderHandle id;
        if (it != m_imageToId.end())
        {
            id = it->second;
        }
        else
        {
            id = static_cast<RenderHandle>(m_images.size());
            // The unflagged id must fit under the flag bit.
            assert(id <= kCanvasHandleMask);
            m_images.push_back(ref_rcp(image));
            m_imageToId[image] = id;
        }
        return kCanvasHandleFlag | id;
    }

    // Replay time lookup of the real image by unflagged id.
    RenderImage* imageAt(RenderHandle id) const
    {
        return id < m_images.size() ? m_images[id].get() : nullptr;
    }

    // Retained id indexed images for the consumer snapshot.
    const std::vector<rcp<RenderImage>>& images() const { return m_images; }

    void reset()
    {
        m_images.clear();
        m_imageToId.clear();
    }

private:
    std::vector<rcp<RenderImage>> m_images;
    std::unordered_map<RenderImage*, RenderHandle> m_imageToId;
};

} // namespace rive::cmd
