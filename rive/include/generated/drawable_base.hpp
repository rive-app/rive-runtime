#ifndef _RIVE_DRAWABLE_BASE_HPP_
#define _RIVE_DRAWABLE_BASE_HPP_
#include "core/field_types/core_int_type.hpp"
#include "node.hpp"
namespace rive
{
	class DrawableBase : public Node
	{
	public:
		static const int typeKey = 13;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case DrawableBase::typeKey:
				case NodeBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int drawOrderPropertyKey = 22;
		static const int blendModePropertyKey = 23;

	private:
		int m_DrawOrder = 0;
		int m_BlendMode = 0;
	public:
		int drawOrder() const { return m_DrawOrder; }
		void drawOrder(int value)
		{
			if (m_DrawOrder == value)
			{
				return;
			}
			m_DrawOrder = value;
			drawOrderChanged();
		}

		int blendMode() const { return m_BlendMode; }
		void blendMode(int value)
		{
			if (m_BlendMode == value)
			{
				return;
			}
			m_BlendMode = value;
			blendModeChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case drawOrderPropertyKey:
					m_DrawOrder = CoreIntType::deserialize(reader);
					return true;
				case blendModePropertyKey:
					m_BlendMode = CoreIntType::deserialize(reader);
					return true;
			}
			return Node::deserialize(propertyKey, reader);
		}

	protected:
		virtual void drawOrderChanged() {}
		virtual void blendModeChanged() {}
	};
} // namespace rive

#endif