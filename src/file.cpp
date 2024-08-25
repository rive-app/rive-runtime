#include "rive/file.hpp"
#include "rive/runtime_header.hpp"
#include "rive/animation/animation.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/bindable_property_importer.hpp"
#include "rive/importers/data_converter_group_importer.hpp"
#include "rive/importers/enum_importer.hpp"
#include "rive/importers/file_asset_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/keyed_object_importer.hpp"
#include "rive/importers/keyed_property_importer.hpp"
#include "rive/importers/linear_animation_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/importers/state_machine_listener_importer.hpp"
#include "rive/importers/state_machine_layer_importer.hpp"
#include "rive/importers/layer_state_importer.hpp"
#include "rive/importers/state_transition_importer.hpp"
#include "rive/importers/state_machine_layer_component_importer.hpp"
#include "rive/importers/transition_viewmodel_condition_importer.hpp"
#include "rive/importers/viewmodel_importer.hpp"
#include "rive/importers/viewmodel_instance_importer.hpp"
#include "rive/importers/viewmodel_instance_list_importer.hpp"
#include "rive/animation/blend_state_transition.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_state_direct.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property_string.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/viewmodel/viewmodel_property_list.hpp"

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
static Core* readRuntimeObject(BinaryReader& reader, const RuntimeHeader& header)
{
    auto coreObjectKey = reader.readVarUintAs<int>();
    auto object = CoreRegistry::makeCoreInstance(coreObjectKey);
    while (true)
    {
        auto propertyKey = reader.readVarUintAs<uint16_t>();
        if (propertyKey == 0)
        {
            // Terminator. https://media.giphy.com/media/7TtvTUMm9mp20/giphy.gif
            break;
        }

        if (reader.hasError())
        {
            delete object;
            return nullptr;
        }
        if (object == nullptr || !object->deserialize(propertyKey, reader))
        {
            // We have an unknown object or property, first see if core knows
            // the property type.
            int id = CoreRegistry::propertyFieldId(propertyKey);
            if (id == -1)
            {
                // No, check if it's in toc.
                id = header.propertyFieldId(propertyKey);
            }

            if (id == -1)
            {
                // Still couldn't find it, give up.
                fprintf(stderr,
                        "Unknown property key %d, missing from property ToC.\n",
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

File::File(Factory* factory, FileAssetLoader* assetLoader) :
    m_factory(factory), m_assetLoader(assetLoader)
{
    assert(factory);
}

File::~File()
{
    for (auto artboard : m_artboards)
    {
        delete artboard;
    }
    // Assets delete after artboards as they reference them.
    for (auto asset : m_fileAssets)
    {
        delete asset;
    }
    delete m_backboard;
}

std::unique_ptr<File> File::import(Span<const uint8_t> bytes,
                                   Factory* factory,
                                   ImportResult* result,
                                   FileAssetLoader* assetLoader)
{
    BinaryReader reader(bytes);
    RuntimeHeader header;
    if (!RuntimeHeader::read(reader, header))
    {
        fprintf(stderr, "Bad header\n");
        if (result)
        {
            *result = ImportResult::malformed;
        }
        return nullptr;
    }
    if (header.majorVersion() != majorVersion)
    {
        fprintf(stderr,
                "Unsupported version %u.%u expected %u.%u.\n",
                header.majorVersion(),
                header.minorVersion(),
                majorVersion,
                minorVersion);
        if (result)
        {
            *result = ImportResult::unsupportedVersion;
        }
        return nullptr;
    }
    auto file = rivestd::make_unique<File>(factory, assetLoader);

    auto readResult = file->read(reader, header);
    if (result)
    {
        *result = readResult;
    }
    if (readResult != ImportResult::success)
    {
        file.reset(nullptr);
    }
    return file;
}

ImportResult File::read(BinaryReader& reader, const RuntimeHeader& header)
{
    ImportStack importStack;
    // TODO: @hernan consider moving this to a special importer. It's not that
    // simple because Core doesn't have a typeKey, so it should be treated as
    // a special case. In any case, it's not that bad having it here for now.
    Core* lastBindableObject;
    while (!reader.reachedEnd())
    {
        auto object = readRuntimeObject(reader, header);
        if (object == nullptr)
        {
            importStack.readNullObject();
            continue;
        }
        if (!object->is<DataBind>())
        {
            lastBindableObject = object;
        }
        else if (lastBindableObject != nullptr)
        {
            object->as<DataBind>()->target(lastBindableObject);
        }
        if (object->import(importStack) == StatusCode::Ok)
        {
            switch (object->coreType())
            {
                case Backboard::typeKey:
                    m_backboard = object->as<Backboard>();
                    break;
                case Artboard::typeKey:
                {
                    Artboard* ab = object->as<Artboard>();
                    ab->m_Factory = m_factory;
                    m_artboards.push_back(ab);
                }
                break;
                case ImageAsset::typeKey:
                case FontAsset::typeKey:
                case AudioAsset::typeKey:
                {
                    auto fa = object->as<FileAsset>();
                    m_fileAssets.push_back(fa);
                }
                break;
                case ViewModel::typeKey:
                {
                    auto vmc = object->as<ViewModel>();
                    m_ViewModels.push_back(vmc);
                    break;
                }
                case DataEnum::typeKey:
                {
                    auto de = object->as<DataEnum>();
                    m_Enums.push_back(de);
                    break;
                }
                case ViewModelPropertyEnum::typeKey:
                {
                    auto vme = object->as<ViewModelPropertyEnum>();
                    vme->dataEnum(m_Enums[vme->enumId()]);
                }
                break;
            }
        }
        else
        {
            fprintf(stderr, "Failed to import object of type %d\n", object->coreType());
            delete object;
            continue;
        }
        std::unique_ptr<ImportStackObject> stackObject = nullptr;
        auto stackType = object->coreType();

        switch (stackType)
        {
            case Backboard::typeKey:
                stackObject = rivestd::make_unique<BackboardImporter>(object->as<Backboard>());
                break;
            case Artboard::typeKey:
                stackObject = rivestd::make_unique<ArtboardImporter>(object->as<Artboard>());
                break;
            case DataEnum::typeKey:
                stackObject = rivestd::make_unique<EnumImporter>(object->as<DataEnum>());
                break;
            case LinearAnimation::typeKey:
                stackObject =
                    rivestd::make_unique<LinearAnimationImporter>(object->as<LinearAnimation>());
                break;
            case KeyedObject::typeKey:
                stackObject = rivestd::make_unique<KeyedObjectImporter>(object->as<KeyedObject>());
                break;
            case KeyedProperty::typeKey:
            {
                auto importer =
                    importStack.latest<LinearAnimationImporter>(LinearAnimation::typeKey);
                if (importer == nullptr)
                {
                    return ImportResult::malformed;
                }
                stackObject =
                    rivestd::make_unique<KeyedPropertyImporter>(importer->animation(),
                                                                object->as<KeyedProperty>());
                break;
            }
            case StateMachine::typeKey:
                stackObject =
                    rivestd::make_unique<StateMachineImporter>(object->as<StateMachine>());
                break;
            case StateMachineLayer::typeKey:
            {
                auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
                if (artboardImporter == nullptr)
                {
                    return ImportResult::malformed;
                }

                stackObject =
                    rivestd::make_unique<StateMachineLayerImporter>(object->as<StateMachineLayer>(),
                                                                    artboardImporter->artboard());

                break;
            }
            case EntryState::typeKey:
            case ExitState::typeKey:
            case AnyState::typeKey:
            case AnimationState::typeKey:
            case BlendState1D::typeKey:
            case BlendStateDirect::typeKey:
                stackObject = rivestd::make_unique<LayerStateImporter>(object->as<LayerState>());
                stackType = LayerState::typeKey;
                break;
            case StateTransition::typeKey:
            case BlendStateTransition::typeKey:
                stackObject =
                    rivestd::make_unique<StateTransitionImporter>(object->as<StateTransition>());
                stackType = StateTransition::typeKey;
                break;
            case StateMachineListener::typeKey:
                stackObject = rivestd::make_unique<StateMachineListenerImporter>(
                    object->as<StateMachineListener>());
                break;
            case ImageAsset::typeKey:
            case FontAsset::typeKey:
            case AudioAsset::typeKey:
                stackObject = rivestd::make_unique<FileAssetImporter>(object->as<FileAsset>(),
                                                                      m_assetLoader,
                                                                      m_factory);
                stackType = FileAsset::typeKey;
                break;
            case ViewModel::typeKey:
                stackObject = rivestd::make_unique<ViewModelImporter>(object->as<ViewModel>());
                stackType = ViewModel::typeKey;
                break;
            case ViewModelInstance::typeKey:
                stackObject = rivestd::make_unique<ViewModelInstanceImporter>(
                    object->as<ViewModelInstance>());
                stackType = ViewModelInstance::typeKey;
                break;
            case ViewModelInstanceList::typeKey:
                stackObject = rivestd::make_unique<ViewModelInstanceListImporter>(
                    object->as<ViewModelInstanceList>());
                stackType = ViewModelInstanceList::typeKey;
                break;
            case TransitionViewModelCondition::typeKey:
            case TransitionArtboardCondition::typeKey:
                stackObject = rivestd::make_unique<TransitionViewModelConditionImporter>(
                    object->as<TransitionViewModelCondition>());
                stackType = TransitionViewModelCondition::typeKey;
                break;
            case BindablePropertyNumber::typeKey:
            case BindablePropertyString::typeKey:
            case BindablePropertyColor::typeKey:
            case BindablePropertyEnum::typeKey:
            case BindablePropertyBoolean::typeKey:
                stackObject =
                    rivestd::make_unique<BindablePropertyImporter>(object->as<BindableProperty>());
                stackType = BindablePropertyBase::typeKey;
                break;
            case DataConverterGroupBase::typeKey:
                stackObject = rivestd::make_unique<DataConverterGroupImporter>(
                    object->as<DataConverterGroup>());
                stackType = DataConverterGroupBase::typeKey;
                break;
        }
        if (importStack.makeLatest(stackType, std::move(stackObject)) != StatusCode::Ok)
        {
            // Some previous stack item didn't resolve.
            return ImportResult::malformed;
        }
        if (object->is<StateMachineLayerComponent>() &&
            importStack.makeLatest(StateMachineLayerComponent::typeKey,
                                   rivestd::make_unique<StateMachineLayerComponentImporter>(
                                       object->as<StateMachineLayerComponent>())) != StatusCode::Ok)
        {
            return ImportResult::malformed;
        }
    }

    return !reader.hasError() && importStack.resolve() == StatusCode::Ok ? ImportResult::success
                                                                         : ImportResult::malformed;
}

Artboard* File::artboard(std::string name) const
{
    for (const auto& artboard : m_artboards)
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
    if (m_artboards.empty())
    {
        return nullptr;
    }
    return m_artboards[0];
}

Artboard* File::artboard(size_t index) const
{
    if (index >= m_artboards.size())
    {
        return nullptr;
    }
    return m_artboards[index];
}

std::string File::artboardNameAt(size_t index) const
{
    auto ab = this->artboard(index);
    return ab ? ab->name() : "";
}

std::unique_ptr<ArtboardInstance> File::artboardDefault() const
{
    auto ab = this->artboard();
    return ab ? ab->instance() : nullptr;
}

std::unique_ptr<ArtboardInstance> File::artboardAt(size_t index) const
{
    auto ab = this->artboard(index);
    return ab ? ab->instance() : nullptr;
}

std::unique_ptr<ArtboardInstance> File::artboardNamed(std::string name) const
{
    auto ab = this->artboard(name);
    return ab ? ab->instance() : nullptr;
}

void File::completeViewModelInstance(ViewModelInstance* viewModelInstance)
{
    auto viewModel = m_ViewModels[viewModelInstance->viewModelId()];
    auto propertyValues = viewModelInstance->propertyValues();
    for (auto value : propertyValues)
    {
        if (value->is<ViewModelInstanceViewModel>())
        {
            auto property = viewModel->property(value->viewModelPropertyId());
            if (property->is<ViewModelPropertyViewModel>())
            {
                auto valueViewModel = value->as<ViewModelInstanceViewModel>();
                auto propertViewModel = property->as<ViewModelPropertyViewModel>();
                auto viewModelReference = m_ViewModels[propertViewModel->viewModelReferenceId()];
                auto viewModelInstance =
                    viewModelReference->instance(valueViewModel->propertyValue());
                if (viewModelInstance != nullptr)
                {
                    valueViewModel->referenceViewModelInstance(
                        copyViewModelInstance(viewModelInstance));
                }
            }
        }
        else if (value->is<ViewModelInstanceList>())
        {
            auto viewModelList = value->as<ViewModelInstanceList>();
            for (auto listItem : viewModelList->listItems())
            {
                auto viewModel = m_ViewModels[listItem->viewModelId()];
                auto viewModelInstance = viewModel->instance(listItem->viewModelInstanceId());
                listItem->viewModelInstance(copyViewModelInstance(viewModelInstance));
                if (listItem->artboardId() < m_artboards.size())
                {
                    listItem->artboard(m_artboards[listItem->artboardId()]);
                }
            }
        }
        value->viewModelProperty(viewModel->property(value->viewModelPropertyId()));
    }
}

ViewModelInstance* File::copyViewModelInstance(ViewModelInstance* viewModelInstance)
{
    auto copy = viewModelInstance->clone()->as<ViewModelInstance>();
    completeViewModelInstance(copy);
    return copy;
}

ViewModelInstance* File::createViewModelInstance(std::string name)
{
    for (auto viewModel : m_ViewModels)
    {
        if (viewModel->is<ViewModel>())
        {
            if (viewModel->name() == name)
            {
                return createViewModelInstance(viewModel);
            }
        }
    }
    return nullptr;
}

ViewModelInstance* File::createViewModelInstance(std::string name, std::string instanceName)
{
    for (auto viewModel : m_ViewModels)
    {
        if (viewModel->is<ViewModel>())
        {
            if (viewModel->name() == name)
            {
                auto instance = viewModel->instance(instanceName);
                if (instance != nullptr)
                {
                    return copyViewModelInstance(instance);
                }
            }
        }
    }
    return nullptr;
}

ViewModelInstance* File::createViewModelInstance(size_t index, size_t instanceIndex)
{
    if (index < m_ViewModels.size())
    {
        auto viewModel = m_ViewModels[index];
        auto instance = viewModel->instance(instanceIndex);
        if (instance != nullptr)
        {
            return copyViewModelInstance(instance);
        }
    }
    return nullptr;
}

ViewModelInstance* File::createViewModelInstance(ViewModel* viewModel)
{
    if (viewModel != nullptr)
    {
        auto viewModelInstance = viewModel->defaultInstance();
        return copyViewModelInstance(viewModelInstance);
    }
    return nullptr;
}

ViewModelInstance* File::createViewModelInstance(Artboard* artboard)
{
    if ((size_t)artboard->viewModelId() < m_ViewModels.size())
    {
        auto viewModel = m_ViewModels[artboard->viewModelId()];
        if (viewModel != nullptr)
        {
            return createViewModelInstance(viewModel);
        }
    }
    return nullptr;
}

ViewModelInstanceListItem* File::viewModelInstanceListItem(ViewModelInstance* viewModelInstance)
{
    // Search for an implicit artboard linked to the viewModel.
    // It will return the first one it finds, but there could be more.
    // We should decide if we want to be more restrictive and only return
    // an artboard if one and only one is found.
    for (auto artboard : m_artboards)
    {
        if (artboard->viewModelId() == viewModelInstance->viewModelId())
        {
            return viewModelInstanceListItem(viewModelInstance, artboard);
        }
    }
    return nullptr;
}

ViewModelInstanceListItem* File::viewModelInstanceListItem(ViewModelInstance* viewModelInstance,
                                                           Artboard* artboard)
{
    auto viewModelInstanceListItem = new ViewModelInstanceListItem();
    viewModelInstanceListItem->viewModelInstance(viewModelInstance);
    viewModelInstanceListItem->artboard(artboard);
    return viewModelInstanceListItem;
}

ViewModel* File::viewModel(std::string name)
{
    for (auto viewModel : m_ViewModels)
    {
        if (viewModel->name() == name)
        {
            return viewModel;
        }
    }
    return nullptr;
}

const std::vector<FileAsset*>& File::assets() const { return m_fileAssets; }

#ifdef WITH_RIVE_TOOLS
const std::vector<uint8_t> File::stripAssets(Span<const uint8_t> bytes,
                                             std::set<uint16_t> typeKeys,
                                             ImportResult* result)
{
    std::vector<uint8_t> strippedData;
    strippedData.reserve(bytes.size());
    BinaryReader reader(bytes);
    RuntimeHeader header;
    if (!RuntimeHeader::read(reader, header))
    {
        if (result)
        {
            *result = ImportResult::malformed;
        }
    }
    else if (header.majorVersion() != majorVersion)
    {
        if (result)
        {
            *result = ImportResult::unsupportedVersion;
        }
    }
    else
    {
        strippedData.insert(strippedData.end(), bytes.data(), reader.position());
        const uint8_t* from = reader.position();
        const uint8_t* to = reader.position();
        uint16_t lastAssetType = 0;
        while (!reader.reachedEnd())
        {
            auto object = readRuntimeObject(reader, header);
            if (object == nullptr)
            {
                continue;
            }
            if (object->is<FileAssetBase>())
            {
                lastAssetType = object->coreType();
            }
            if (object->is<FileAssetContents>() && typeKeys.find(lastAssetType) != typeKeys.end())
            {
                if (from != to)
                {
                    strippedData.insert(strippedData.end(), from, to);
                }
                from = reader.position();
            }
            delete object;
            to = reader.position();
        }
        if (from != to)
        {
            strippedData.insert(strippedData.end(), from, to);
        }
        *result = ImportResult::success;
    }
    return strippedData;
}
#endif
