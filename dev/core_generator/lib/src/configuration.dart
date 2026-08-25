// Canonical defs live at the repo root in `dev/defs/`. The
// previously-checked-in `packages/runtime/dev/defs/` was a sparse
// checkout of the upstream rive-app/rive runtime defs (147 types) and
// drifted from the editor-side superset: missing ~47 editor-only types
// (component_asset, library_*, code/*, viewmodel/library/*, …) plus
// editor-only property additions on shared types (tagIds, exportName,
// flagValues, …). Repointing at the repo-root tree gives the generator
// the full picture in one shot. The sparse checkout and the
// `update_defs.sh` that fed it are gone; the canonical defs are
// versioned alongside the editor source.
String defsPath = '../../../dev/defs/';

// Runtime output — runtime-visible types only (`"runtime": true`).
// Safe to ship in the runtime-only build; the shipped `.a` / `.so` /
// WASM blobs never carry editor-only class machinery.
String generatedHppPath = '../include/rive/generated/';
String concreteHppPath = '../include/';
String generatedCppPath = '../src/generated/';

// Editor-only output — types declared `"runtime": false`. Emitted
// under `editor_native/` so the runtime package stays free of
// editor-only artifacts entirely. CoreRegistry still lives in
// runtime but wraps its includes of these headers in
// `#ifdef WITH_RIVE_EDITOR`, and editor_native's build exposes
// `packages/editor_native/kernel/include` as a header-search root so
// the conditional includes resolve.
//
// Header include-path prefix for editor-only classes: the generator
// writes e.g. `assets/layered_asset_base.hpp` with an include like
// `#include "editor_native/generated/assets/layered_asset_base.hpp"`.
String editorGeneratedHppPath =
    '../../editor_native/kernel/include/editor_native/generated/';
String editorConcreteHppPath = '../../editor_native/kernel/include/';
String editorGeneratedCppPath = '../../editor_native/kernel/src/generated/';
