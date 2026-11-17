# animascript_audio_owner

Source of `assets/animascript_audio_owner.riv`, which `wasm_script_audio_test.cpp` loads.
It is unsigned, so only tools builds run its script. Rebuild it from this
directory with a rive-cli built with its dev tools, then copy
`build/animascript_audio_owner.riv` into `assets/`:

    rive . --unsigned-riv
