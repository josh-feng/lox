#!/usr/bin/env lua
-- ================================================================== --
-- Lua Object Model: on top of 'lxp'
-- DOM: tbm = {['.'] = tag; ['@'] = {}; ['&'] = {{}..}; {'comment'}, ...}
-- Usage example:
--      lom = require('lom')
--      doc = lom(xmlfile or '')
--      doc = lom() doc:parse(txt):parse()
--      lom(true) -- buildxlink
--      xmltxt = doc:dump(1)
-- ================================================================== --
local lxp = require('lxp') -- the standard Lua Expat module
local class = require('pool') -- https://github.com/josh-feng/pool.git
local we = require('us') -- https://github.com/josh-feng/lox.git

local next, assert, type = next, assert, type
local strlen, strsub, strmatch, strgmatch = string.len, string.sub, string.match, string.gmatch
local strrep, strgsub, strfind = string.rep, string.gsub, string.find
local tinsert, tremove, tconcat = table.insert, table.remove, table.concat
-- ================================================================== --
-- LOM (Lua Object Model)
-- ================================================================== --
local docs = {} -- xml object list (hidden upvalue)

local function starttag (p, name, attr) -- {{{
    local stack = p:getcallbacks().stack
    tinsert(stack, {['.'] = name, ['@'] = #attr > 0 and attr or nil})
end -- }}}
local function endtag (p, name) -- {{{
    local stack = p:getcallbacks().stack
    local element = tremove(stack)
    -- assert(element['.'] == name)
    tinsert(stack[#stack], element)
end -- }}}
local function cleantext (p, txt) -- {{{
    if strfind(txt, '%S') then
        txt = strmatch(txt, '^.*%S')
        local stack = p:getcallbacks().stack
        tinsert(stack[#stack], txt)
    end
end -- }}}
local function text (p, txt) -- {{{
    local stack = p:getcallbacks().stack
    tinsert(stack[#stack], txt)
end -- }}}
local function comment (p, txt) -- {{{
    local stack = p:getcallbacks().stack
    tinsert(stack[#stack], {txt})
end -- }}}

local function parse (o, txt) -- friend function {{{
    local p = o['*']
    local status, msg, line, col, pos = p:parse(txt) -- pass nil if failed
    if not (txt and status) then
        if not status then o['?'] = {msg..' #'..line} end
        p:close() -- seems destroy the lxp obj
        o['*'] = nil
        o.parse = nil
    end
    return o -- for cascade oop
end --}}}
-- ================================================================== --
local function xPath (o, path, doc) -- {{{ return doc/xml-node table, missingTag
    if (not path) or path == '' or #doc == 0 then return doc, path end
    -- xpath syntax: NB: xpointer does not have standard treatment
    -- /A/B[@attr="val",@bb='4']
    -- anywhere/A/B[-3]/-2/3
    local anywhere = strsub(path, 1, 1) ~= '/'
    local tagatt
    tagatt, path = strmatch(path, '([^/]+)(.*)$')
    local idx = tonumber(tagatt)
    if idx then return xPath(o, path, {doc[(idx - 1) % #doc + 1]}) end
    local tag, attr = strmatch(tagatt, '([^%[]+)%[?([^%]]*)')
    local attrspec, autopass = we.str2tbl(attr)

    local xn = {} -- xml-node (doc)
    local docl = doc['&']
    local docn = docl and 0
    repeat
        for i = 1, #doc do
            local mt = doc[i]
            if type(mt) == 'table' then
                if mt['.'] == tag and (autopass or we.match(mt['@'], attrspec)) then
                    tinsert(xn, mt)
                elseif anywhere and (#mt > 0 or mt['&']) then
                    local mtl = mt['&']
                    local mtn = mtl and 0
                    repeat
                        local sub = xPath(o, tagatt, mt)
                        for j = 1, #sub do tinsert(xn, sub[j]) end
                        if mtn then mtn = mtn < #mtl and mtn + 1 end
                        mt = mtn and mtl[mtn]
                    until not mt
                end
            end
        end
        if docn then docn = docn < #docl and docn + 1 end
        doc = docn and docl[docn]
    until not doc

    if #attrspec > 0 and #xn > 0 then -- collect the indixed table
        local nxn = {}
        for i = 1, #attrspec do
            if type(attrspec[i]) == 'number' then
                tinsert(nxn, xn[(attrspec[i] - 1) % #xn + 1])
            end
        end
        if #nxn ~= 0 then xn = nxn end -- collected
    end
    if path ~= '' and #xn > 0 then -- not final so break for further search
        local nxn = {}
        for i = 1, #xn do
            local mt = xn[i]
            for j = 1, #mt do tinsert(nxn, mt[j]) end
        end
        xn = nxn
    end
    return xPath(o, path, xn)
end -- }}}
-- ================================================================== --
local function xmlstr (s, fenc) -- {{{
    -- encode: gzip -c | base64 -w 128
    -- decode: base64 -i -d | zcat -f
    -- return '<!-- base64 -i -d | zcat -f -->{{{'..
    --     we.popen(s, 'we.gzip -c | base64 -w 128'):read('*all')..'}}}'
    s = tostring(s)
    if strfind(s, '\n') or (strlen(s) > 1024) then -- large text
        if fenc or strfind(s, ']]>') then -- enc flag or hostile strings
            local status, stdout, stderr = we.popen(s, 'gzip -c | base64 -w 128')
            return '<!-- base64 -i -d | zcat -f -->{{{'..stdout..'}}}'
        else
            -- return (strfind(s, '"') or strfind(s, "'") or strfind(s, '&') or
            --         strfind(s, '<') or strfind(s, '>')) and '<![CDATA[\n'..s..']]>' or s
            return (strfind(s, '&') or strfind(s, '<') or strfind(s, '>'))
                and '<![CDATA[\n'..s..']]>' or s
        end
    else -- escape characters
        return strgsub(strgsub(strgsub(strgsub(strgsub(s,
            '&', '&amp;'), '"', '&quot;'), "'", '&apos;'), '<', '&lt;'), '>', '&gt;')
    end
end -- }}}

local function dumpLom (node) -- {{{
    if 'string' == type(node) then return node end
    if not node['.'] then return node[1] and '<!--'..node[1]..'-->' end
    local res = {}
    if node['@'] then
        for _, k in ipairs(node['@']) do tinsert(res, k..'="'..strgsub(node['@'][k], '"', '\\"')..'"') end
    end
    res = '<'..node['.']..(#res > 0 and ' '..tconcat(res, ' ') or '')
    if #node == 0 then return res..' />' end
    res = {res..'>'}
    for i = 1, #node do tinsert(res, type(node[i]) == 'table' and dumpLom(node[i]) or xmlstr(node[i])) end
    if #res == 2 and #(res[2]) < 100 and not strfind(res[2], '\n') then
        return res[1]..res[2]..'</'..node['.']..'>'
    end
    return strgsub(tconcat(res, '\n'), '\n', '\n  ')..'\n</'..node['.']..'>' -- indent 2
end -- }}}

local dom = class { -- lua document object model
    ['.'] = false; -- tag name
    ['@'] = false; -- attr
    ['&'] = false; -- xlink table
    ['#'] = false; -- stamp
    ['?'] = false; -- errors
    ['*'] = false; -- module
    ['+'] = false; -- data array

    ['<'] = function (o, spec, mode) --{{{
        if type(spec) == 'table' then -- partial table-tree
            for k, v in pairs(spec) do o[k] = v end
        elseif type(spec) == 'string' then -- '' for text
            local p = lxp.new {
                StartElement = starttag,
                EndElement = endtag,
                CharacterData = mode and text or cleantext,
                Comment = mode and comment or nil,
                _nonstrict = true,
                stack = {o} -- {{}}
            }

            if spec == '' then
                o['*'] = p
                o.parse = parse
            else
                local file, msg = io.open(spec, 'r')
                if file then
                    local status, msg, line, col, pos = p:parse(file:read('*all'))
                    file:close()
                    if status then status, msg, line = p:parse() end
                    if not status then o['?'] = {msg..' #'..line} end
                    p:close()
                else
                    o['?'] = {msg}
                end
            end
        end
        if type(spec) == 'string' and spec ~= '' then
            docs[spec] = o
        else
            tinsert(docs, o)
        end
    end; --}}}

    ['>'] = function (o) for i = 1, #o do o[i] = nil end end;

    parse = false; -- implemented in friend function

    xpath = function (o, path, doc) return xPath(o, path, doc or o) end;

    -- output
    dump = function (o, fxml) -- {{{ dump fxml=1/html
        if not fxml then return we.dumpVar(0, o) end
        local res = {fxml == 1 and '<?xml version="1.0" encoding="UTF-8"?>' or nil}
        local docl = o['&']
        local docn = docl and 0
        repeat
            for j = 1, #o do tinsert(res, dumpLom(o[j])) end
            if docn then docn = docn < #docl and docn + 1 end
            o = docn and docl[docn]
        until not o
        return tconcat(res, '\n')
    end;-- }}}

    -- member functions supporting cascade oo style {{{
    select = function (o, path) return class:new(o, o:xpath(path)) end;

    attr = function (o, var, val) -- {{{
        if type(val) == 'function' then -- handle data
            if type(o['+']) ~= 'table' then return end
            for i = 1, #(o['+']) do -- TODO
                -- o[i]['@'] = o[i]['@'] or {}
                -- if o[i]['@'][var] == nil then tinsert(o[i]['@'], var) end
                -- o[i]['@'][var] = val(o['*'], i)
            end
        elseif val then
            for _, t in ipairs(o) do
                if t['@'][var] == nil then tinsert(t['@'], var) end
                t['@'][var] = val
            end
            return o
        end
        local vals = {}
        for _, t in ipairs(o) do tinsert(vals, t['@'][var]) end
        return vals
    end; -- }}}

    text = function (o, txt)
        if type(o[1]) == 'table' then tinsert(o[1], txt) end
        return o
    end;

    style = function (o, var, val) -- TODO
        return o
    end;

    filter = function (o, func) -- TODO
        return o
    end;

    map = function (o, func) -- TODO
        return o
    end;

    insert = function (o, ele, i) -- {{{ also append
        if type(o[1]) == 'table' then
            tinsert(o[1], ((tonumber(i) or 0) -1) % (#(o[1]) + 1) + 1, {['.'] = ele})
        end
        return o
    end; -- }}}

    enter = function (o) -- TODO
        return o
    end;

    data = function (o, data) -- attach
        o['+'] = data
        return o
    end;

    exit = function (o) -- TODO
        return o
    end;
    -- }}}
}

local buildxlink = function () -- xlink -- xlink/xpointer based on root {{{
    for _, xml in ipairs(docs) do if xml.parse then xml:parse() end end
    local stamp = math.random()
    local stack = {}
    local function traceTbl (doc, xml) -- {{{ lua table form
        local href = doc['@'] and doc['@']['xlink:href']
        if href then -- print(xml..'<'..doc['.']..'>'..href)
            tinsert(stack, xml..'<'..doc['.']..'>'..href)
            if stack[href] then
                href = 'loop '..tconcat(stack, '->')
                tremove(stack)
                if not docs[xml]['?'] then docs[xml]['?'] = {} end
                return tinsert(docs[xml]['?'], href)
            end
            stack[href] = true

            if (not doc['&']) or (doc['&']['#'] ~= stamp) then -- attr
                local link, xpath = strmatch(href, '^([^#]*)(.*)') -- {{{ file_link, tag_path
                if link == '' then -- back to this doc root
                    link = xml
                else -- new file
                    if strsub(link, 1, 1) ~= '/' then
                        link = strgsub(type(xml) == 'string' and xml or '', '[^/]*$', '')..link
                    end
                    link = we.normpath(link)
                end -- }}}

                if (type(link) == 'string') and not docs[link] then docs[link] = dom(link) end
                if xml ~= link then traceTbl(docs[link], link) end
                link, xpath = xPath(o, strmatch(xpath or '', '#xpointer%((.*)%)'), docs[link])

                if #link == 0 then -- error message
                    href ='broken <'..doc['.']..'> '..xpath
                    if not docs[xml]['?'] then docs[xml]['?'] = {} end
                    tinsert(docs[xml]['?'], href)
                    doc['&'] = nil
                else
                    doc['&'] = link -- xlink
                    link['#'] = stamp
                    -- print(doc['@']['xlink:href'], 'linked', #link)
                end
            end

            stack[href] = nil
            tremove(stack) -- #stack
        end
        for i = 1, #doc do -- continous override
            if type(doc[i]) == 'table' and doc[i]['.'] then traceTbl(doc[i], xml) end
        end
    end -- }}}
    for xml, o in pairs(docs) do if not o['?'] then traceTbl(o, xml) end end
    -- for xml, o in pairs(docs) do print(xml, o['?'] and tconcat(o['?'], '\n')) end
end; -- }}}

local lom = {
    docs = docs; -- doctree for files, user's management
    lapi = class.static[dom].__index; -- extension: api.style = function ... end
}

setmetatable(lom, {
    __metatable = true;
    __call = function (c, spec) -- {{{ dom object creator
        if type(spec) == 'string' then -- '' for incremental text
            spec = we.normpath(spec)
            if docs[spec] then return docs[spec] end
        elseif spec and type(spec) ~= 'table' then -- closing
            return buildxlink()
        end
        return dom(spec)
    end; -- }}}
})

-- ================================================================== --
-- service for checking object model and demo/debug -- {{{
if arg and #arg > 0 and strfind(arg[0], 'lom.lua$') then
    local doc = lom(arg[1] == '-' and '' or arg[1])
    if arg[1] == '-' then doc:parse(io.stdin:read('a')):parse() end
    lom(true)
    print(doc['?'] and tconcat(doc['?'], '\n') or doc:dump())
end -- }}}

return lom
-- vim:ts=4:sw=4:sts=4:et:fen:fdm=marker:fmr={{{,}}}:fdl=1
