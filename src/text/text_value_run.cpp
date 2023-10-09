#include "rive/core_context.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"

using namespace rive;

void TextValueRun::textChanged() { parent()->as<Text>()->markShapeDirty(); }

StatusCode TextValueRun::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (parent() != nullptr && parent()->is<Text>())
    {
        parent()->as<Text>()->addRun(this);
        return StatusCode::Ok;
    }

    return StatusCode::MissingObject;
}

StatusCode TextValueRun::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(styleId());
    if (coreObject == nullptr || !coreObject->is<TextStyle>())
    {
        return StatusCode::MissingObject;
    }

    m_style = static_cast<TextStyle*>(coreObject);

    return StatusCode::Ok;
}

StatusCode TextValueRun::import(ImportStack& importStack)
{
    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addTextValueRun(this);
    return Super::import(importStack);
}

void TextValueRun::styleIdChanged()
{
    auto coreObject = artboard()->resolve(styleId());
    if (coreObject != nullptr && coreObject->is<TextStyle>())
    {
        m_style = static_cast<TextStyle*>(coreObject);
        parent()->as<Text>()->markShapeDirty();
    }
}

uint32_t TextValueRun::offset() const
{
#ifdef WITH_RIVE_TEXT
    Text* text = parent()->as<Text>();
    uint32_t offset = 0;

    for (const TextValueRun* run : text->runs())
    {
        if (run == this)
        {
            break;
        }
        offset += (uint32_t)run->text().size();
    }
    return offset;
#else
    return 0;
#endif
}