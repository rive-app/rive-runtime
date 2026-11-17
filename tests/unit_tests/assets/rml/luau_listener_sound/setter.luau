-- Sets the view model's score once, from its second advance, after the
-- listening scripts have started listening.
type Setter = {
    context: Context?,
    frames: number,
}

function init(self: Setter, context: Context): boolean
    self.context = context
    return true
end

function advance(self: Setter, seconds: number): boolean
    self.frames += 1
    local context = self.context
    if self.frames ~= 2 or context == nil then
        return true
    end
    local vm = context:viewModel()
    local score = if vm then vm:getNumber('score') else nil
    if score then
        score.value += 1
    end
    return true
end

return function(): Node<Setter>
    return {
        context = nil,
        frames = 0,
        init = init,
        advance = advance,
    }
end
