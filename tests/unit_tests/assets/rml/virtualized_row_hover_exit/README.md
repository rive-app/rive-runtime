# virtualized_row_hover_exit

`scene.rml` is the source markup for `assets/virtualized_row_hover_exit.riv`,
which `virtualized_row_hover_exit_test.cpp` loads. Its header comment says what
the scene is for.

Rebuild the `.riv` with the CLI from this directory, then copy
`build/virtualized_row_hover_exit.riv` into `assets/`:

    rive . --publish

`--publish` needs `rive login`. Without a login, `packages/rml_cli` builds the
file instead, and that is how the checked-in file was made. `--via-markup`
keeps the names the tests use to find objects:

    rive_rml_generator -t rev -s scene.rml -d scene.rev
    rive_rml_generator -t riv --via-markup -s scene.rev -d virtualized_row_hover_exit.riv
