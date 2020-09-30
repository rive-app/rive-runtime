#ifndef _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#define _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#include "animation/linear_animation.hpp"
#include <string>
#include <vector>
namespace rive
{
	class LinearAnimation;

	class IAnimationObserver
	{
	public:
		virtual ~IAnimationObserver() = 0;
		virtual void onFinished(std::string const&) = 0;
		virtual void onLoop(std::string const&) = 0;
		virtual void onPingPong(std::string const&) = 0;
	};

	class LinearAnimationInstance
	{
	private:
		std::vector<IAnimationObserver*> m_Observers;
		LinearAnimation* m_Animation = nullptr;
		float m_Time;
		int m_Direction;

	public:
		LinearAnimationInstance(LinearAnimation* animation);
		bool advance(float seconds);
		float time() const { return m_Time; }
		void time(float value);
		void apply(Artboard* artboard, float mix = 1.0f) const
		{
			m_Animation->apply(artboard, m_Time, mix);
		}

		void attachObserver(IAnimationObserver* observer)
		{
			m_Observers.push_back(observer);
		}

		void detachObserver(IAnimationObserver* observer)
		{
			auto position = std::find(m_Observers.begin(), m_Observers.end(), observer);
			if (position != m_Observers.end())
			{
				m_Observers.erase(position);
			}
		}

		void notifyObservers(Loop loopType);
	};
} // namespace rive
#endif