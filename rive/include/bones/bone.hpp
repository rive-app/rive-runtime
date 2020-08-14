#ifndef _RIVE_BONE_HPP_
#define _RIVE_BONE_HPP_
#include "generated/bones/bone_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
	class Bone : public BoneBase
	{
	private:
		std::vector<Bone*> m_ChildBones;

	public:
		void onAddedClean(CoreContext* context) override;
		float x() const override;
		float y() const override;

		inline const std::vector<Bone*> childBones() { return m_ChildBones; }

		void addChildBone(Bone* bone);

	private:
		void lengthChanged() override;
	};
} // namespace rive

#endif