#ifndef _RIVE_TRANSFORM_COMPONENT_HPP_
#define _RIVE_TRANSFORM_COMPONENT_HPP_
#include "generated/transform_component_base.hpp"
#include "math/mat2d.hpp"

namespace rive
{
	class TransformComponent : public TransformComponentBase
	{
	private:
		Mat2D m_Transform;
		Mat2D m_WorldTransform;
		float m_RenderOpacity = 0.0f;
		TransformComponent* m_ParentTransformComponent = nullptr;

	public:
		StatusCode onAddedClean(CoreContext* context) override;
		void buildDependencies() override;
		void update(ComponentDirt value) override;
		void updateTransform();
		void updateWorldTransform();
		void markTransformDirty();
		void markWorldTransformDirty();

		/// Opacity inherited by any child of this transform component. This'll
		/// later get overridden by effect layers.
		virtual float childOpacity() { return m_RenderOpacity; }
		float renderOpacity() const { return m_RenderOpacity; }

		const Mat2D& transform() const;
		const Mat2D& worldTransform() const;

		virtual float x() const = 0;
		virtual float y() const = 0;

		void rotationChanged() override;
		void scaleXChanged() override;
		void scaleYChanged() override;
		void opacityChanged() override;
	};
} // namespace rive

#endif