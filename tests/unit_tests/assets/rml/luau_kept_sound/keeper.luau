-- Plays the sound from init and keeps it on the shelf.
local shelf = require('shelf')

type Keeper = {}

function init(self: Keeper, context: Context): boolean
    local source = context:audio('what')
    if source then
        shelf.sound = Audio.play(source)
    end
    return true
end

return function(): Node<Keeper>
    return {
        init = init,
    }
end
