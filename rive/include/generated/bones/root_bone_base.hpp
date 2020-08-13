#ifndef _RIVE_ROOT_BONE_BASE_HPP_
#define _RIVE_ROOT_BONE_BASE_HPP_
#include "bones/bone.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class RootBoneBase : public Bone
	{
	protected:
		typedef Bone Super;

	public:
		static const int typeKey = 41;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case RootBoneBase::typeKey:
				case BoneBase::typeKey:
				case SkeletalComponentBase::typeKey:
				case TransformComponentBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int xPropertyKey = 90;
		static const int yPropertyKey = 91;

	private:
		float m_X = 0;
		float m_Y = 0;
	public:
		inline float x() const override { return m_X; }
		void x(float value)
		{
			if (m_X == value)
			{
				return;
			}
			m_X = value;
			xChanged();
		}

		inline float y() const override { return m_Y; }
		void y(float value)
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
			return Bone::deserialize(propertyKey, reader);
		}

	protected:
		virtual void xChanged() {}
		virtual void yChanged() {}
	};
} // namespace rive

#endif