#ifndef _RIVE_N_SLICER_DETAILS_HPP_
#define _RIVE_N_SLICER_DETAILS_HPP_
#include <stdio.h>
#include <unordered_map>

namespace rive
{
class AxisX;
class AxisY;
class Axis;
class Component;
class NSlicerTileMode;
enum class NSlicerTileModeType : int;

class NSlicerDetails
{
    friend class Axis;

protected:
    std::vector<Axis*> m_xs;
    std::vector<Axis*> m_ys;
    std::unordered_map<int, NSlicerTileModeType> m_tileModes; // patchIndex key

public:
    static NSlicerDetails* from(Component* component);
    int patchIndex(int patchX, int patchY);
    const std::vector<Axis*>& xs() const { return m_xs; }
    const std::vector<Axis*>& ys() const { return m_ys; }
    const std::unordered_map<int, NSlicerTileModeType>& tileModes()
    {
        return m_tileModes;
    }

    void addAxisX(Axis* axis);
    void addAxisY(Axis* axis);
    void addTileMode(int patchIndex, NSlicerTileModeType style);
    virtual void axisChanged() = 0; // only axis gets animated at runtime

    virtual ~NSlicerDetails() {}
};
} // namespace rive

#endif
