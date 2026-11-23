-- A list row's script. It keeps advancing a few frames after anything wakes
-- it, then sleeps, and reports into its item's view model what woke it.
type Probe = {
    level: Input<number>,
    context: Context?,
    listening: boolean,
    left: number,
}

local function bump(vm: any, name: string)
    local value = vm:getNumber(name)
    if value then
        value.value += 1
    end
end

-- A change to poke asks for an update, which runs update and then advance.
local function onPoke(self: Probe)
    if self.context then
        self.context:markNeedsUpdate()
    end
end

function init(self: Probe, context: Context): boolean
    self.context = context
    return true
end

function update(self: Probe)
    local context = self.context
    local vm = if context then context:viewModel() else nil
    if vm then
        local doubled = vm:getNumber('doubled')
        if doubled then
            doubled.value = self.level * 2
        end
    end
    self.left = 2
end

function advance(self: Probe, seconds: number): boolean
    local context = self.context
    local vm = if context then context:viewModel() else nil
    if context == nil or vm == nil then
        return true
    end
    -- The row's view model is bound by the first advance, not in init.
    if not self.listening then
        self.listening = true
        local poke = vm:getNumber('poke')
        if poke then
            poke:addListener(self, onPoke)
        end
    end
    bump(vm, 'advances')
    local artboard = context:artboard()
    local box = if artboard then artboard:node('Box') else nil
    local seen = vm:getNumber('seen')
    if box and seen then
        seen.value = box.x
    end
    local busy = vm:getBoolean('busy')
    if busy and busy.value then
        return true
    end
    if self.left > 0 then
        self.left -= 1
        return true
    end
    return false
end

function pointerDown(self: Probe, event: PointerEvent)
    local context = self.context
    local vm = if context then context:viewModel() else nil
    if vm then
        bump(vm, 'presses')
    end
end

return function(): Node<Probe>
    return {
        level = 0,
        context = nil,
        listening = false,
        left = 2,
        init = init,
        update = update,
        advance = advance,
        pointerDown = pointerDown,
    }
end
