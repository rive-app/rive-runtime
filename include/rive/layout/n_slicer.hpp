#ifndef _RIVE_N_SLICER_HPP_
#define _RIVE_N_SLICER_HPP_
#include "rive/generated/layout/n_slicer_base.hpp"
#include "rive/layout/n_slicer_details.hpp"
#include <stdio.h>
#include <unordered_map>

namespace rive
{
class Image;
class SliceMesh;

class NSlicer : public NSlicerBase, public NSlicerDetails
{
private:
    std::unique_ptr<SliceMesh> m_sliceMesh; // the mesh that gets drawn

public:
    NSlicer();
    StatusCode onAddedDirty(CoreContext* context) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;

    Image* image();
    SliceMesh* sliceMesh() { return m_sliceMesh.get(); };
    void axisChanged() override;
};
} // namespace rive

#endif
