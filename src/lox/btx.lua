#!/usr/bin/env lua
-- ======================================================================== --
local next, assert, type = next, assert, type
local strlen, strsub, strmatch, strgmatch = string.len, string.sub, string.match, string.gmatch
local strrep, strgsub, strfind = string.rep, string.gsub, string.find
local tinsert, tremove, tconcat = table.insert, table.remove, table.concat

local btx = {} -- basic toolbox

--- construct normal path
-- @path mixing /, ../, etc
btx.normpath = function (path) -- {{{ full, base, name
    local o = {}
    for v in strgmatch(strgsub(strgsub(strgsub(path, '^%s*', ''), '%s*$', ''), '/+/', '/'), '[^/]*') do
        if v == '..' then
            if #o == 0 or o[#o] == '..' then tinsert(o, v) elseif o[#o] ~= '' then tremove(o) end
        elseif v ~= '.' then
            tinsert(o, v)
        end
    end
    o = tconcat(o, '/')
    return o, strmatch(o, '(.-/?)([^/]+)$') -- full, base, name
end -- }}}

--- convert string setting to table
-- @tmpl the string
-- @sep the character separate the setting
-- @set the set character
btx.str2tbl = function (tmpl, sep, set) -- {{{
    local res = {}
    if tmpl then
        set = set or '='
        for token in strgmatch(tmpl, '([^'..(sep or ',')..']+)') do
            local k, v = strmatch(token, '([^'..set..']+)'..set..'(.*)')
            if k and v and k ~= '' then
                local q, qo = strmatch(v, '^([\'"])(.*)%1$') -- trim qotation mark
                res[k] = qo or v
            else
                tinsert(res, token)
            end
        end
    end
    return res
end -- }}}

-- see if settings in targ match the template
-- @targ to be tested
-- @tmpl assignment
btx.matchtbl = function (targ, tmpl) -- {{{ -- match assignment in tmpl
    if type(targ) ~= 'table' then return not next(tmpl) end
    if tmpl then
        for k, v in pairs(tmpl) do if targ[k] ~= v then return false end end
    end
    return true
end -- }}}

local function dumpVar (key, value, ctrl) -- {{{ dump variables in lua
    key = (type(key) == 'string' and strfind(key, '%W')) and '["'..key..'"]' or key
    local assign = type(key) == 'number' and '' or key..' = '
    if type(value) == 'number' then return assign..value end
    if type(value) == 'string' then return assign..'"'..strgsub(value, '"', '\\"')..'"' end
    if type(value) ~= 'table' then return '' end
    tinsert(ctrl, type(key) == 'number' and '['..key..']' or key) -- increase the depth
    local extdef, keyhead = ''
    if ctrl.ext then -- the depths to external {{{
        for _ = 1, #(ctrl.ext) do
            if #ctrl == ctrl.ext[_] then
                keyhead = strgsub(tconcat(ctrl, "."), '%.%[', '[')
                break
            end
        end
    end -- }}}
    local res, kset, tmp1 = {}, {}, ctrl['L'..#ctrl]
    for k, v in pairs(value) do
        if type(k) == 'string' then
            tinsert(kset, k)
        elseif type(v) == 'table' and type(k) == 'number' then -- 2D format
            tmp1 = false
        end
    end

    if #value > 0 then -- {{{
        if keyhead then
            for i = #value, 1, -1 do -- {{{
                local v = dumpVar(i, value[i], ctrl)
                if v ~= '' then tinsert(ctrl.def, 1, keyhead..'['..i..'] = '..v) end
            end -- }}}
        else
            for i = 1, #value do -- {{{
                local v = dumpVar(i, value[i], ctrl)
                if v ~= '' then tinsert(res, v) end
            end -- }}}
        end
        if tmp1 and (#res > ctrl['L'..#ctrl]) then -- level L# 2D table @ column
            local w, m = ctrl['L'..#ctrl], {} -- {{{
            for i = 0, #res - 1 do
                local l = string.len(tostring(res[i + 1]))
                m[i % w] = (m[i % w] and m[i % w] >= l) and m[i % w] or l
            end
            for i = 0, #res - 1 do
                res[i + 1] = string.format((i > 0 and (i % w) == 0 and '\n%' or '%')..m[i % w]..'s,', res[i + 1])
            end
            res = {tconcat(res, ' ')} -- }}}
        else --
            tmp1 = false
            res = tconcat(res, ',\n')
            if string.len(res) < ctrl.len then res = strgsub(res, '\n', ' ') end
            res = {res}
        end
    end -- }}}
    if #kset > 0 then -- {{{
        table.sort(kset)
        for i = 1, #kset do
            local v = dumpVar(kset[i], value[kset[i]], ctrl)
            if v ~= '' then -- {{{
                if keyhead then -- recursive so must be the first
                    tinsert(ctrl.def, 1, keyhead..(strsub(v, 1, 1) == '[' and v or '.'..v))
                else
                    tinsert(res, v)
                end
            end -- }}}
        end
    end -- }}}
    kset = #res
    tremove(ctrl)
    res = tconcat(res, ',\n')
    if #ctrl == 0 and #(ctrl.def) > 0 then extdef = '\n'..tconcat(ctrl.def, '\n') end
    if not strfind(res, '\n') then return assign..'{'..res..'}'..extdef end
    if (not tmp1) and string.len(res) < ctrl.len and kset < ctrl.num then
        return assign..'{'..strgsub(res, '\n', ' ')..'}'..extdef
    end
    tmp1 = string.rep(' ', 4)
    return assign..'{\n'..tmp1..strgsub(res, '\n', '\n'..tmp1)..'\n}'..extdef
end -- }}}

btx.dumpVar = function (key, value, ext) -- {{{
    local ctrl = {def = {}, len = 111, num = 11} -- external definitions m# = cloumn_num
    if type(ext) == 'table' then -- {{{
        ctrl.len = ext.len or ctrl.len -- max txt width
        ctrl.num = ext.num or ctrl.num -- max items in one line table
        for k, v in pairs(ext) do
            if type(k) == 'string' and strmatch(k, '^L%d+$') and tonumber(v) and v > 1 then ctrl[k] = v end
        end
        ctrl.ext = ext -- control table
    end -- }}}
    return dumpVar(key, value, ctrl)
end -- }}}

-- e.g. print(btx.dumpVar('a', a, {1, L4=3}))
-- e.g. print(btx.dumpVar('a', a,
-- {1, ['a.b.1.3.5'] = 'ab',
--     ['a.b.1.3.5'] = 13,
-- }))

return btx
-- vim: ts=4 sw=4 sts=4 et foldenable fdm=marker fmr={{{,}}} fdl=1
