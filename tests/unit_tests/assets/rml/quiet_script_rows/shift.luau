-- A list row's path effect. It moves its shape's path right by its offset
-- input plus one unit per step, and down one unit per update. A change to
-- offset gives it two steps, one per advance, then it sleeps; any other
-- update (async work the host delivers) only moves it down.
type Shift = {
    offset: Input<number>,
    context: Context?,
    seenOffset: number,
    steps: number,
    updates: number,
    left: number,
    out: Path,
}

function init(self: Shift, context: Context): boolean
    self.context = context
    return true
end

function update(self: Shift, inPath: PathData, node: NodeReadData): PathData
    self.updates += 1
    if self.offset ~= self.seenOffset then
        self.seenOffset = self.offset
        self.left = 2
    end
    local dx = self.offset + self.steps
    local dy = self.updates
    local out = self.out
    out:reset()
    local contour: ContourMeasure? = inPath:contours()
    while contour do
        local length = contour.length
        for i = 0, 4 do
            local position = contour:positionAndTangent(length * i / 4)
            local point = Vector.xy(position.x + dx, position.y + dy)
            if i == 0 then
                out:moveTo(point)
            else
                out:lineTo(point)
            end
        end
        contour = contour.next
    end
    return out
end

-- Each step asks for an update, which redraws the path with it.
function advance(self: Shift, seconds: number): boolean
    if self.left <= 0 then
        return false
    end
    self.left -= 1
    self.steps += 1
    if self.context then
        self.context:markNeedsUpdate()
    end
    return true
end

return function(): PathEffect<Shift>
    return {
        offset = 0,
        context = nil,
        seenOffset = 0,
        steps = 0,
        updates = 0,
        left = 2,
        out = Path.new(),
        init = init,
        update = update,
        advance = advance,
    }
end
