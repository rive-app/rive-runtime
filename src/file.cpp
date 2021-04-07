#include "file.hpp"
#include "animation/animation.hpp"
#include "core/field_types/core_color_type.hpp"
#include "core/field_types/core_double_type.hpp"
#include "core/field_types/core_string_type.hpp"
#include "core/field_types/core_uint_type.hpp"
#include "generated/core_registry.hpp"
#include "importers/artboard_importer.hpp"
#include "importers/import_stack.hpp"
#include "importers/keyed_object_importer.hpp"
#include "importers/keyed_property_importer.hpp"
#include "importers/linear_animation_importer.hpp"
#include "importers/state_machine_importer.hpp"
#include "importers/state_machine_layer_importer.hpp"
#include "importers/layer_state_importer.hpp"
#include "importers/state_transition_importer.hpp"
#include "animation/any_state.hpp"
#include "animation/entry_state.hpp"
#include "animation/exit_state.hpp"
#include "animation/animation_state.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

// Import a single Rive runtime object.
// Used by the file importer.
static Core* readRuntimeObject(BinaryReader& reader,
                               const RuntimeHeader& header)
{
	auto coreObjectKey = reader.readVarUint();
	auto object = CoreRegistry::makeCoreInstance((int)coreObjectKey);
	while (true)
	{
		auto propertyKey = reader.readVarUint();
		if (propertyKey == 0)
		{
			// Terminator. https://media.giphy.com/media/7TtvTUMm9mp20/giphy.gif
			break;
		}

		if (reader.didOverflow())
		{
			delete object;
			return nullptr;
		}
		if (object == nullptr || !object->deserialize((int)propertyKey, reader))
		{
			// We have an unknown object or property, first see if core knows
			// the property type.
			int id = CoreRegistry::propertyFieldId((int)propertyKey);
			if (id == -1)
			{
				// No, check if it's in toc.
				id = header.propertyFieldId((int)propertyKey);
			}

			if (id == -1)
			{
				// Still couldn't find it, give up.
				fprintf(
				    stderr,
				    "Unknown property key %llu, missing from property ToC.\n",
				    propertyKey);
				delete object;
				return nullptr;
			}

			switch (id)
			{
				case CoreUintType::id:
					CoreUintType::deserialize(reader);
					break;
				case CoreStringType::id:
					CoreStringType::deserialize(reader);
					break;
				case CoreDoubleType::id:
					CoreDoubleType::deserialize(reader);
					break;
				case CoreColorType::id:
					CoreColorType::deserialize(reader);
					break;
			}
		}
	}
	if (object == nullptr)
	{
		// fprintf(stderr,
		//         "File contains an unknown object with coreType %llu, which "
		//         "this runtime doesn't understand.\n",
		//         coreObjectKey);
		return nullptr;
	}
	return object;
}

File::~File()
{
	for (auto artboard : m_Artboards)
	{
		delete artboard;
	}
	delete m_Backboard;
}

// Import a Rive file from a file handle
ImportResult File::import(BinaryReader& reader, File** importedFile)
{
	RuntimeHeader header;
	if (!RuntimeHeader::read(reader, header))
	{
		fprintf(stderr, "Bad header\n");
		return ImportResult::malformed;
	}
	if (header.majorVersion() != majorVersion)
	{
		fprintf(stderr,
		        "Unsupported version %u.%u expected %u.%u.\n",
		        header.majorVersion(),
		        header.minorVersion(),
		        majorVersion,
		        minorVersion);
		return ImportResult::unsupportedVersion;
	}
	auto file = new File();
	auto result = file->read(reader, header);
	if (result != ImportResult::success)
	{
		delete file;
		return result;
	}
	*importedFile = file;
	return result;
}

ImportResult File::read(BinaryReader& reader, const RuntimeHeader& header)
{
	ImportStack importStack;
	while (!reader.reachedEnd())
	{
		auto object = readRuntimeObject(reader, header);
		if (object == nullptr)
		{
			importStack.readNullObject();
			continue;
		}
		ImportStackObject* stackObject = nullptr;
		auto stackType = object->coreType();

		switch (stackType)
		{
			case Artboard::typeKey:
				stackObject = new ArtboardImporter(object->as<Artboard>());
				break;
			case LinearAnimation::typeKey:
				stackObject =
				    new LinearAnimationImporter(object->as<LinearAnimation>());
				break;
			case KeyedObject::typeKey:
				stackObject =
				    new KeyedObjectImporter(object->as<KeyedObject>());
				break;
			case KeyedProperty::typeKey:
			{
				auto importer = importStack.latest<LinearAnimationImporter>(
				    LinearAnimation::typeKey);
				if (importer == nullptr)
				{
					return ImportResult::malformed;
				}
				stackObject = new KeyedPropertyImporter(
				    importer->animation(), object->as<KeyedProperty>());
				break;
			}
			case StateMachine::typeKey:
				stackObject =
				    new StateMachineImporter(object->as<StateMachine>());
				break;
			case StateMachineLayer::typeKey:
			{
				auto artboardImporter =
				    importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
				if (artboardImporter == nullptr)
				{
					return ImportResult::malformed;
				}

				stackObject = new StateMachineLayerImporter(
				    object->as<StateMachineLayer>(), artboardImporter);

				break;
			}
			case EntryState::typeKey:
			case ExitState::typeKey:
			case AnyState::typeKey:
			case AnimationState::typeKey:
				stackObject = new LayerStateImporter(object->as<LayerState>());
				stackType = LayerState::typeKey;
				break;
			case StateTransition::typeKey:
				stackObject =
				    new StateTransitionImporter(object->as<StateTransition>());
				break;
		}
		if (importStack.makeLatest(stackType, stackObject) != StatusCode::Ok)
		{
			// Some previous stack item didn't resolve.
			return ImportResult::malformed;
		}
		if (object->import(importStack) == StatusCode::Ok)
		{
			switch (object->coreType())
			{
				case Backboard::typeKey:
					m_Backboard = object->as<Backboard>();
					break;
				case Artboard::typeKey:
					m_Artboards.push_back(object->as<Artboard>());
					break;
			}
		}
	}

	return importStack.resolve() == StatusCode::Ok ? ImportResult::success
	                                               : ImportResult::malformed;
}

Backboard* File::backboard() const { return m_Backboard; }

Artboard* File::artboard(std::string name) const
{
	for (auto artboard : m_Artboards)
	{
		if (artboard->name() == name)
		{
			return artboard;
		}
	}
	return nullptr;
}

Artboard* File::artboard() const
{
	if (m_Artboards.empty())
	{
		return nullptr;
	}
	return m_Artboards[0];
}
