#ifndef _RIVE_EDITOR_HOOKS_HPP_
#define _RIVE_EDITOR_HOOKS_HPP_

// Generated setters and the shared copy/deserialize bodies call these
// unconditionally so `rive/generated` carries no editor branches. Only the
// editor build gives them a body; everywhere else they vanish.
//
// The editor-only members, accessors and dispatch they reach for are declared
// by the per-type `_ext.inl` the generated class body includes under
// WITH_RIVE_EDITOR.
#ifdef WITH_RIVE_EDITOR

#define RIVE_EDITOR_CHANGING(propertyKey, oldValue, newValue)                  \
    onPropertyChanging(propertyKey, oldValue, newValue)
#define RIVE_EDITOR_STRING_CHANGING(propertyKey, oldValue, newValue)           \
    onStringChanging(propertyKey, oldValue, newValue)
#define RIVE_EDITOR_FRACTIONAL_INDEX_CHANGING(propertyKey, oldValue, newValue) \
    onFractionalIndexChanging(propertyKey, oldValue, newValue)

// Coop hydration mutates properties before onAddedDirty wires the object up,
// so the side effecting callback waits until the object validates.
#define RIVE_EDITOR_CHANGED(call)                                              \
    if (hasValidated())                                                        \
    {                                                                          \
        call;                                                                  \
    }                                                                          \
    (void)0

#define RIVE_EDITOR_COPY(object) copyEditorProperties(object)
#define RIVE_EDITOR_COPY_VALIDATED(object)                                     \
    if (object.hasValidated())                                                 \
    {                                                                          \
        markValidated();                                                       \
    }                                                                          \
    (void)0
#define RIVE_EDITOR_DESERIALIZE(propertyKey, reader)                           \
    if (deserializeEditorProperties(propertyKey, reader))                      \
    {                                                                          \
        return true;                                                           \
    }                                                                          \
    (void)0

#else

#define RIVE_EDITOR_CHANGING(propertyKey, oldValue, newValue) (void)0
#define RIVE_EDITOR_STRING_CHANGING(propertyKey, oldValue, newValue) (void)0
#define RIVE_EDITOR_FRACTIONAL_INDEX_CHANGING(propertyKey, oldValue, newValue) \
    (void)0
#define RIVE_EDITOR_CHANGED(call) call
#define RIVE_EDITOR_COPY(object) (void)0
#define RIVE_EDITOR_COPY_VALIDATED(object) (void)0
#define RIVE_EDITOR_DESERIALIZE(propertyKey, reader) (void)0

#endif

#endif
