-- From init, waits on spawn. Its handler makes an instance of deck and
-- advances it, so the instance's script runs its hooks there, then plays
-- the sound itself.
local bus = require('bus')

type Hatcher = {
    deck: Input<Artboard>,
    source: AudioSource?,
    inner: Artboard?,
}

function init(self: Hatcher, context: Context): boolean
    self.source = context:audio('what')
    bus.spawn:andThen(function(_: number)
        local inner = self.deck:instance()
        self.inner = inner
        inner:advance(1 / 60)
        local source = self.source
        if source then
            Audio.play(source)
        end
    end)
    return true
end

return function(): Node<Hatcher>
    return {
        deck = late(),
        source = nil,
        inner = nil,
        init = init,
    }
end
