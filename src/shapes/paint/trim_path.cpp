#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/metrics_path.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

TrimPath::TrimPath() : m_TrimmedPath(makeRenderPath()) {}
TrimPath::~TrimPath() { delete m_TrimmedPath; }

StatusCode TrimPath::onAddedClean(CoreContext* context) {
    if (!parent()->is<Stroke>()) {
        return StatusCode::InvalidObject;
    }

    parent()->as<Stroke>()->addStrokeEffect(this);

    return StatusCode::Ok;
}

RenderPath* TrimPath::effectPath(MetricsPath* source) {
    if (m_RenderPath != nullptr) {
        return m_RenderPath;
    }

    // Source is always a containing (shape) path.
    const std::vector<MetricsPath*>& subPaths = source->paths();

    m_TrimmedPath->reset();
    auto renderOffset = std::fmod(std::fmod(offset(), 1.0f) + 1.0f, 1.0f);
    switch (modeValue()) {
        case 1: {
            float totalLength = source->length();
            auto startLength = totalLength * (start() + renderOffset);
            auto endLength = totalLength * (end() + renderOffset);

            if (endLength < startLength) {
                float swap = startLength;
                startLength = endLength;
                endLength = swap;
            }

            if (startLength > totalLength) {
                startLength -= totalLength;
                endLength -= totalLength;
            }

            int i = 0, subPathCount = (int)subPaths.size();
            while (endLength > 0) {
                auto path = subPaths[i % subPathCount];
                auto pathLength = path->length();

                if (startLength < pathLength) {
                    path->trim(startLength, endLength, true, m_TrimmedPath);
                    endLength -= pathLength;
                    startLength = 0;
                } else {
                    startLength -= pathLength;
                    endLength -= pathLength;
                }
                i++;
            }
        } break;

        case 2: {
            for (auto path : subPaths) {
                auto pathLength = path->length();
                auto startLength = pathLength * (start() + renderOffset);
                auto endLength = pathLength * (end() + renderOffset);
                if (endLength < startLength) {
                    auto length = startLength;
                    startLength = endLength;
                    endLength = length;
                }

                if (startLength > pathLength) {
                    startLength -= pathLength;
                    endLength -= pathLength;
                }
                path->trim(startLength, endLength, true, m_TrimmedPath);
                while (endLength > pathLength) {
                    startLength = 0;
                    endLength -= pathLength;
                    path->trim(startLength, endLength, true, m_TrimmedPath);
                }
            }
        } break;
    }

    m_RenderPath = m_TrimmedPath;
    return m_RenderPath;
}

void TrimPath::invalidateEffect() {
    m_RenderPath = nullptr;
    parent()->as<Stroke>()->parent()->addDirt(ComponentDirt::Paint);
}

void TrimPath::startChanged() { invalidateEffect(); }
void TrimPath::endChanged() { invalidateEffect(); }
void TrimPath::offsetChanged() { invalidateEffect(); }
void TrimPath::modeValueChanged() { invalidateEffect(); }
