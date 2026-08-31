# Generated layout matrix — do not edit by hand

Every `.riv` and `.expect` in this folder is produced by
`packages/rive_core/test/matrix/emit_matrix_test.dart` from the specs in
`slices.dart`.

That test **verifies** these files by default and fails if they do not match the
specs. To regenerate after changing a spec:

    cd packages/rive_core
    RIVE_MATRIX_WRITE=1 flutter test test/matrix/emit_matrix_test.dart

Verifying is the default on purpose. A plain `flutter test` runs every file
under `test/`, so an emitter that always wrote would regenerate the corpus on
the Dart CI job while the C++ job kept testing whatever is committed. Edit a
slice, forget to regenerate, and the two halves silently test different things
with nothing red anywhere. Verifying turns that into a failure on the PR that
caused it.

Do not add hand-authored files here: the C++ runner
(`runtime/layout_matrix_test.cpp`) assumes every `.riv` has an `.expect`
sibling, and the verifier reports anything it did not generate as unexpected.

Expected values are DERIVED from the declared geometry in `slices.dart`, never
recorded from a run. If a fixture starts failing, the question is whether the
spec or the implementation is wrong — not which one to re-record.

## Both runtimes

The same specs and the same derivations are checked twice:

- **C++** — `runtime/layout_matrix_test.cpp` reads the `.riv` + `.expect` pairs
  here. This is the runtime that ships.
- **Dart** — `packages/rive_core/test/matrix/dart_conformance_test.dart` builds
  the same scenes live and checks the editor's own layout pipeline. It skips
  what rive_core cannot mount on its own (nested artboards, data-bound lists,
  riv version gates) and counts those out loud rather than passing silently.

A cell that only fails on one side is a runtime divergence, which is the whole
reason for running both.

## Markers in a `.expect`

| tag | meaning |
|---|---|
| `C` | the correct value; asserted directly |
| `L` | a value the runtime must NOT produce; the runner asserts the actual value DIFFERS. Only written for a field with no `C`, since a `C` already fails on every wrong value including this one |
| `X` | a known, reproduced defect. The value recorded is still the CORRECT one and the assertion is inverted: a mismatch warns, a MATCH fails and tells whoever fixed it to clear the `knownDefect` marker in the spec |
