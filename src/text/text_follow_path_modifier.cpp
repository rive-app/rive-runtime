#include "rive/core_context.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_follow_path_modifier.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/transform_component.hpp"

using namespace rive;

// Path -> this -> text
void TextFollowPathModifier::buildDependencies()
{
    if (m_Target != nullptr && m_Target->is<Shape>())
    {
        Shape* shape = static_cast<Shape*>(m_Target);
        shape->pathComposer()->addDependent(this);
    }

    else if (m_Target != nullptr && m_Target->is<Path>())
    {
        Path* path = static_cast<Path*>(m_Target);
        path->addDependent(this);
    }

    Text* text = textComponent();
    if (text != nullptr)
    {
        addDependent(text);
    }
}

StatusCode TextFollowPathModifier::onAddedClean(CoreContext* context)
{
    if (m_Target != nullptr)
    {
        if (m_Target->is<Shape>())
        {
            Shape* shape = static_cast<Shape*>(m_Target);
            shape->addFlags(PathFlags::followPath);
        }
        else if (m_Target->is<Path>())
        {
            Path* path = static_cast<Path*>(m_Target);
            path->addFlags(PathFlags::followPath);
        }
    }
    return Super::onAddedClean(context);
}

void TextFollowPathModifier::update(ComponentDirt value)
{
    std::vector<Path*> paths;
    if (m_Target != nullptr && m_Target->is<Shape>())
    {
        auto shape = m_Target->as<Shape>();
        for (auto path : shape->paths())
        {
            paths.push_back(path);
        }
    }
    else if (m_Target != nullptr && m_Target->is<Path>())
    {
        paths.push_back(m_Target->as<Path>());
    }

    m_worldPath.rewind();
    for (auto path : paths)
    {
        m_worldPath.addPath(path->rawPath(), &path->pathTransform());
    }
}

void TextFollowPathModifier::reset(const Mat2D* inverseText)
{
    if (m_Target == nullptr)
    {
        m_pathMeasure = PathMeasure();
        return;
    }
    m_localPath.rewind();
    m_localPath.addPath(m_worldPath, inverseText);
    m_pathMeasure = PathMeasure(&m_localPath, 0.1f);
}

TransformComponents TextFollowPathModifier::transformGlyph(
    const TransformComponents& cur,
    const TransformGlyphArg& arg)
{
    if (m_pathMeasure.length() == 0)
    {
        return cur;
    }

    const Vec2D& originPosition = arg.originPosition;
    const auto& paragraphLines = arg.paragraphLines;
    int lineIndexInParagraph = arg.lineIndexInParagraph;

    Vec2D position, tangent;

    if (originPosition.x < 0)
    {
        auto result = m_pathMeasure.atDistance(0);
        tangent = result.tan;
        tangent = tangent.normalized();
        Vec2D zeroPosition = result.pos;
        float extra = -originPosition.x;
        position = Vec2D(zeroPosition.x - tangent.x * extra,
                         zeroPosition.y - tangent.y * extra);
    }
    else if (originPosition.x <= m_pathMeasure.length())
    {
        // Render text along the path if in
        // bounds.
        auto result = m_pathMeasure.atDistance(originPosition.x);
        position = result.pos;
        tangent = result.tan;
    }
    else
    {
        // Otherwise continue rendering along the tangent.
        auto result = m_pathMeasure.atDistance(m_pathMeasure.length());
        tangent = result.tan;
        tangent = tangent.normalized();
        Vec2D termPosition = result.pos;
        float extra = originPosition.x - m_pathMeasure.length();
        position = Vec2D(termPosition.x + tangent.x * extra,
                         termPosition.y + tangent.y * extra);
    }

    // Find out the baseline to sit on
    float lastBaseline = 0;
    float curBaseline = 0;
    auto validLine = [&](int i) -> bool {
        return i >= 0 && i < paragraphLines.size();
    };
    int lastLineIndex = lineIndexInParagraph - 1;
    if (validLine(lastLineIndex))
    {
        lastBaseline = paragraphLines[lastLineIndex].baseline;
    }
    if (validLine(lineIndexInParagraph))
    {
        curBaseline = paragraphLines[lineIndexInParagraph].baseline;
    }

    TransformComponents tc;
    Vec2D translation =
        Vec2D(position.x,
              originPosition.y + position.y + lastBaseline - curBaseline);

    tc.x(translation.x);
    tc.y(translation.y);
    tc.rotation(std::atan2(tangent.y, tangent.x));
    return tc;
}
