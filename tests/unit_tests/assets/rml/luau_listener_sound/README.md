# luau_listener_sound

Source of `assets/luau_listener_sound.riv`, which `audio_test.cpp` and
`scripting_audio_test.cpp` load. It is unsigned, so only tools builds run its
scripts. Rebuild it from this directory with a rive-cli built with its dev
tools, then copy `build/luau_listener_sound.riv` into `assets/`:

    rive . --unsigned-riv
