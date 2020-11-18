#ifndef _RIVE_PARAMETRIC_PATH_BASE_HPP_
#define _RIVE_PARAMETRIC_PATH_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/path.hpp"
namespace rive
{
	class ParametricPathBase : public Path
	{
	protected:
		typedef Path Super;

	public:
		static const int typeKey = 15;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case ParametricPathBase::typeKey:
				case PathBase::typeKey:
				case NodeBase::typeKey:
				case TransformComponentBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int widthPropertyKey = 20;
		static const int heightPropertyKey = 21;
		static const int originXPropertyKey = 123;
		static const int originYPropertyKey = 124;

	private:
		float m_Width = 0;
		float m_Height = 0;
		float m_OriginX = 0.5;
		float m_OriginY = 0.5;
	public:
		inline float width() const { return m_Width; }
		void width(float value)
		{
			if (m_Width == value)
			{
				return;
			}
			m_Width = value;
			widthChanged();
		}

		inline float height() const { return m_Height; }
		void height(float value)
		{
			if (m_Height == value)
			{
				return;
			}
			m_Height = value;
			heightChanged();
		}

		inline float originX() const { return m_OriginX; }
		void originX(float value)
		{
			if (m_OriginX == value)
			{
				return;
			}
			m_OriginX = value;
			originXChanged();
		}

		inline float originY() const { return m_OriginY; }
		void originY(float value)
		{
			if (m_OriginY == value)
			{
				return;
			}
			m_OriginY = value;
			originYChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case widthPropertyKey:
					m_Width = CoreDoubleType::deserialize(reader);
					return true;
				case heightPropertyKey:
					m_Height = CoreDoubleType::deserialize(reader);
					return true;
				case originXPropertyKey:
					m_OriginX = CoreDoubleType::deserialize(reader);
					return true;
				case originYPropertyKey:
					m_OriginY = CoreDoubleType::deserialize(reader);
					return true;
			}
			return Path::deserialize(propertyKey, reader);
		}

	protected:
		virtual void widthChanged() {}
		virtual void heightChanged() {}
		virtual void originXChanged() {}
		virtual void originYChanged() {}
	};
} // namespace rive

#endif