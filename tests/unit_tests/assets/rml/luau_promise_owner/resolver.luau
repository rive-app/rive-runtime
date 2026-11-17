-- From its first advance, resolves ok, rejects bad and cancels held, which
-- runs Waiter's handlers inside this hook.
local bus = require('bus')

type Resolver = {
    done: boolean,
}

function advance(self: Resolver, seconds: number): boolean
    if not self.done then
        self.done = true
        bus.settle()
    end
    return true
end

return function(): Node<Resolver>
    return {
        done = false,
        advance = advance,
    }
end
