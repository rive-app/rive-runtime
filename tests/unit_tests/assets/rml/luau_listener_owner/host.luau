-- Makes an instance of deck on the view model it is bound to and listens to
-- echo, allocating in the listener. Then drops the instance and changes the
-- value the instance's script listens to: relay if nested is set, else score.
type Host = {
    deck: Input<Artboard>,
    context: Context?,
    inner: Artboard?,
    echo: Property<number>?,
    frames: number,
}

local function onEcho(self: Host)
    local junk = {}
    for i = 1, 200000 do
        junk[i % 64] = { i }
    end
end

function init(self: Host, context: Context): boolean
    self.context = context
    return true
end

function advance(self: Host, seconds: number): boolean
    self.frames += 1
    local context = self.context
    local vm = if context then context:viewModel() else nil
    if vm == nil then
        return true
    end
    if self.frames == 1 then
        local inner = self.deck:instance(vm)
        self.inner = inner
        inner:advance(seconds)
        local echo = vm:getNumber('echo')
        if echo then
            self.echo = echo
            echo:addListener(self, onEcho)
        end
    elseif self.frames == 2 then
        -- Nothing on the stack holds the instance in this call.
        self.inner = nil
        local nested = vm:getBoolean('nested')
        local name = if nested and nested.value then 'relay' else 'score'
        local value = vm:getNumber(name)
        if value then
            value.value += 1
        end
    end
    return true
end

return function(): Node<Host>
    return {
        deck = late(),
        context = nil,
        inner = nil,
        echo = nil,
        frames = 0,
        init = init,
        advance = advance,
    }
end
