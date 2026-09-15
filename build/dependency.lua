local m = {}

local last_str = ''

function iop(str)
    io.write(('\b \b'):rep(#last_str)) -- erase old line
    io.write(str) -- write new line
    io.flush()
    last_str = str
end

newoption({ trigger = 'no-download-progress', description = 'Hide progress?' })

function m.github(project, tag, opts)
    local dependencies = os.getenv('DEPENDENCIES')
    if dependencies == nil then
        dependencies = path.getabsolute(_WORKING_DIR) .. '/dependencies'
        os.mkdir(dependencies)
    end
    local dirname = project .. '_' .. tag
    dirname = string.gsub(dirname, '/', '_')
    local dependency_path = dependencies .. '/' .. dirname
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
        -- Local patches apply once, on the fresh clone; a dependency dir
        -- that predates its patch set must be deleted to pick patches up.
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
        end
    end
    assert(os.isdir(dependency_path))
    return dependency_path
end
return m
