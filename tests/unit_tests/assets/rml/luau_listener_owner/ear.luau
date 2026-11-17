-- Listens from its first advance. Its score listener allocates enough for
-- the collector to delete this instance, then plays the sound. Its relay
-- listener changes echo, whose listener in Host allocates, then plays.
type Ear = {
    context: Context?,
    source: AudioSource?,
    score: Property<number>?,
    relay: Property<number>?,
    echo: Property<number>?,
}

local function play(self: Ear)
    local source = self.source
    if source then
        Audio.play(source)
    end
end

local function onScore(self: Ear)
    local junk = {}
    for i = 1, 200000 do
        junk[i % 64] = { i }
    end
    play(self)
end

local function onRelay(self: Ear)
    local echo = self.echo
    if echo then
        echo.value += 1
    end
    play(self)
end

function init(self: Ear, context: Context): boolean
    self.context = context
    self.source = context:audio('what')
    return true
end

function advance(self: Ear, seconds: number): boolean
    local context = self.context
    if self.score ~= nil or context == nil then
        return true
    end
    local vm = context:viewModel()
    if vm == nil then
        return true
    end
    local score = vm:getNumber('score')
    local relay = vm:getNumber('relay')
    self.echo = vm:getNumber('echo')
    if score and relay then
        self.score = score
        self.relay = relay
        score:addListener(self, onScore)
        relay:addListener(self, onRelay)
    end
    return true
end

return function(): Node<Ear>
    return {
        context = nil,
        source = nil,
        score = nil,
        relay = nil,
        echo = nil,
        init = init,
        advance = advance,
    }
end
