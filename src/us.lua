#!/usr/bin/env lua
-- ================================================================== --
-- useful stuff (us:utility subroutine) Josh Feng (C) MIT license 2022
-- ================================================================== --
local we = {} -- working environment

we.stamp, we.posix = pcall(require, 'posix') -- return status and package

local strgsub, strsub, strgmatch, strmatch, strfind =
    string.gsub, string.sub, string.gmatch, string.match, string.find
local tinsert, tremove, tconcat, tsort =
    table.insert, table.remove, table.concat, table.sort

local Log

we.log = function (logfile) -- {{{ initially no log file: log('logfile')
    Log = {file = io.open(logfile, 'w')}
    if Log.file then
        setmetatable(Log, {__gc = function (o) if o.file then o.file:close() end end})
        we.log = nil -- self destructive, only one log per run
    else
        we.info('WRN: failed writing '..logfile)
    end
end -- }}}

we.info = function (msg, header) -- {{{ message and header: info('msg', 'ERR')
    header = header and header..' ' or ''
    if we.stamp then header = os.date('%T')..' '..header end
    msg = header..we.tbl2str(msg, '\n'..header)
    if Log and Log.file then Log.file:write(msg..'\n') end
    print(msg)
end -- }}}

we.fatal = function (msg) -- {{{ fatal('err msg')
    if msg then we.info('ERR: '..msg) end
    os.exit(msg and 1 or 0)
end -- }}}

we.dbg = function (msg) if we.debug then we.info(msg, 'DBG:') end end
-- ================================================================== --
-- ================  EXTERNAL SUBPROCESS COMMAND   ================== --
-- ================================================================== --
we.popen = function (datastr, cmd) -- {{{ status, out, err = we.popen(in, cmd)
    local posix = we.posix or posix
    if not posix then return 'Not supported' end
    --  thread
    --  +--------+
    --  | i-rw-> | pipe (is uni-directional)
    --  | <-rw-o | pipe
    --  | <-rw-e | ...
    --  v ...... v
    --  Parent   Children

    -- usage example: local status, out, err = we.popen(in, 'wc -c')
    local ri, wi = posix.pipe() -- for child stdin
    local ro, wo = posix.pipe() -- for child stdout
    local re, we = posix.pipe() -- for child stderr
    assert(wi or ro or re, 'pipe() failed')
    local pid, err = posix.fork() -- child proc
    if pid == 0 then -- for child proc {{{
        posix.close(wi)
        posix.close(ro)
        posix.close(re)
        posix.dup2(ri, posix.fileno(io.stdin))
        posix.dup2(wo, posix.fileno(io.stdout))
        posix.dup2(we, posix.fileno(io.stderr))
        -- local ret, err = posix.execp(path, argt or {}) -- w/ shell
        local ret, err = posix.exec('/bin/sh', {'-c', cmd}) -- w/o shell
        posix.close(ri)
        posix.close(wo)
        posix.close(we)
        posix._exit(1)
    end -- }}}
    -- for parent proc
    posix.close(ri)
    posix.close(wo)
    posix.close(we)

    -- pid, wi, ro, re -- posix pid and posix's filedes
    -- send to stdin
    posix.write(wi, datastr)
    posix.close(wi)
    -- get from stdout
    local stdout = {}
    while true do
        local buf = posix.read(ro, 4096)
        if buf == nil or #buf == 0 then break end
        tinsert(stdout, buf)
    end
    -- posix.close(r3)
    local stderr = {}
    while true do
        local buf = posix.read(re, 4096)
        if buf == nil or #buf == 0 then break end
        tinsert(stderr, buf)
    end
    return posix.wait(pid), tconcat(stdout), tconcat(stderr)
end -- }}}

we.cmd = function (cmd, multi) -- {{{ do as told quietly: cmd('cd / ; ls', true)
    we.dbg(cmd)
    if multi then cmd = string.gsub(cmd, ';', ' > /dev/null 2>&1 ;') end
    return assert(os.execute(cmd..' > /dev/null 2>&1'))
end -- }}}

we.ask = function (cmd, multi, err) -- {{{ Est-ce-que (Alor, on veut savoir le resultat)
    we.dbg(cmd)
    if err and multi then cmd = string.gsub(cmd, ';', ' 2>&1;') end
    -- NB: lua use (POSIX) sh, we can use 2>&1 to redirect stderr to stdout
    local file = io.popen(cmd..(err and ' 2>&1' or ''), 'r')
    local msg = file:read('*all')
    file:close()
    return msg
end -- }}}

