-- From init, waits on boom with a handler that plays the sound and throws.
local bus = require('bus')

type Fumbler = {
    source: AudioSource?,
}

function init(self: Fumbler, context: Context): boolean
    local source = context:audio('what')
    self.source = source
    bus.boom:andThen(function(_: number)
        if source then
            Audio.play(source)
        end
        error('boom')
    end)
    return true
end

return function(): Node<Fumbler>
    return {
        source = nil,
        init = init,
    }
end
