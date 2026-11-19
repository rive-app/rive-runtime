-- touch.luau as a scripted layout that takes all the room its parent gives
-- it, as a full-screen game's world would.
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

function measure(self: Touch): Vector
    return Vector.xy(10000, 10000)
end

return function(): Layout<Touch>
    return {
        context = nil,
        init = init,
        pointerDown = pointerDown,
        pointerMove = pointerMove,
        pointerUp = pointerUp,
        pointerExit = pointerExit,
        measure = measure,
    }
end
