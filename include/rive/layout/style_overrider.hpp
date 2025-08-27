#ifndef _RIVE_STYLE_OVERRIDER_HPP_
#define _RIVE_STYLE_OVERRIDER_HPP_
#include <stdio.h>
#include "rive/artboard.hpp"
namespace rive
{
// class ArtboardInstance;

template <typename T> class StyleOverrider
{
public:
    StyleOverrider(T* sp) : m_styleProvider(sp) {}
    void updateHeightOverride(ArtboardInstance* artboardInstance)
    {

        if (artboardInstance == nullptr)
        {
            return;
        }
        auto isRow = m_styleProvider->isRow();
        if (m_styleProvider->instanceHeightScaleType() ==
            0) // LayoutScaleType::fixed
        {
            // If we're set to fixed, pass the unit value (points|percent)
            artboardInstance->heightIntrinsicallySizeOverride(false);
            artboardInstance->heightOverride(
                actualInstanceHeight(artboardInstance),
                m_styleProvider->instanceHeightUnitsValue(),
                isRow);
        }
        else if (m_styleProvider->instanceHeightScaleType() ==
                 1) // LayoutScaleType::fill
        {
            // If we're set to fill, pass auto
            artboardInstance->heightIntrinsicallySizeOverride(false);
            artboardInstance->heightOverride(
                actualInstanceHeight(artboardInstance),
                3,
                isRow);
        }
        else if (m_styleProvider->instanceWidthScaleType() == 2)
        {
            artboardInstance->heightIntrinsicallySizeOverride(true);
        }
        m_styleProvider->markHostingLayoutDirty(artboardInstance);
    }
    void updateWidthOverride(ArtboardInstance* artboardInstance)
    {

        if (artboardInstance == nullptr)
        {
            return;
        }
        auto isRow = m_styleProvider->isRow();
        if (m_styleProvider->instanceWidthScaleType() ==
            0) // LayoutScaleType::fixed
        {
            // If we're set to fixed, pass the unit value (points|percent)
            artboardInstance->widthIntrinsicallySizeOverride(false);
            artboardInstance->widthOverride(
                actualInstanceWidth(artboardInstance),
                m_styleProvider->instanceWidthUnitsValue(),
                isRow);
        }
        else if (m_styleProvider->instanceWidthScaleType() ==
                 1) // LayoutScaleType::fill
        {
            // If we're set to fill, pass auto
            artboardInstance->widthIntrinsicallySizeOverride(false);
            artboardInstance->widthOverride(
                actualInstanceWidth(artboardInstance),
                3,
                isRow);
        }
        else if (m_styleProvider->instanceWidthScaleType() == 2)
        {
            artboardInstance->widthIntrinsicallySizeOverride(true);
        }
        m_styleProvider->markHostingLayoutDirty(artboardInstance);
    }
    float actualInstanceWidth(ArtboardInstance* artboardInstance)
    {

        return m_styleProvider->instanceWidth() == -1.0f
                   ? artboardInstance->originalWidth()
                   : m_styleProvider->instanceWidth();
    }
    float actualInstanceHeight(ArtboardInstance* artboardInstance)
    {

        return m_styleProvider->instanceHeight() == -1.0f
                   ? artboardInstance->originalHeight()
                   : m_styleProvider->instanceHeight();
    }

private:
protected:
    T* m_styleProvider = nullptr;
};

} // namespace rive

#endif