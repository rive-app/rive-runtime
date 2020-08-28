#ifndef _RIVE_PATH_VERTEX_HPP_
#define _RIVE_PATH_VERTEX_HPP_
#include "bones/weight.hpp"
#include "generated/shapes/path_vertex_base.hpp"
#include "math/mat2d.hpp"
namespace rive
{
	class PathVertex : public PathVertexBase
	{
		friend class Weight;

	private:
		Weight* m_Weight = nullptr;
		void weight(Weight* value) { m_Weight = value; }

	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override
		{
			return StatusCode::Ok;
		}
		template <typename T> T* weight() { return m_Weight->as<T>(); }
		virtual void deform(Mat2D& worldTransform, float* boneTransforms);
		bool hasWeight() { return m_Weight != nullptr; }
		Vec2D renderTranslation();

	protected:
		void markPathDirty();
		void xChanged() override;
		void yChanged() override;

#ifdef TESTING
	public:
		Weight* weight() { return m_Weight; }
#endif
	};
} // namespace rive

#endif