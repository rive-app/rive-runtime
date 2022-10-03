#ifndef _RIVE_PARAMETRIC_PATH_HPP_
#define _RIVE_PARAMETRIC_PATH_HPP_
#include "rive/generated/shapes/parametric_path_base.hpp"
namespace rive
{
class ParametricPath : public ParametricPathBase
{
protected:
    void widthChanged() override;
    void heightChanged() override;
    void originXChanged() override;
    void originYChanged() override;
};
} // namespace rive

#endif