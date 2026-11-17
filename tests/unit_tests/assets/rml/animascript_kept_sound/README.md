# animascript_kept_sound

Source of `assets/animascript_kept_sound.riv`, which `wasm_script_audio_test.cpp` loads.
It is unsigned, so only tools builds run its scripts. Rebuild it from this
directory with a rive-cli built with its dev tools, then copy
`build/animascript_kept_sound.riv` into `assets/`:

    rive . --unsigned-riv
