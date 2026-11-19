-- A whole-canvas touch handler, as a game in its own canvas would have: it
-- claims every press, move and release, and counts each event, and each
-- exit, on the root view model.
type Touch = {
    context: Context?,
}

local function count(self: Touch, name: string)
    local context = self.context
    if context == nil then
        return
    end
    local vm = context:rootViewModel()
    if vm == nil then
        return
    end
    local n = vm:getNumber(name)
    if n then
        n.value += 1
    end
end

function init(self: Touch, context: Context): boolean
    self.context = context
    return true
end

function pointerDown(self: Touch, event: PointerEvent)
    count(self, 'downs')
    event:hit()
end

function pointerMove(self: Touch, event: PointerEvent)
    count(self, 'moves')
    event:hit()
end

function pointerUp(self: Touch, event: PointerEvent)
    count(self, 'ups')
    event:hit()
end

function pointerExit(self: Touch, event: PointerEvent)
    count(self, 'exits')
end

return function(): Node<Touch>
    return {
        context = nil,
        init = init,
        pointerDown = pointerDown,
        pointerMove = pointerMove,
        pointerUp = pointerUp,
        pointerExit = pointerExit,
    }
end
