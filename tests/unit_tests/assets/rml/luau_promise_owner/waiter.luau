-- From init, waits on the bus's promises with every kind of handler, each
-- of which plays the sound: andThen, finally and an await on ok, catch on
-- bad, and a cancel hook on held.
local bus = require('bus')

type Waiter = {
    source: AudioSource?,
}

function init(self: Waiter, context: Context): boolean
    local source = context:audio('what')
    self.source = source
    local function play()
        if source then
            Audio.play(source)
        end
    end
    bus.ok:andThen(function(_: number)
        play()
    end)
    bus.ok:finally(play)
    bus.bad:catch(function(_: any)
        play()
        return nil
    end)
    bus.held:onCancel(play)
    async(function()
        await(bus.ok)
        play()
        return true
    end)
    return true
end

return function(): Node<Waiter>
    return {
        source = nil,
        init = init,
    }
end
