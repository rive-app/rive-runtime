#ifndef _RIVE_PATH_VERTEX_BASE_HPP_
#define _RIVE_PATH_VERTEX_BASE_HPP_
#include "component.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class PathVertexBase : public Component
	{
	public:
		static const int typeKey = 14;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case PathVertexBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int xPropertyKey = 24;
		static const int yPropertyKey = 25;

	private:
		double m_X = 0.0;
		double m_Y = 0.0;
	public:
		double x() const { return m_X; }
		void x(double value)
		{
			if (m_X == value)
			{
				return;
			}
			m_X = value;
			xChanged();
		}

		double y() const { return m_Y; }
		void y(double value)
		{
			if (m_Y == value)
			{
				return;
			}
			m_Y = value;
			yChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case xPropertyKey:
					m_X = CoreDoubleType::deserialize(reader);
					return true;
				case yPropertyKey:
					m_Y = CoreDoubleType::deserialize(reader);
					return true;
			}
			return Component::deserialize(propertyKey, reader);
		}

	protected:
		virtual void xChanged() {}
		virtual void yChanged() {}
	};
} // namespace rive

#endif