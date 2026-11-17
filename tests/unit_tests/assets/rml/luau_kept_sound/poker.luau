-- From its advance, runs op on the shelf's sound and clears it: 1 plays it,
-- 2 resumes it, 3 plays another sound and stops that one, 4 runs every other
-- operation. Then reports whether the sound says it has completed.
local shelf = require('shelf')

type Poker = {
    context: Context?,
    source: AudioSource?,
}

function init(self: Poker, context: Context): boolean
    self.context = context
    self.source = context:audio('what')
    return true
end

local function runOp(self: Poker, vm: ViewModel, sound: AudioSound, op: number)
    if op == 1 then
        sound:play()
    elseif op == 2 then
        sound:resume()
    elseif op == 3 then
        local source = self.source
        local other = if source then Audio.play(source) else nil
        if other then
            other:stop()
        end
    elseif op == 4 then
        sound:pause()
        sound:stop()
        sound:stop(100)
        local seeked = vm:getBoolean('seeked')
        if seeked then
            seeked.value = sound:seek(0) or sound:seekFrame(0)
        end
        local time = vm:getNumber('time')
        if time then
            time.value = sound:time() + sound:timeFrame()
        end
        sound.volume = 0.5
        local volume = vm:getNumber('volume')
        if volume then
            volume.value = sound.volume
        end
    end
end

function advance(self: Poker, seconds: number): boolean
    local context = self.context
    local vm = if context then context:viewModel() else nil
    local sound = shelf.sound
    local op = if vm then vm:getNumber('op') else nil
    if vm == nil or sound == nil or op == nil or op.value == 0 then
        return true
    end
    runOp(self, vm, sound, op.value)
    op.value = 0
    local completed = vm:getBoolean('completed')
    if completed then
        completed.value = sound:completed()
    end
    return true
end

return function(): Node<Poker>
    return {
        context = nil,
        source = nil,
        init = init,
        advance = advance,
    }
end
