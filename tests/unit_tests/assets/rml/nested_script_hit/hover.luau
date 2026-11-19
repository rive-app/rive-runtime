-- Hears moves without claiming them, and has no press handler: it counts
-- each move and exit on the root view model.
type Hover = {
    context: Context?,
}

local function count(self: Hover, name: string)
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

function init(self: Hover, context: Context): boolean
    self.context = context
    return true
end

function pointerMove(self: Hover, event: PointerEvent)
    count(self, 'moves')
end

function pointerExit(self: Hover, event: PointerEvent)
    count(self, 'exits')
end

return function(): Node<Hover>
    return {
        context = nil,
        init = init,
        pointerMove = pointerMove,
        pointerExit = pointerExit,
    }
end
