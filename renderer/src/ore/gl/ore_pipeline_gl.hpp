#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace rive::ore
{
class ContextGL;
class RenderPassGL;

class PipelineGL : public LITE_RTTI_OVERRIDE(Pipeline, PipelineGL)
{
public:
  PipelineGL(const PipelineDesc& desc) : lite_rtti_override(desc) {}
  ~PipelineGL() override;

  // A texture shadow samplers read at `unit` and plain samplers at `twin`,
  // since GL samples a unit through one sampler type.
  struct UnitTwin
  {
    uint16_t unit;
    uint16_t twin;
    bool operator==(const UnitTwin& o) const
    {
      return unit == o.unit && twin == o.twin;
    }
  };

  uint16_t twinOf(uint16_t unit) const
  {
    for (const UnitTwin& t : m_unitTwins)
    {
      if (t.unit == unit)
        return t.twin;
    }
    return BindingMap::kAbsent;
  }

  bool comparesAt(uint16_t unit) const
  {
    return std::find(m_shadowUnits.begin(), m_shadowUnits.end(), unit) !=
           m_shadowUnits.end();
  }

private:
  friend class ContextGL;
  friend class RenderPassGL;

  // Notes the units shadow samplers read and moves plain samplers off them to
  // a twin, since GL fails a draw that reads a unit through two sampler types.
  bool separateShadowUnits(std::string* error);

  unsigned int m_glProgram = 0; // GLuint; 0 = unlinked
  std::vector<UnitTwin> m_unitTwins;
  std::vector<uint16_t> m_shadowUnits;

  // A deferred link holds its modules: settling reads their fixup tables.
  BuildStatus m_status = BuildStatus::ready;
  rcp<ShaderModule> m_linkingVS;
  rcp<ShaderModule> m_linkingFS;
  std::string m_linkError;
};
} // namespace rive::ore
