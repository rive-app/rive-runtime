# nested_script_hit

Source of `assets/nested_script_hit.riv`, which
`scripting_nested_hit_test.cpp` loads. It is unsigned, so only tools builds
run its script. Rebuild it from this directory with a rive-cli built with its
dev tools, then copy `build/nested_script_hit.riv` into `assets/`:

    rive . --unsigned-riv

A released rive-cli that still has `--once` (1.9.0-uat.55) writes the same
unsigned file with `rive . --once`.
