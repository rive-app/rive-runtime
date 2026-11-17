-- Listens to the view model's score from its first advance and plays the
-- sound when it changes.
type Listener = {
    context: Context?,
    source: AudioSource?,
    score: Property<number>?,
}

local function onScore(self: Listener)
    local source = self.source
    if source then
        Audio.play(source)
    end
end

function init(self: Listener, context: Context): boolean
    self.context = context
    self.source = context:audio('what')
    return true
end

function advance(self: Listener, seconds: number): boolean
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

return function(): Node<Listener>
    return {
        context = nil,
        source = nil,
        score = nil,
        init = init,
        advance = advance,
    }
end
