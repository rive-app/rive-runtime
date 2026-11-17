-- From its first advance, plays the sound from a promise's executor, from
-- a handler on that promise it then resolves itself, and from a handler on
-- a promise that has already resolved.
type Keeper = {
    source: AudioSource?,
    done: boolean,
}

function init(self: Keeper, context: Context): boolean
    self.source = context:audio('what')
    return true
end

function advance(self: Keeper, seconds: number): boolean
    local source = self.source
    if self.done or source == nil then
        return true
    end
    self.done = true
    local resolveLater: (number) -> () = function(_: number) end
    local later = Promise.new(function(resolve, _reject, _onCancel)
        Audio.play(source)
        resolveLater = resolve
    end)
    later:andThen(function(_: number)
        Audio.play(source)
    end)
    resolveLater(1)
    Promise.resolve(1):andThen(function(_: number)
        Audio.play(source)
    end)
    return true
end

return function(): Node<Keeper>
    return {
        source = nil,
        done = false,
        init = init,
        advance = advance,
    }
end
