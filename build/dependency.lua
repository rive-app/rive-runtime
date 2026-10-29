local m = {}

local last_str = ''

function iop(str)
    io.write(('\b \b'):rep(#last_str)) -- erase old line
    io.write(str) -- write new line
    io.flush()
    last_str = str
end

newoption({ trigger = 'no-download-progress', description = 'Hide progress?' })

-- The patch set a clone was built with, one "<name> <bytes>" line per patch,
-- sorted. Written into the clone after patching and compared on every later
-- run, because patches apply once and a clone that predates a patch is
-- otherwise indistinguishable from a current one.
--
-- Name and size rather than content: an added patch is the case this exists
-- for, and both catch it. A patch edited in place to the same byte count is
-- not caught, and deleting the dependency directory remains the answer there.
local function patchStamp(opts)
    if opts == nil or opts.patches == nil then
        return nil
    end
    local patches = os.matchfiles(opts.patches .. '/*.patch')
    table.sort(patches)
    local lines = {}
    for _, patch in ipairs(patches) do
        local size = 0
        local file = io.open(patch, 'rb')
        if file ~= nil then
            size = file:seek('end')
            file:close()
        end
        table.insert(lines, path.getname(patch) .. ' ' .. tostring(size))
    end
    return table.concat(lines, '\n') .. '\n'
end

local function readStamp(stampPath)
    local file = io.open(stampPath, 'rb')
    if file == nil then
        return nil
    end
    local contents = file:read('*a')
    file:close()
    return contents
end

function m.github(project, tag, opts)
    local dependencies = os.getenv('DEPENDENCIES')
    if dependencies == nil then
        dependencies = path.getabsolute(_WORKING_DIR) .. '/dependencies'
        os.mkdir(dependencies)
    end
    local dirname = project .. '_' .. tag
    dirname = string.gsub(dirname, '/', '_')
    local dependency_path = dependencies .. '/' .. dirname
    local stamp = patchStamp(opts)
    local stampPath = dependency_path .. '/.rive_patch_stamp'
    -- A clone whose stamp disagrees with the patch set on disk is thrown away
    -- and fetched again. Compared even when no patches are configured, so
    -- that dropping the last one refetches a clone that still carries them.
    -- A dependency that never had patches has no stamp and expects none, so
    -- it matches and is reused.
    --
    -- A clone from before this stamp existed has none either, so one with
    -- patches is refetched once. That is the only honest answer, since
    -- nothing records what it was patched with.
    if os.isdir(dependency_path) and readStamp(stampPath) ~= stamp then
        print('Patch set changed, refetching ' .. dirname .. '...')
        os.rmdir(dependency_path)
        if os.isdir(dependency_path) then
            error(
                '\nCould not remove '
                    .. dependency_path
                    .. '\nDelete it and build again.'
            )
        end
    end
    if not os.isdir(dependency_path) then
        print('Fetching dependency ' .. project .. ' at tag ' .. tag .. '...')
        local gitcmd = 'git -c advice.detachedHead=false -C '
            .. dependencies
            .. ' clone --depth 1 --branch '
            .. tag
            .. ' https://github.com/'
            .. project
            .. '.git'
            .. ' '
            .. dirname
        if not os.execute(gitcmd) then
            error('\nError executing command:\n  ' .. gitcmd)
        end
        -- Local patches apply once, on the fresh clone. The stamp written
        -- after them is what lets a later run notice the set has moved.
        if opts ~= nil and opts.patches ~= nil then
            local patches = os.matchfiles(opts.patches .. '/*.patch')
            table.sort(patches)
            for _, patch in ipairs(patches) do
                print('Applying ' .. patch)
                local applycmd = 'git -C '
                    .. dependency_path
                    .. ' apply '
                    .. patch
                if not os.execute(applycmd) then
                    error('\nError executing command:\n  ' .. applycmd)
                end
            end
            local file = io.open(stampPath, 'wb')
            if file == nil then
                error('\nCould not write ' .. stampPath)
            end
            file:write(stamp)
            file:close()
        end
    end
    assert(os.isdir(dependency_path))
    if opts ~= nil and opts.submodules ~= nil and #opts.submodules > 0 then
        local gitcmd = 'git -C '
            .. dependency_path
            .. ' submodule update --init --depth 1 -- '
            .. table.concat(opts.submodules, ' ')
        if not os.execute(gitcmd) then
            error('\nError executing command:\n  ' .. gitcmd)
        end
    end
    return dependency_path
end
return m
