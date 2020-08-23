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

return btx
-- vim: ts=4 sw=4 sts=4 et foldenable fdm=marker fmr={{{,}}} fdl=1
