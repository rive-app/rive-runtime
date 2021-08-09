#ifndef _RIVE_SKINNABLE_HPP_
#define _RIVE_SKINNABLE_HPP_

namespace rive
{
	class Skin;
	class Component;

	class Skinnable
	{
		friend class Skin;

	private:
		Skin* m_Skin = nullptr;

	protected:
		void skin(Skin* skin);
	public:
		Skin* skin() const { return m_Skin; }
		virtual void markSkinDirty() = 0;

		static Skinnable* from(Component* component);
	};
} // namespace rive

#endif