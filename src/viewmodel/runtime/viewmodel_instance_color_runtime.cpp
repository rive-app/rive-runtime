
#include "rive/viewmodel/runtime/viewmodel_instance_color_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

int ViewModelInstanceColorRuntime::value() const
{
    return m_viewModelInstanceValue->as<ViewModelInstanceColor>()
        ->propertyValue();
}

void ViewModelInstanceColorRuntime::value(int val)
{
    m_viewModelInstanceValue->as<ViewModelInstanceColor>()->propertyValue(val);
}

void ViewModelInstanceColorRuntime::rgb(int r, int g, int b)
{
    auto val =
        m_viewModelInstanceValue->as<ViewModelInstanceColor>()->propertyValue();
    int alpha = (val & 0xFF000000) >> 24;
    argb(alpha, r, g, b);
}

void ViewModelInstanceColorRuntime::alpha(int a)
{
    auto color =
        m_viewModelInstanceValue->as<ViewModelInstanceColor>()->propertyValue();
    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;
    argb(a, r, g, b);
}

void ViewModelInstanceColorRuntime::argb(int a, int r, int g, int b)
{
    auto color = (a << 24) | (r << 16) | (g << 8) | b;
    m_viewModelInstanceValue->as<ViewModelInstanceColor>()->propertyValue(
        color);
}
