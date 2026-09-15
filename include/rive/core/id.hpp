#ifndef _RIVE_ID_HPP_
#define _RIVE_ID_HPP_

#include <cstdint>

namespace rive
{
// `Id` is the type used by `Core` / `CoreContext` for object references
// (parentId, targetId, styleId, etc.). Its shape depends on how the
// runtime is being compiled:
//
//   - Without `WITH_RIVE_EDITOR`: `Id` is a flat `uint32_t` — the index
//     of the referenced object in the containing artboard's
//     `m_Objects` array (runtime `.riv` file layout).
//
//   - With `WITH_RIVE_EDITOR` (editor build): `Id` is a `{client,
//     object}` struct that represents BOTH kinds of references:
//       a) `.rev` (coop) references: `(client, object)` pair — the
//          stable per-peer identity the editor uses.
//       b) `.riv` (runtime) references loaded at edit time:
//          `(0, runtimeIndex)` — synthesized by
//          `CoreIdType::runtimeDeserialize` so edit-time tooling can
//          inspect a runtime-loaded object graph through the same
//          resolver.
//     The struct is implicitly constructible from `uint32_t` with
//     `client == 0` so call sites that still hand back runtime-style
//     `uint32_t` (generated `xxxId()` getters pre-regeneration) keep
//     working without each being rewritten by hand — the resulting
//     `Id{0, value}` resolves identically via `Artboard::resolve`. The
//     one exception is the unset sentinel, which promotes to
//     `kEmptyId` so `== -1` keeps meaning "not set".
#ifdef WITH_RIVE_EDITOR
struct Id
{
    // The "unset" / "invalid" sentinel is `{UINT32_MAX, UINT32_MAX}` —
    // not `{0, 0}`. Runtime `.riv` files use index `0` as a legitimate
    // first-object reference (parentId=0 means "child of the artboard
    // at m_Objects[0]"), and we want `Artboard::resolve(Id{0, 0})` to
    // keep returning that object. The JSON defs use
    // `initialValueRuntime: "-1"` for Id fields that really mean
    // "unset"; `IdFieldType::convertCpp` rewrites both `-1` and `0`-
    // with-`type:Id`+initial-matches-missing into `kEmptyId`.
    static constexpr uint32_t kUnsetField = 0xFFFFFFFFu;

    uint32_t client = kUnsetField;
    uint32_t object = kUnsetField;

    constexpr Id() = default;
    constexpr Id(uint32_t c, uint32_t o) : client(c), object(o) {}
    // Implicit conversion from runtime-index uint32_t. Still callable
    // from code that treats `Id` as an integer (e.g. `id == 0`).
    // `0xFFFFFFFF` is the defs' unset sentinel, so it has to land on
    // `kEmptyId` rather than a client-0 index that far out.
    constexpr Id(uint32_t runtimeIndex) :
        client(runtimeIndex == kUnsetField ? kUnsetField : 0),
        object(runtimeIndex)
    {}

    friend constexpr bool operator==(const Id& a, const Id& b)
    {
        return a.client == b.client && a.object == b.object;
    }
    friend constexpr bool operator!=(const Id& a, const Id& b)
    {
        return !(a == b);
    }

    constexpr bool empty() const
    {
        return client == kUnsetField && object == kUnsetField;
    }

    // Direct comparisons against integer literals — specifically
    // disambiguate `id == 0 / id != 0` against the combined presence
    // of the `Id(uint32_t)` promoting constructor and the implicit
    // `operator uint32_t()` narrowing conversion (either path is a
    // "valid" way to unify the operand types, and the compiler refuses
    // to pick).
    //
    // Semantics (match pre-refactor runtime behavior):
    //   - `-1` (== UINT32_MAX as unsigned) is the "unset" sentinel
    //     per the JSON defs' `initialValueRuntime: "-1"` convention,
    //     so it compares equal to `kEmptyId` regardless of `client`.
    //     This preserves the runtime idiom `if (id != -1)` which is
    //     sprinkled through `animation/*.cpp` to check "is this Id
    //     field set at all?".
    //   - Any other integer matches an Id at `client == 0` with that
    //     `object` index — matching how `Id(uint32_t)` projects
    //     runtime indices into the struct.
    friend constexpr bool operator==(const Id& a, int b)
    {
        if (b == -1)
            return a.empty();
        return a.client == 0 && a.object == static_cast<uint32_t>(b);
    }
    friend constexpr bool operator==(int a, const Id& b) { return b == a; }
    friend constexpr bool operator!=(const Id& a, int b) { return !(a == b); }
    friend constexpr bool operator!=(int a, const Id& b) { return !(a == b); }
    friend constexpr bool operator==(const Id& a, uint32_t b)
    {
        if (b == 0xFFFFFFFFu)
            return a.empty();
        return a.client == 0 && a.object == b;
    }
    friend constexpr bool operator==(uint32_t a, const Id& b) { return b == a; }
    friend constexpr bool operator!=(const Id& a, uint32_t b)
    {
        return !(a == b);
    }
    friend constexpr bool operator!=(uint32_t a, const Id& b)
    {
        return !(a == b);
    }

    // Implicit narrowing to `uint32_t` so runtime-shaped code that
    // returns Ids from `uint32_t`-typed APIs (`animation/*.hpp`
    // getters, `rive_binding.cpp` callback scaffolding, `size_t`
    // bounds checks on vertex / object arrays, etc.) keeps compiling
    // against the editor-mode struct. This drops the `client` field —
    // which is meaningful only for coop (non-runtime) Ids. For every
    // synthesized-from-runtime Id (`client == 0`) the object-field
    // IS the runtime index, so the conversion is lossless; for true
    // CoopIds it is lossy, and any path that cares about the client
    // must take `const Id&` (or access `.client` / `.object`
    // explicitly) rather than accept a `uint32_t` here. The pattern
    // matches the Dart editor, where runtime-typed APIs use the
    // `Id.object` field and editor code branches on `client`.
    constexpr operator uint32_t() const { return object; }
};
constexpr Id kEmptyId = {};
#else
using Id = uint32_t;
// Matches the `initialValueRuntime: "-1"` convention used across all
// Id-typed fields in the JSON defs — UINT32_MAX as unsigned.
constexpr Id kEmptyId = 0xFFFFFFFFu;
#endif
} // namespace rive

#endif
