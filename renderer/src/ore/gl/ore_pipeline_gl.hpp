#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"

namespace rive::ore
{
class ContextGL;
class RenderPassGL;

class PipelineGL : public LITE_RTTI_OVERRIDE(Pipeline, PipelineGL)
{
public:
    PipelineGL(const PipelineDesc& desc) : lite_rtti_override(desc) {}
    ~PipelineGL() override = default;
    void onRefCntReachedZero() const override;

private:
    friend class ContextGL;
    friend class RenderPassGL;
    unsigned int m_glProgram = 0; // GLuint; 0 = unlinked
};
} // namespace rive::ore
