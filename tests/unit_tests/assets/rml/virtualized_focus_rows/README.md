# virtualized_focus_rows

`scene.rml` is the source markup for `assets/virtualized_focus_rows.riv`, which
`virtualized_focus_rows_test.cpp` loads. Its header comment says what the scene
is for.

Rebuild the `.riv` with a dev build of the CLI from this directory, then copy
`build/virtualized_focus_rows.riv` into `assets/`:

    rive . --unsigned-riv

That is how the checked-in file was made. A release CLI has no
`--unsigned-riv`; `rive . --publish` writes a signed file and needs
`rive login`.
