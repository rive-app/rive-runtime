#include "shapes/paint/trim_path.hpp"
#include "shapes/metrics_path.hpp"
#include "shapes/paint/stroke.hpp"

using namespace rive;

TrimPath::TrimPath() : m_TrimmedPath(makeRenderPath()) {}
TrimPath::~TrimPath() { delete m_TrimmedPath; }

StatusCode TrimPath::onAddedClean(CoreContext* context)
{
	if (!parent()->is<Stroke>())
	{
		return StatusCode::InvalidObject;
	}

	parent()->as<Stroke>()->addStrokeEffect(this);

	return StatusCode::Ok;
}

RenderPath* TrimPath::effectPath(MetricsPath* source)
{
	if (m_RenderPath != nullptr)
	{
		return m_RenderPath;
	}

	float totalLength = source->computeLength();
	m_TrimmedPath->reset();
	source->trim(0, totalLength * offset(), m_TrimmedPath);
	// m_TrimmedPath = makeRenderPath();
	// m_TrimmedPath->reset();
	// m_TrimmedPath->moveTo(100, 100);
	// m_TrimmedPath->lineTo(100 + 1000 * offset(), 100);

	m_RenderPath = m_TrimmedPath;
	return m_RenderPath;
}

void TrimPath::invalidateEffect()
{
	m_RenderPath = nullptr;
	parent()->as<Stroke>()->parent()->addDirt(ComponentDirt::Paint);
}

void TrimPath::startChanged() { invalidateEffect(); }
void TrimPath::endChanged() { invalidateEffect(); }
void TrimPath::offsetChanged() { invalidateEffect(); }
void TrimPath::modeValueChanged() { invalidateEffect(); }