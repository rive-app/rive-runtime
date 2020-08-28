#include "file.hpp"
#include "animation/animation.hpp"
#include "generated/core_registry.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

// Import a single Rive runtime object.
// Used by the file importer.
template <typename T = Core> static T* readRuntimeObject(BinaryReader& reader)
{
	auto coreObjectKey = reader.readVarUint();
	auto object = CoreRegistry::makeCoreInstance(coreObjectKey);

	if (object == nullptr)
	{
		fprintf(stderr, "Unknown object of type %llu.\n", coreObjectKey);
		return nullptr;
	}
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

		if (!object->deserialize(propertyKey, reader))
		{
			fprintf(stderr, "Unknown property of type %llu.\n", propertyKey);
			delete object;
			return nullptr;
		}
	}

	// This is evaluated at compile time based on how the templated method is
	// called. This means that it'll get optimized out when calling with type
	// Core (which is the default). The type checking is skipped in this case.
	if constexpr (!std::is_same<T, Core>::value)
	{
		// Concrete Core object couldn't be read.
		if (object == nullptr)
		{
			fprintf(stderr,
			        "Expected object of type %d but found %llu, which this "
			        "runtime doesn't understand.\n",
			        T::typeKey,
			        coreObjectKey);
			return nullptr;
		}
		// Ensure the object is of the provided type, if not, return null and
		// delete the object. Note that we read in the properties regardless of
		// whether or not this object is the expected one. This ensures our
		// reader has advanced past the object.
		if (!object->is<T>())
		{
			fprintf(stderr,
			        "Expected object of type %d but found %d.\n",
			        T::typeKey,
			        object->coreType());
			delete object;
			return nullptr;
		}
	}
	// Core object couldn't be read.
	else if (object == nullptr)
	{
		fprintf(stderr,
		        "Expected a Core object but found %llu, which this runtime "
		        "doesn't understand.\n",
		        coreObjectKey);
		return nullptr;
	}
	return reinterpret_cast<T*>(object);
}

File::~File() { delete m_Backboard; }

// Import a Rive file from a file handle
ImportResult File::import(BinaryReader& reader, File** importedFile)
{
	RuntimeHeader header;
	if (!RuntimeHeader::read(reader, header))
	{
		fprintf(stderr, "Bad header\n");
		return ImportResult::malformed;
	}
	if (header.majorVersion() != majorVersion ||
	    (header.majorVersion() == majorVersion &&
	     header.minorVersion() > minorVersion))
	{
		fprintf(stderr,
		        "Unsupported version %u expected %u.\n",
		        header.majorVersion(),
		        majorVersion);
		return ImportResult::unsupportedVersion;
	}
	auto file = new File();
	auto result = file->read(reader);
	if (result != ImportResult::success)
	{
		delete file;
		return result;
	}
	*importedFile = file;
	return result;
}

ImportResult File::read(BinaryReader& reader)
{
	m_Backboard = readRuntimeObject<Backboard>(reader);
	if (m_Backboard == nullptr)
	{
		fprintf(stderr, "Expected first object to be the backboard.\n");
		return ImportResult::malformed;
	}

	auto numArtboards = reader.readVarUint();
	for (int i = 0; i < numArtboards; i++)
	{
		auto numObjects = reader.readVarUint();
		if (numObjects == 0)
		{
			fprintf(
			    stderr,
			    "Artboards must contain at least one object (themselves).\n");
			return ImportResult::malformed;
		}
		auto artboard = readRuntimeObject<Artboard>(reader);
		if (artboard == nullptr)
		{
			fprintf(stderr, "Found non-artboard in artboard list.\n");
			return ImportResult::malformed;
		}
		m_Artboards.push_back(artboard);

		artboard->addObject(artboard);

		for (int i = 1; i < numObjects; i++)
		{
			auto object = readRuntimeObject(reader);
			// N.B. we add objects that don't load (null) too as we need to look
			// them up by index.
			artboard->addObject(object);
		}

		// Animations also need to reference objects, so make sure they get read
		// in before the hierarchy resolves (batch add completes).
		auto numAnimations = reader.readVarUint();
		for (int i = 0; i < numAnimations; i++)
		{
			auto animation = readRuntimeObject<Animation>(reader);
			if (animation == nullptr)
			{
				continue;
			}
			artboard->addAnimation(animation);
			if (animation->coreType() == LinearAnimationBase::typeKey)
			{
				auto linearAnimation =
				    reinterpret_cast<LinearAnimation*>(animation);
				int numKeyedObjects = reader.readVarUint();
				for (int j = 0; j < numKeyedObjects; j++)
				{
					auto keyedObject = readRuntimeObject<KeyedObject>(reader);
					if (keyedObject == nullptr)
					{
						continue;
					}
					linearAnimation->addKeyedObject(keyedObject);

					int numKeyedProperties = reader.readVarUint();
					for (int k = 0; k < numKeyedProperties; k++)
					{
						auto keyedProperty =
						    readRuntimeObject<KeyedProperty>(reader);
						if (keyedProperty == nullptr)
						{
							continue;
						}
						keyedObject->addKeyedProperty(keyedProperty);

						int numKeyframes = reader.readVarUint();
						for (int l = 0; l < numKeyframes; l++)
						{
							auto keyframe = readRuntimeObject<KeyFrame>(reader);
							if (keyframe == nullptr)
							{
								continue;
							}
							else if (keyframe->is<KeyFrameDrawOrder>())
							{
								auto drawOrderKeyFrame =
								    reinterpret_cast<KeyFrameDrawOrder*>(
								        keyframe);
								int numValues = reader.readVarUint();
								for (int m = 0; m < numValues; m++)
								{
									auto valueObject =
									    new KeyFrameDrawOrderValue();
									valueObject->drawableId(
									    reader.readVarUint());
									valueObject->value(m);
									drawOrderKeyFrame->addValue(valueObject);
								}
							}
							keyframe->computeSeconds(linearAnimation->fps());
							keyedProperty->addKeyFrame(keyframe);
						}
					}
				}
			}
		}

		// Artboard has been read in.
		artboard->initialize();
	}
	return ImportResult::success;
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