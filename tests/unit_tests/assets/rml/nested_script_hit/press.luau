-- Counts each press on the root view model without claiming it, so a hit
-- shape in the same artboard still hears the press.
type Press = {
    context: Context?,
}

function init(self: Press, context: Context): boolean
    self.context = context
    return true
end

function pointerDown(self: Press, event: PointerEvent)
    local context = self.context
    if context == nil then
        return
    end
    local vm = context:rootViewModel()
    if vm == nil then
        return
    end
    local n = vm:getNumber('downs')
    if n then
        n.value += 1
    end
end

return function(): Node<Press>
    return {
        context = nil,
        init = init,
        pointerDown = pointerDown,
    }
end
