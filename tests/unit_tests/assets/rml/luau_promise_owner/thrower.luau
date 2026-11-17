-- From its first advance, resolves boom, whose handler in Fumbler throws,
-- then plays the sound itself in the same hook.
local bus = require('bus')

type Thrower = {
    source: AudioSource?,
    done: boolean,
}

function init(self: Thrower, context: Context): boolean
    self.source = context:audio('what')
    return true
end

function advance(self: Thrower, seconds: number): boolean
    if not self.done then
        self.done = true
        bus.fumble()
        local source = self.source
        if source then
            Audio.play(source)
        end
    end
    return true
end

return function(): Node<Thrower>
    return {
        source = nil,
        done = false,
        init = init,
        advance = advance,
    }
end
