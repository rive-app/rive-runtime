#ifndef _RIVE_NODE_BASE_HPP_
#define _RIVE_NODE_BASE_HPP_
#include "container_component.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class NodeBase : public ContainerComponent
	{
	protected:
		typedef ContainerComponent Super;

	public:
		static const int typeKey = 2;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case NodeBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int xPropertyKey = 13;
		static const int yPropertyKey = 14;
		static const int rotationPropertyKey = 15;
		static const int scaleXPropertyKey = 16;
		static const int scaleYPropertyKey = 17;
		static const int opacityPropertyKey = 18;

	private:
		float m_X = 0;
		float m_Y = 0;
		float m_Rotation = 0;
		float m_ScaleX = 1;
		float m_ScaleY = 1;
		float m_Opacity = 1;
	public:
		float x() const { return m_X; }
		void x(float value)
		{
			if (m_X == value)
			{
				return;
			}
			m_X = value;
			xChanged();
		}

		float y() const { return m_Y; }
		void y(float value)
		{
			if (m_Y == value)
			{
				return;
			}
			m_Y = value;
			yChanged();
		}

		float rotation() const { return m_Rotation; }
		void rotation(float value)
		{
			if (m_Rotation == value)
			{
				return;
			}
			m_Rotation = value;
			rotationChanged();
		}

		float scaleX() const { return m_ScaleX; }
		void scaleX(float value)
		{
			if (m_ScaleX == value)
			{
				return;
			}
			m_ScaleX = value;
			scaleXChanged();
		}

		float scaleY() const { return m_ScaleY; }
		void scaleY(float value)
		{
			if (m_ScaleY == value)
			{
				return;
			}
			m_ScaleY = value;
			scaleYChanged();
		}

		float opacity() const { return m_Opacity; }
		void opacity(float value)
		{
			if (m_Opacity == value)
			{
				return;
			}
			m_Opacity = value;
			opacityChanged();
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
				case rotationPropertyKey:
					m_Rotation = CoreDoubleType::deserialize(reader);
					return true;
				case scaleXPropertyKey:
					m_ScaleX = CoreDoubleType::deserialize(reader);
					return true;
				case scaleYPropertyKey:
					m_ScaleY = CoreDoubleType::deserialize(reader);
					return true;
				case opacityPropertyKey:
					m_Opacity = CoreDoubleType::deserialize(reader);
					return true;
			}
			return ContainerComponent::deserialize(propertyKey, reader);
		}

	protected:
		virtual void xChanged() {}
		virtual void yChanged() {}
		virtual void rotationChanged() {}
		virtual void scaleXChanged() {}
		virtual void scaleYChanged() {}
		virtual void opacityChanged() {}
	};
} // namespace rive

#endif