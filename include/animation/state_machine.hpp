#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "generated/animation/state_machine_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
	class StateMachineLayer;
	class StateMachineInput;
	class StateMachineImporter;
	class StateMachine : public StateMachineBase
	{
		friend class StateMachineImporter;

	private:
		std::vector<StateMachineLayer*> m_Layers;
		std::vector<StateMachineInput*> m_Inputs;

		void addLayer(StateMachineLayer* layer);
		void addInput(StateMachineInput* input);

	public:
		~StateMachine();
		StatusCode import(ImportStack& importStack) override;

		size_t layerCount() { return m_Layers.size(); }
		size_t inputCount() { return m_Inputs.size(); }

		StateMachineInput* input(std::string name);
		StateMachineInput* input(size_t index);
		StateMachineLayer* layer(std::string name);
		StateMachineLayer* layer(size_t index);

		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;
	};
} // namespace rive

#endif