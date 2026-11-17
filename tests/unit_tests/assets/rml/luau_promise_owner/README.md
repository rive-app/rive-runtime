# luau_promise_owner

Source of `assets/luau_promise_owner.riv`, which `scripting_audio_test.cpp`
loads. It is unsigned, so only tools builds run its scripts. Rebuild it from
this directory with a rive-cli built with its dev tools, then copy
`build/luau_promise_owner.riv` into `assets/`:

    rive . --unsigned-riv
