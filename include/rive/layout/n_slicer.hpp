#ifndef _RIVE_N_SLICER_HPP_
#define _RIVE_N_SLICER_HPP_
#include "rive/generated/layout/n_slicer_base.hpp"
#include <stdio.h>
namespace rive
{
class Image;
class SliceMesh;
class Axis;
class NSlicerTileMode;

class NSlicer : public NSlicerBase
{
private:
    std::unique_ptr<SliceMesh> m_sliceMesh; // the mesh that gets drawn
    std::vector<Axis*> m_xs;
    std::vector<Axis*> m_ys;
    std::vector<NSlicerTileMode*> m_tileModes;

public:
    NSlicer();
    StatusCode onAddedDirty(CoreContext* context) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;

    Image* image();
    SliceMesh* sliceMesh() { return m_sliceMesh.get(); };
    const std::vector<Axis*>& xs() { return m_xs; }
    const std::vector<Axis*>& ys() { return m_ys; }

    void addAxisX(Axis* axis);
    void addAxisY(Axis* axis);
    void addTileMode(NSlicerTileMode* tileMode);
    void axisChanged(); // only axis gets animated at runtime
};
} // namespace rive

#endif
