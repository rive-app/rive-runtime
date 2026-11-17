-- A sound every script that requires this module shares: one script keeps
-- it here and another uses it.
type Shelf = {
    sound: AudioSound?,
}

local shelf: Shelf = {
    sound = nil,
}

return shelf
