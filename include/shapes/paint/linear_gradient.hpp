#ifndef _RIVE_LINEAR_GRADIENT_HPP_
#define _RIVE_LINEAR_GRADIENT_HPP_
#include "generated/shapes/paint/linear_gradient_base.hpp"
#include "math/vec2d.hpp"
#include "shapes/paint/shape_paint_mutator.hpp"
#include <vector>
namespace rive
{
	class Node;
	class GradientStop;

	class LinearGradient : public LinearGradientBase, public ShapePaintMutator
	{
	private:
		std::vector<GradientStop*> m_Stops;
		bool m_PaintsInWorldSpace;
		Node* m_ShapePaintContainer = nullptr;

	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override
		{
			return StatusCode::Ok;
		}
		void addStop(GradientStop* stop);
		void update(ComponentDirt value) override;
		bool paintsInWorldSpace() const;
		void paintsInWorldSpace(bool value);
		void markGradientDirty();
		void markStopsDirty();

	protected:
		void buildDependencies() override;
		void startXChanged() override;
		void startYChanged() override;
		void endXChanged() override;
		void endYChanged() override;
		void opacityChanged() override;
		void renderOpacityChanged() override;
		virtual void makeGradient(const Vec2D& start, const Vec2D& end);
	};
} // namespace rive

#endif