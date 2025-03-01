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

void TextFollowPathModifier::radialChanged()
{
    Text* text = textComponent();
    if (text != nullptr)
    {
        text->modifierShapeDirty();
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
    float pathLength = m_pathMeasure.length();
    if (pathLength == 0)
    {
        return cur;
    }

    const auto& paragraphLines = arg.paragraphLines;
    int lineIndexInParagraph = arg.lineIndexInParagraph;

    Vec2D position, tangent;
    Vec2D positionOnPath = arg.originPosition + arg.offset;
    bool pathClosed = m_localPath.isClosed();

    // Render along the tangent if overflowing on either ends
    if (!pathClosed && positionOnPath.x < 0)
    {
        auto result = m_pathMeasure.atDistance(0);
        tangent = result.tan;
        tangent = tangent.normalized();
        Vec2D zeroPosition = result.pos;
        float extra = -positionOnPath.x;
        position = Vec2D(zeroPosition.x - tangent.x * extra,
                         zeroPosition.y - tangent.y * extra);
    }
    else if (!pathClosed && positionOnPath.x > pathLength)
    {
        auto result = m_pathMeasure.atDistance(m_pathMeasure.length());
        tangent = result.tan;
        tangent = tangent.normalized();
        Vec2D termPosition = result.pos;
        float extra = positionOnPath.x - m_pathMeasure.length();
        position = Vec2D(termPosition.x + tangent.x * extra,
                         termPosition.y + tangent.y * extra);
    }
    else
    {
        // Closed path, or in the range of the path.
        // Use the percentage API to wrap around
        auto result = m_pathMeasure.atPercentage(positionOnPath.x / pathLength);
        position = result.pos;
        tangent = result.tan;
        tangent = tangent.normalized();
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

    // We assume the glyph would have been rendered on the
    // current line's baseline.
    Vec2D translation;
    if (radial())
    {
        float verticalSpacing = positionOnPath.y - curBaseline;
        Vec2D perpendicular(-tangent.y, tangent.x);
        translation = Vec2D(position.x + verticalSpacing * perpendicular.x,
                            position.y + verticalSpacing * perpendicular.y);
    }
    else
    {
        translation =
            Vec2D(position.x,
                  positionOnPath.y - curBaseline + position.y + lastBaseline);
    }

    TransformComponents tc;
    tc.x(translation.x);
    tc.y(translation.y);
    tc.rotation(std::atan2(tangent.y, tangent.x));
    return tc;
}
