#ifndef _RIVE_DRAWABLE_BASE_HPP_
#define _RIVE_DRAWABLE_BASE_HPP_
#include "core/field_types/core_uint_type.hpp"
#include "node.hpp"
namespace rive
{
	class DrawableBase : public Node
	{
	protected:
		typedef Node Super;

	public:
		static const int typeKey = 13;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case DrawableBase::typeKey:
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

		static const int blendModeValuePropertyKey = 23;

	private:
		int m_BlendModeValue = 3;
	public:
		inline int blendModeValue() const { return m_BlendModeValue; }
		void blendModeValue(int value)
		{
			if (m_BlendModeValue == value)
			{
				return;
			}
			m_BlendModeValue = value;
			blendModeValueChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case blendModeValuePropertyKey:
					m_BlendModeValue = CoreUintType::deserialize(reader);
					return true;
			}
			return Node::deserialize(propertyKey, reader);
		}

	protected:
		virtual void blendModeValueChanged() {}
	};
} // namespace rive

#endif