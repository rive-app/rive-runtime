#ifndef _RIVE_STATE_MACHINE_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
	class StateMachineInput;
	class StateMachineLayer;
	class StateMachine;
	class StateMachineImporter : public ImportStackObject
	{
	private:
		StateMachine* m_StateMachine;

	public:
		StateMachineImporter(StateMachine* machine);
		const StateMachine* stateMachine() const { return m_StateMachine; }
		void addLayer(StateMachineLayer* layer);
		void addInput(StateMachineInput* input);
		StatusCode resolve() override;
		bool readNullObject() override;
	};
} // namespace rive
#endif
