#include "rive/scripted/decoded_file.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

using namespace rive;

rcp<File> rive::importDecodedFile(
    Span<const uint8_t> bytes,
    Factory* factory,
    ImportResult* result)
{
  auto file = File::import(
      bytes,
      factory,
      result,
      nullptr,
      nullptr,
      /*requireSignedScripts*/ true);
#if defined(__EMSCRIPTEN__) && defined(WITH_RIVE_SCRIPTING_WASM)
  // No host awaits a prepare step for a file a script decodes, so its
  // modules instantiate inline here.
  if (file != nullptr)
  {
    file->startScripts();
  }
#endif
  return file;
}

DecodedBindable rive::decodedBindable(
    const File& file,
    const char* name,
    size_t length)
{
  DecodedBindable bindable;
  bindable.artboard = name != nullptr ? file.bindableArtboardNamed(
                                            std::string(name, length))
                                      : file.bindableArtboardDefault();
  if (bindable.artboard != nullptr)
  {
    bindable.viewModel = file.createDefaultViewModelInstance(
        bindable.artboard->artboard());
  }
  return bindable;
}

rcp<BindableArtboard> rive::hostBindableArtboard(
    const File& file,
    const char* name,
    size_t length)
{
  Artboard* artboard = name != nullptr
                           ? file.artboard(std::string(name, length))
                           : file.artboard();
  return artboard != nullptr ? make_rcp<BindableArtboard>(artboard) : nullptr;
}

static bool ownsViewModel(const File& file, const ViewModel* viewModel)
{
  for (size_t i = 0; i < file.viewModelCount(); i++)
  {
    if (file.viewModel(i) == viewModel)
    {
      return true;
    }
  }
  return false;
}

// Binds as a host binds the artboard it plays, where binding the machine
// completes the main and global instances (completeViewModelInstances), but
// embedded, so without a top level instance's watermark.
static DecodedRoot bindRoot(
    const File& file,
    Artboard* source,
    rcp<ViewModelInstance> main)
{
  DecodedRoot root;
  auto artboard = file.embeddedArtboard(source);
  if (artboard == nullptr)
  {
    return root;
  }
  // Drawn at the script's origin, as an Artboard input is.
  artboard->frameOrigin(false);
  if (main != nullptr && !ownsViewModel(file, main->viewModel()))
  {
    // Another file's instance would bind by this file's property ids.
    main = nullptr;
  }
  root.viewModel = main != nullptr
                       ? std::move(main)
                       : file.createDefaultViewModelInstance(artboard.get());
  root.stateMachine = artboard->defaultStateMachine();
  if (root.stateMachine != nullptr)
  {
    if (root.viewModel != nullptr)
    {
      root.stateMachine->setViewModelInstance(root.viewModel);
    }
    root.stateMachine->bind();
  }
  else
  {
    if (root.viewModel != nullptr)
    {
      artboard->setViewModelInstance(root.viewModel);
    }
    for (auto* viewModel : file.globalViewModels())
    {
      artboard->setGlobalViewModelInstance(
          viewModel->name(),
          file.createDefaultViewModelInstance(viewModel));
    }
    if (artboard->dataContext() == nullptr)
    {
      // Nothing to bind, but a context all the same, as binding a machine
      // makes one: a script asking for data in init waits until one exists.
      auto empty = make_rcp<DataContext>(rcp<ViewModelInstance>(nullptr));
      empty->addDependentContainer(artboard.get());
      artboard->internalDataContext(empty);
    }
    else
    {
      artboard->bind();
    }
  }
  root.artboard = std::move(artboard);
  return root;
}

DecodedRoot rive::decodedRoot(const File& file, const char* name, size_t length)
{
  return bindRoot(
      file,
      name != nullptr ? file.artboard(std::string(name, length))
                      : file.artboard(),
      nullptr);
}

DecodedRoot rive::decodedRootLike(
    const File& file,
    const ArtboardInstance& like,
    rcp<ViewModelInstance> main)
{
  const Artboard* source = like.artboardSource();
  for (size_t i = 0; i < file.artboardCount(); i++)
  {
    // Without decoding the artboards before it.
    if (file.artboardHeader(i)->handsOut(source))
    {
      return bindRoot(file, file.artboard(i), std::move(main));
    }
  }
  return DecodedRoot();
}
