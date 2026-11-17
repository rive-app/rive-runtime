-- Listens to the view model's score from its first advance. Its listener
-- makes an instance of deck and advances it, so the instance's script runs
-- its hooks there, then plays the sound itself.
type Relay = {
    deck: Input<Artboard>,
    context: Context?,
    source: AudioSource?,
    score: Property<number>?,
    inner: Artboard?,
}

local function onScore(self: Relay)
    local inner = self.deck:instance()
    self.inner = inner
    inner:advance(1 / 60)
    local source = self.source
    if source then
        Audio.play(source)
    end
end

function init(self: Relay, context: Context): boolean
    self.context = context
    self.source = context:audio('what')
    return true
end

function advance(self: Relay, seconds: number): boolean
    local context = self.context
    if self.score ~= nil or context == nil then
        return true
    end
    local vm = context:viewModel()
    local score = if vm then vm:getNumber('score') else nil
    if score then
        self.score = score
        score:addListener(self, onScore)
    end
    return true
end

return function(): Node<Relay>
    return {
        deck = late(),
        context = nil,
        source = nil,
        score = nil,
        inner = nil,
        init = init,
        advance = advance,
    }
end
