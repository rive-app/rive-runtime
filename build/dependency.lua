local m = {}

local last_str = ''

function iop(str)
    io.write(('\b \b'):rep(#last_str)) -- erase old line
    io.write(str) -- write new line
    io.flush()
    last_str = str
end

newoption({ trigger = 'no-download-progress', description = 'Hide progress?' })

function m.github(project, tag)
    local dependencies = os.getenv('DEPENDENCIES')
    if dependencies == nil then
        dependencies = path.getabsolute(_WORKING_DIR) .. '/dependencies'
        os.mkdir(dependencies)
    end
    local hash = string.sub(string.sha1(project .. tag), 0, 9)
    if not os.isdir(dependencies .. '/' .. hash) then
        function progress(total, current)
            local ratio = current / total
            ratio = math.min(math.max(ratio, 0), 1)
            local percent = math.floor(ratio * 100)
            if total == current then
                iop('')
            else
                iop('Downloading ' .. project .. ' ' .. percent .. '%')
            end
        end

        local downloadFilename = dependencies .. '/' .. hash .. '.zip'
        http.download(
            'https://github.com/' .. project .. '/archive/' .. tag .. '.zip',
            downloadFilename,
            -- Download progress explodes the github logs with megabytes of text.
            -- github runners have a "CI" environment variable.
            {
                progress = not _OPTIONS['no-download-progress']
                    and os.getenv('CI') ~= 'true'
                    and progress,
            }
        )
        print('Downloaded ' .. project .. ' to ' .. dependencies .. '/' .. hash)
        zip.extract(downloadFilename, dependencies .. '/' .. hash)
        os.remove(downloadFilename)
    end
    local dirs = os.matchdirs(dependencies .. '/' .. hash .. '/*')

    local iter = pairs(dirs)
    local currentKey, currentValue = iter(dirs)
    return currentValue
end
return m
