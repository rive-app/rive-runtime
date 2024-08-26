#ifndef _RIVE_N_SLICER_HPP_
#define _RIVE_N_SLICER_HPP_
#include "rive/generated/layout/n_slicer_base.hpp"
#include <stdio.h>
#include <unordered_map>

namespace rive
{
class Image;
class SliceMesh;
class Axis;
class NSlicerTileMode;
enum class NSlicerTileModeType : int;

class NSlicer : public NSlicerBase
{
private:
    std::unique_ptr<SliceMesh> m_sliceMesh; // the mesh that gets drawn
    std::vector<Axis*> m_xs;
    std::vector<Axis*> m_ys;
    std::unordered_map<int, NSlicerTileModeType> m_tileModes; // patchIndex key

public:
    NSlicer();
    StatusCode onAddedDirty(CoreContext* context) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;

    Image* image();
    SliceMesh* sliceMesh() { return m_sliceMesh.get(); };
    int patchIndex(int patchX, int patchY);
    const std::vector<Axis*>& xs() { return m_xs; }
    const std::vector<Axis*>& ys() { return m_ys; }
    const std::unordered_map<int, NSlicerTileModeType>& tileModes() { return m_tileModes; }

    void addAxisX(Axis* axis);
    void addAxisY(Axis* axis);
    void addTileMode(int patchIndex, NSlicerTileModeType style);
    void axisChanged(); // only axis gets animated at runtime
};
} // namespace rive

#endif
