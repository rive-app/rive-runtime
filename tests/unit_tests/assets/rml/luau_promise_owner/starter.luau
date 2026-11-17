-- From init, runs an async function that plays the sound before its await
-- on ok and again after it.
local bus = require('bus')

type Starter = {}

function init(self: Starter, context: Context): boolean
    local source = context:audio('what')
    async(function()
        if source then
            Audio.play(source)
        end
        await(bus.ok)
        if source then
            Audio.play(source)
        end
        return true
    end)
    return true
end

return function(): Node<Starter>
    return {
        init = init,
    }
end
