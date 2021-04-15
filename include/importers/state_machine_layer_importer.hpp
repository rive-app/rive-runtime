#ifndef _RIVE_STATE_MACHINE_LAYER_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_LAYER_IMPORTER_HPP_

#include "importers/import_stack.hpp"

namespace rive
{
	class StateMachineLayer;
	class LayerState;
	class ArtboardImporter;

	class StateMachineLayerImporter : public ImportStackObject
	{
	private:
		StateMachineLayer* m_Layer;
		ArtboardImporter* m_ArtboardImporter;

	public:
		StateMachineLayerImporter(StateMachineLayer* layer,
		                          ArtboardImporter* artboardImporter);
		void addState(LayerState* state);
		StatusCode resolve() override;
		bool readNullObject() override;
	};
} // namespace rive
#endif
