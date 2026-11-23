# quiet_script_rows

Source of `assets/quiet_script_rows.riv`, which `quiet_rows_test.cpp` loads.
It is unsigned, so only tools builds run its script. Rebuild it from this
directory with a rive-cli built with its dev tools, then copy
`build/quiet_script_rows.riv` into `assets/`:

    rive . --unsigned-riv

Each row of Main's list hosts Probe, a script that keeps advancing a few
frames after anything wakes it and then sleeps, so its row can go quiet. It
reports into its item's view model what woke it:
- `advances` counts its advances;
- `doubled` is twice its `level` input, bound to the item's `level`;
- `presses` counts presses on the row;
- `seen` is the row's Box x as its last advance read it, and Box x is bound
  to the item's `box`;
- a change to `poke` runs its listener, which asks for an update;
- `busy` keeps it advancing.

Each row also has Line, whose stroke runs Shift, a path effect that sleeps
the same way. It moves the stroke's path right by its `offset` input, bound
to the item's `offset`, plus one unit per step, and down one unit per update.
A change to `offset` gives it two steps, one per advance, each asking for an
update; any other update (async work the host delivers) only moves it down.
