-- Promises every script that requires this module shares: one script's
-- handlers wait on them and another script settles them.
type Bus = {
    ok: Promise<number>,
    bad: Promise<number>,
    held: Promise<number>,
    spawn: Promise<number>,
    boom: Promise<number>,
    settle: () -> (),
    hatch: () -> (),
    fumble: () -> (),
}

local resolveOk: (number) -> () = function(_: number) end
local rejectBad: (any) -> () = function(_: any) end
local resolveSpawn: (number) -> () = function(_: number) end
local resolveBoom: (number) -> () = function(_: number) end

local bus: Bus = {
    ok = Promise.new(function(resolve, _reject, _onCancel)
        resolveOk = resolve
    end),
    bad = Promise.new(function(_resolve: (number) -> (), reject, _onCancel)
        rejectBad = reject
    end),
    held = Promise.new(function(_resolve: (number) -> (), _reject, _onCancel) end),
    spawn = Promise.new(function(resolve, _reject, _onCancel)
        resolveSpawn = resolve
    end),
    boom = Promise.new(function(resolve, _reject, _onCancel)
        resolveBoom = resolve
    end),
    settle = function() end,
    hatch = function() end,
    fumble = function() end,
}

bus.settle = function()
    resolveOk(1)
    rejectBad('no')
    bus.held:cancel()
end

bus.hatch = function()
    resolveSpawn(1)
end

bus.fumble = function()
    resolveBoom(1)
end

return bus
