#ifndef _RIVE_TEXT_FOLLOW_PATH_MODIFIER_HPP_
#define _RIVE_TEXT_FOLLOW_PATH_MODIFIER_HPP_
#include "rive/generated/text/text_follow_path_modifier_base.hpp"
#include "rive/math/path_measure.hpp"
#include "rive/math/transform_components.hpp"
namespace rive
{
struct TransformGlyphArg;
class TextFollowPathModifier : public TextFollowPathModifierBase
{
public:
    void buildDependencies() override;
    StatusCode onAddedClean(CoreContext* context) override;
    void update(ComponentDirt value) override;

    void reset(const Mat2D* inverseText);
    TransformComponents transformGlyph(const TransformComponents& cur,
                                       const TransformGlyphArg& arg);

private:
    RawPath m_worldPath;
    RawPath m_localPath;
    PathMeasure m_pathMeasure;
};
} // namespace rive

#endif