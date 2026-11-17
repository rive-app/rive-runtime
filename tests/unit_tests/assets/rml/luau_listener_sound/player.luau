-- Plays the sound from init and again from its first advance, and keeps
-- neither.
type Player = {
    source: AudioSource?,
    started: boolean,
}

function init(self: Player, context: Context): boolean
    local source = context:audio('what')
    self.source = source
    if source then
        Audio.play(source)
    end
    return true
end

function advance(self: Player, seconds: number): boolean
    local source = self.source
    if not self.started and source then
        self.started = true
        Audio.play(source)
    end
    return true
end

return function(): Node<Player>
    return {
        source = nil,
        started = false,
        init = init,
        advance = advance,
    }
end
