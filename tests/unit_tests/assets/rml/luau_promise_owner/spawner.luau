-- From its first advance, resolves spawn, which runs Hatcher's handler
-- inside this hook.
local bus = require('bus')

type Spawner = {
    done: boolean,
}

function advance(self: Spawner, seconds: number): boolean
    if not self.done then
        self.done = true
        bus.hatch()
    end
    return true
end

return function(): Node<Spawner>
    return {
        done = false,
        advance = advance,
    }
end