we.put = function (str, cmd) -- {{{ put to stdin (str?)
    we.dbg(cmd)
    local file = io.popen(cmd, 'w')
    local result = file:write(str)
    file:close()
end -- }}}

we.exist = function (path) -- {{{ isdir : we.exist(path..'/.')
    path = io.open(path, 'r')
    return path and (path:close() or true)
end -- }}}
-- ================================================================== --
-- ======================  SIMPLE I/O  ============================== --
-- ================================================================== --
we.loadStr = function (filename, verify) -- {{{ load a file into a string
    local file, msg = io.open(filename, 'r')
    if file == nil then return verify and error(msg) end
    local chunk = file:read('*all')
    file:close()
    return chunk
end -- }}}

we.emitStr = function (o, filename, verify) -- {{{ emit a string to a file
    local file, msg = io.open(filename, 'w')
    if file == nil then return verify and error(msg) or msg end
    file:write(type(o) == 'table' and we.tbl2str(o, '\n') or tostring(o))
    file:close()
end -- }}}
-- ================================================================== --
-- =====================  STRING FUNCTIONS  ========================= --
-- ================================================================== --
we.split = function (str, sep) -- {{{ split string w/ ':' into a table
    if type(str) ~= 'string' then return end
    local t = {}
    for o in strgmatch(str, '[^'..(sep or ':')..']*') do tinsert(t, o) end
    return t
end -- }}}

