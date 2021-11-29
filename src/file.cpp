#include "rive/file.hpp"
#include "rive/animation/animation.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/file_asset_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/keyed_object_importer.hpp"
#include "rive/importers/keyed_property_importer.hpp"
#include "rive/importers/linear_animation_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/importers/state_machine_layer_importer.hpp"
#include "rive/importers/layer_state_importer.hpp"
#include "rive/importers/state_transition_importer.hpp"
#include "rive/animation/blend_state_transition.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_state_direct.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

#if !defined(RIVE_FMT_U64)
#if defined(__ANDROID__)
#if INTPTR_MAX == INT64_MAX
#define RIVE_FMT_U64 "%lu"
#define RIVE_FMT_I64 "%ld"
#else
#define RIVE_FMT_U64 "%llu"
#define RIVE_FMT_I64 "%lld"
#endif
#elif defined(_WIN32)
#define RIVE_FMT_U64 "%lld"
#define RIVE_FMT_I64 "%llu"
#else
#include <inttypes.h>
#define RIVE_FMT_U64 "%" PRIu64
#define RIVE_FMT_I64 "%" PRId64
#endif
#endif

// Import a single Rive runtime object.
// Used by the file importer.
static Core* readRuntimeObject(BinaryReader& reader,
                               const RuntimeHeader& header)
{
	auto coreObjectKey = reader.readVarUint64();
	auto object = CoreRegistry::makeCoreInstance((int)coreObjectKey);
	while (true)
	{
		auto propertyKey = reader.readVarUint64();
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
				fprintf(stderr,
				        "Unknown property key " RIVE_FMT_U64
				        ", missing from property ToC.\n",
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
		//         "File contains an unknown object with coreType " RIVE_FMT_U64
		//         ", which " "this runtime doesn't understand.\n",
		//         coreObjectKey);
		return nullptr;
	}
	return object;
}

File::File(FileAssetResolver* assetResolver) : m_AssetResolver(assetResolver) {}

File::~File()
{
	for (auto artboard : m_Artboards)
	{
		delete artboard;
	}
	delete m_Backboard;
}

// Import a Rive file from a file handle
ImportResult File::import(BinaryReader& reader,
                          File** importedFile,
                          FileAssetResolver* assetResolver)
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
	auto file = new File(assetResolver);
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
		else
		{
			fprintf(stderr,
			        "Failed to import object of type %d\n",
			        object->coreType());
			delete object;
			continue;
		}
		ImportStackObject* stackObject = nullptr;
		auto stackType = object->coreType();

		switch (stackType)
		{
			case Backboard::typeKey:
				stackObject = new BackboardImporter(object->as<Backboard>());
				break;
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
				    object->as<StateMachineLayer>(),
				    artboardImporter->artboard());

				break;
			}
			case EntryState::typeKey:
			case ExitState::typeKey:
			case AnyState::typeKey:
			case AnimationState::typeKey:
			case BlendState1D::typeKey:
			case BlendStateDirect::typeKey:
				stackObject = new LayerStateImporter(object->as<LayerState>());
				stackType = LayerState::typeKey;
				break;
			case StateTransition::typeKey:
			case BlendStateTransition::typeKey:
				stackObject =
				    new StateTransitionImporter(object->as<StateTransition>());
				stackType = StateTransition::typeKey;
				break;
			case ImageAsset::typeKey:
				stackObject = new FileAssetImporter(object->as<FileAsset>(),
				                                    m_AssetResolver);
				stackType = FileAsset::typeKey;
				break;
		}
		if (importStack.makeLatest(stackType, stackObject) != StatusCode::Ok)
		{
			// Some previous stack item didn't resolve.
			return ImportResult::malformed;
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

Artboard* File::artboard(size_t index) const
{
	if (index >= m_Artboards.size())
	{
		return nullptr;
	}
	return m_Artboards[index];
}