local pm = '().+-*?[^$' -- need to put % first
we.strpm = function (str) -- pattern matching string {{{ -- escape: ().%+-*?[^$
    str = strgsub(str, '%%', '%%%%')
    for s = 1, #pm do
        s = '%'..strsub(pm, s, s)
        str = strgsub(str, s, '%'..s)
    end
    return str
end -- }}}

we.trim = function (...) -- {{{ trim space
    local res = {}
    for i = 1, select('#', ...) do
        tinsert(res, strmatch(tostring(select(i, ...)), '(%S.-)%s*$') or '')
    end
    return table.unpack(res)
end -- }}}

we.realpath = function (bin) -- {{{ trace the binary link / realpath
    repeat
        local l = strmatch(io.popen('ls -ld '..bin..' 2>/dev/null'):read('*all'), '->%s+(.*)') -- link
        if l then bin = strsub(l, 1, 1) == '/' and l or strgsub(bin, '[^/]*$', '')..l end
    until not l
    return we.normpath(bin)
end -- }}}

we.normpath = function (path, pwd) -- {{{ full, base, name
    if pwd and strsub(path, 1, 1) ~= '/' then path = pwd..'/'..path end
    local o = {}
    for i, v in ipairs(we.split(strgsub(path, '/+/', '/'), '/')) do
        if v == '..' then
            if #o == 0 or o[#o] == '..' then tinsert(o, v) elseif o[#o] ~= '' then tremove(o) end
        elseif v ~= '.' then
            tinsert(o, v)
        end
    end
    o = we.trim(tconcat(o, '/'))
    return o, strmatch(o, '(.-/?)([^/]+)$') -- full, base, name
end -- }}}

we.getstem = function (path) -- {{{
    return strgsub(strgsub(path, '^.*/', ''), '%.[^.]*$', '')
end -- }}}

we.tbl2str = function (tbl, sep) -- {{{ build the set
    if type(tbl) ~= 'table' then return tostring(tbl) end
    local res = {}
    local assign = (sep and string.len(sep) > 1) and ' = ' or '='
    for k, v in pairs(tbl) do
        v = tostring(v)
        table.insert(res, type(k) == 'string' and k..assign..v or v)
    end
    if not sep then table.sort(res) end
    return tconcat(res, sep or ',')
end -- }}}

we.str2tbl = function (txt) -- {{{ -- build the tmpl from string
    local res = {}
    local var
    while txt ~= '' do
        var, txt = strmatch(strsub(txt, 2, #txt), '^([_%w]*)(.*)$')
        local c = strsub(txt, 1, 1)
        if tonumber(var) then
            tinsert(res, tonumber(var))
        elseif c == ',' then -- sep
            txt = strsub(txt, 2, #txt)
            res[var] = true
        elseif c == '=' then -- set (NB: '/" ...)
            c = strsub(txt, 2, 2)
            if c == '"' or c == "'" then
                c, txt = strmatch(strsub(txt, 3, #txt), '^([^'..c..']*)'..c..'(.*)$')
                if not c then error('Wrong var quote', 2) end
                res[var] = c
            else
                c, txt = strmatch(strsub(txt, 2, #txt), '^([^,%]]*)(.*)$')
                res[var] = c
            end
        else
            error('Wrong var setting: '..var..':'..txt, 2)
        end
    end
    return res
end -- }}}

we.match = function (targ, tmpl, fExact) -- {{{ -- match assignment in tmpl
    if type(targ) ~= 'table' then return not next(tmpl) end
    if tmpl then
        for k, v in pairs(tmpl) do
            if (type(k) == 'number' and type(v) == 'string' and targ[v] == nil)
                or (targ[k] ~= v) then
                return false
            end
        end
    end
    if fExact then
        for k in pairs(targ) do
            if type(k) == 'string' and tmpl[k] == nil then return false end
        end
    end
    return true
end -- }}}

we.check = function (v) -- {{{ -- check v is true or false
    if type(v) == 'boolean' then return v end
    if tonumber(v) then return tonumber(v) ~= 0 end
    v = string.lower(tostring(v))
    return (v == 'true') or (v == 'yes') or (v == 'y')
end -- }}}
-- ================================================================== --
local function var2str (value, key, ctrl) -- {{{ emit variables in lua
    key = (type(key) == 'string' and strfind(key, '[^_%w]')) and '["'..key..'"]' or
          (type(key) == 'boolean' and '['..tostring(key)..']' or key)

    local assign = (key == nil) and ""
        or (type(key) == 'number' and '['..tostring(key)..']' or key)..' = '

    if type(value) == 'number' then return assign..value end
    if type(value) == 'string' then return assign..'"'..strgsub(value, '"', '\\"')..'"' end
    if type(value) ~= 'table' then return '' end

    -- tinsert(ctrl, type(key) == 'number' and '['..key..']' or key) -- increase the depth
    tinsert(ctrl, key or '') -- increase the depth

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
        if type(k) ~= 'number' or k < 1 or k > #value  then
            v = tostring(k)
            kset[v] = k
            tinsert(kset, v)
        elseif type(v) == 'table' then -- 2D format
            tmp1 = false
        end
    end

    if #value > 0 then -- {{{
        if keyhead then
            for i = #value, 1, -1 do -- {{{
                -- local v = var2str(value[i], i, ctrl)
                local v = var2str(value[i], nil, ctrl)
                if v ~= '' then tinsert(ctrl.def, 1, keyhead..'['..i..'] = '..v) end
            end -- }}}
        else
            for i = 1, #value do -- {{{
                -- local v = var2str(value[i], i, ctrl)
                local v = var2str(value[i], nil, ctrl)
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
            local v = kset[kset[i]]
            v = var2str(value[v], (type(v) == 'number' or type(v) == 'boolean') and v or kset[i], ctrl)
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

we.var2str = function (value, key, ext) -- {{{
    -- e.g. print(we.var2str(a, 'a', {1, L4=3}))
    -- e.g. print(we.var2str(a, 'a', {1, ['a.b.1.3.5'] = 'ab', ['a.b.1.3.5'] = 13, }))
    local ctrl = {def = {}, len = 111, num = 11} -- external definitions m# = cloumn_num
    if type(ext) == 'table' then -- {{{
        ctrl.len = ext.len or ctrl.len -- max txt width
        ctrl.num = ext.num or ctrl.num -- max items in one line table
        for k, v in pairs(ext) do
            if type(k) == 'string' and strmatch(k, '^L%d+$') and tonumber(v) and v > 1 then ctrl[k] = v end
        end
        ctrl.ext = ext -- control table
    end -- }}}
    return var2str(value, key, ctrl)
end -- }}}
-- ================================================================== --
-- =====================  TABLES FUNCTIONS  ========================= --
-- ================================================================== --
we.cloneTbl = function (src) -- {{{ fully copy table
    local tbls = {}
    local function cloneTbl (_src)
        if tbls[_src] then return tbls end
        local targ = {}
        tbls[_src] = targ
        for k, v in pairs(_src) do
            targ[k] = type(v) == 'table' and cloneTbl(v) or v
        end
        local mt = getmetatable(_src)
        if type(mt) == 'table' then setmetatable(targ, cloneTbl(mt)) end
        return targ
    end
    return cloneTbl(src)
end -- }}}

we.traceTbl = function (tbl, testkey, procvalue, ...) -- {{{ table trace
    for k, v in pairs(tbl) do
        if testkey(k) then
            procvalue(v, ...) -- record in ...
        elseif type(v) == 'table' then -- trace the sub table
            we.traceTbl(v, testkey, procvalue, ...)
        end
    end
end -- }}}
return we
-- vim:ts=4:sw=4:sts=4:et:fen:fdm=marker:fmr={{{,}}}:fdl=1
