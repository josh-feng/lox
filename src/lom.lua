#!/usr/bin/env lua
-- ================================================================== --
-- LOM (Lua Object Model)              Josh Feng (C) MIT license 2022 --
-- DOM: doc = {['.'] = tag; ['@'] = {}; ['&'] = {{}..}; '\0comment', ...}
-- Usage example:
--      lom = require('lom')
--      doc1 = lom(xmlfilepath)
--      doc2 = lom('') ; doc2:parse(txt):parse()
--      lom(true) -- buildxlink
--      xmltxt = doc2:drop(1)
-- ================================================================== --
local class = require('pool') -- https://github.com/josh-feng/pool.git
local we = require('us') -- working environment

local type = type
local strlen, strmatch, strgmatch = string.len, string.match, string.gmatch
local strsub, strgsub, strfind = string.sub, string.gsub, string.find
local tinsert, tremove, tconcat = table.insert, table.remove, table.concat
local mmin = math.min
-- ================================================================== --
local docs = {} -- {{{ doctree for files, user's management
local singleton = {}
local mp = require('lsmp') -- a simple/sloppy SAX to replace lxp

local function lsmp2domAttr (attr) -- {{{ parse lsmp attr table to dom attr table
    local k, v
    for i = 1, #attr do
        local key, eq, val = strmatch(attr[i], "^([^=]*)(=?)(.*)$")
        if eq == '' then -- key
            if v then
                attr[k] = we.trimq(key)
                k = false
            else
                if k then attr[k] = true end
                k = key
            end
            v = false
        elseif key == '' and val == '' then -- =
            v = k and true
        elseif key == '' then -- =val
            if k then attr[k] = we.trimq(val) ; k = false
            else k = we.trimq(val) end
            v = false
        else
            if k then attr[k] = v and '' or true end
            if val ~= '' then -- key=val
                attr[key] = we.trimq(val)
                k = false
                v = false
            else -- key=
                k = key
                v = true
            end
        end
        attr[i] = nil -- clean
    end
    if k then attr[k] = v and '' or true end
    return attr
end -- }}}

local function scheme (p, name, attr) -- {{{ definition/declaration
    local stack = p:getcallbacks().stack
    stack[#stack]['+'] = stack[#stack]['+'] or {}
    attr[0] = name
    tinsert(stack[#stack]['+'], attr)
end -- }}}
local function starttag (p, name, attr) -- {{{
    local stack = p:getcallbacks().stack
    tinsert(singleton[name] and stack[#stack] or stack,
        {['.'] = name, ['@'] = #attr > 0 and lsmp2domAttr(attr) or nil})
end -- }}}
local function endtag (p, name) -- {{{
    if singleton[name] then return end
    local stack = p:getcallbacks().stack
    if #stack > 1 then -- {o}
        local element = tremove(stack)
        tinsert(stack[#stack], element)
    end
end -- }}}
local function cleantext (p, txt) -- {{{
    txt = strgsub(txt, '&nbsp;', '')
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
    tinsert(stack[#stack], '\0'..txt)
end -- }}}
local function extension (p, name, txt) -- {{{
    local stack = p:getcallbacks().stack
    tinsert(stack[#stack], '\0'..name..'\0'..txt)
end -- }}}
local function closing (p) -- {{{
    local stack = p:getcallbacks().stack
    while #stack > 1 do -- closing unmatched tags
        local element = tremove(stack)
        tinsert(stack[#stack], element)
    end
end -- }}}

local function parse (o, txt) -- friend function {{{
    local p = o[0]
    -- local status, msg, line, col, pos = p:parse(txt) -- pass nil if failed
    local status, msg, line = p:parse(txt) -- pass nil if failed
    if not (txt and status) then
        if not status then o['?'] = {msg..' #'..line} end
        p:close() -- mp obj is destroyed
        o[0] = nil
        o.parse = nil
    end
    return o -- for cascade oop
end --}}}
-- ================================================================== --
local function xPath (c, paths, doc, conti) -- {{{ return doc/xml-node table, ending index
    -- print('=>', c, '('..(paths[2 * c - 1] or '')..')', we.var2str(doc), we.var2str(conti))
    local path = paths[2 * c - 1]
    if (not path) or path == '/' or path == '' or #doc == 0 then
        if conti and #conti > 0 then
            for _, v in ipairs(xPath(1, paths, conti)) do tinsert(doc, v) end
        end
        return doc, c
    end
    -- xpath syntax: NB: xpointer does not have standard treatment
    -- /A/B[@attr="val",@bb='4']
    -- anywhere/A/B[-3]/-2/3
    local tag = (strsub(path, 1, 1) ~= '/') and path or strsub(path, 2, #path)
    path = paths[2 * c]

    local idx = tonumber(tag)
    if idx then return xPath(c + 1, paths, {doc[(idx - 1) % #doc + 1]}, conti) end

    local autopass = true
    if path then
        for k, v in pairs(path) do
            if not autopass then break end
            if k ~= 0 then autopass = (type(k) == 'number') and (type(v) == 'number') end
        end
    end

    local xn = {} -- xml-node (doc)
    local docl = (not paths.invidual) and doc['&'] -- follow xpointer or not
    local docn = 0
    repeat
        for i = 1, #doc do
            local mt = doc[i]
            if type(mt) == 'table' then
                if mt['.'] == tag and (autopass or we.match(mt['@'], path)) then
                    tinsert(xn, mt)
                    if c == paths.remove then doc[i] = '\0'..'nil' end -- set as a comment
                elseif strsub(paths[1], 1, 1) ~= '/' then -- start from anywhere?
                    conti = conti or {} -- reset to 1 to continue
                    if c == 1 then
                        for _ = 1, #mt do tinsert(conti, mt[_]) end
                    else
                        tinsert(conti, mt)
                    end
                end
            end
        end
        docn = docn + 1
        doc = docl and docl[docn]
    until not doc

    if path and #path > 0 and #xn > 0 then -- collect the indixed table
        local nxn = {}
        for i = 1, #path do
            if type(path[i]) == 'number' then
                tinsert(nxn, xn[(path[i] - 1) % #xn + 1])
            end
        end
        if #nxn ~= 0 then xn = nxn end -- collected
    end
    -- not final: break to further search
    if #xn > 0 and (2 * c) ~= #paths and tag ~= '' then
        local nxn = {}
        for i = 1, #xn do
            local mt = xn[i]
            for j = 1, #mt do tinsert(nxn, mt[j]) end
        end
        xn = nxn
    end
    return xPath(c + 1, paths, xn, conti)
end -- }}}

local function procXpath (path) -- {{{ XPath language parser
    -- xpath: path0/path1/path3[att1,att2=txt,attr3='txt']
    local t = {}
    repeat
        local elem, attr
        elem, path = strmatch(path, '^(/?[%w_:]*)(.*)$')
        tinsert(t, elem)

        if strsub(path, 1, 1) == '[' then -- heuristic xpath parser
            elem = {}
            while strsub(path, 1, 1) ~= ']' do
                attr, path = strmatch(strsub(path, 2, #path), '^([_%w]*)(.*)$')
                local c = strsub(path, 1, 1)
                if tonumber(attr) then
                    tinsert(elem, tonumber(attr))
                elseif c == ',' then
                    path = strsub(path, 2, #path)
                    elem[attr] = true
                elseif c == '=' then -- '/" ...
                    c = strsub(path, 2, 2)
                    if c == '"' or c == "'" then
                        c, path = strmatch(strsub(path, 3, #path), '^([^'..c..']*)'..c..'(.*)$')
                        if not c then error('Wrong attr quote', 2) end
                        elem[attr] = c
                    else
                        c, path = strmatch(strsub(path, 2, #path), '^([^,%]]*)(.*)$')
                        elem[attr] = c
                    end
                elseif c ~= ']' then
                    error('Wrong attr setting: '..attr..':'..path, 2)
                end
                if #path < 1 then error('Wrong attr setting', 2) end
            end
            path = strsub(path, 2, #path)
        else
            elem = false
        end

        tinsert(t, elem) -- {elem, false; elem, {attr1, ...}; ...}
    until path == ''
    -- print('path =', we.var2str(t)) -- debug
    return t
end -- }}}
-- ================================================================== --
local function xmlstr (s, fenc) -- {{{ enc: gzip -c | base64 -w 128 / dec: base64 -i -d | zcat -f
    s = tostring(s)
    if strfind(s, '\n') or (strlen(s) > 1024) then -- large text
        if fenc or strfind(s, ']]>') then -- enc flag or hostile strings
            -- local status, stdout, stderr = we.popen(s, 'gzip -c | base64 -w 128')
            local status, stdout = we.popen(s, 'gzip -c | base64 -w 128')
            return status and '<!-- base64 -i -d | zcat -f -->{{{'..stdout..'}}}' or ''
        else
            return (strfind(s, '&') or strfind(s, '<') or strfind(s, '>'))
                and '<![CDATA[\n'..s..']]>' or s
        end
    else -- escape characters
        return strgsub(strgsub(strgsub(strgsub(strgsub(s,
            '&', '&amp;'), '"', '&quot;'), "'", '&apos;'), '<', '&lt;'), '>', '&gt;')
    end
end -- }}}

local function wXml (node) -- {{{
    if 'string' == type(node) then
        -- TODO extension <?php ?> <%= %> etc
        return strsub(node, 1, 1) ~= '\0' and node or '<!--'..node..'-->'
    end
    local res = {}
    if node['@'] then
        for k, v in pairs(node['@']) do
            tinsert(res, k..((type(v) == 'string')
            and '="'..strgsub(v, '"', '\\"')..'"' or ''))
        end
    end
    res = '<'..node['.']..(#res > 0 and ' '..tconcat(res, ' ') or '')
    if #node == 0 then return res..' />' end
    res = {res..'>'}
    for i = 1, #node do
        tinsert(res, type(node[i]) == 'table' and wXml(node[i]) or xmlstr(node[i]))
    end
    if #res == 2 and #(res[2]) < 100 and not strfind(res[2], '\n') then
        return res[1]..res[2]..'</'..node['.']..'>'
    end
    return strgsub(tconcat(res, '\n'), '\n', '\n  ')..'\n</'..node['.']..'>' -- indent 2
end -- }}}

local dom = class { -- lua document object model {{{
    ['.'] = false; -- tag name
    ['@'] = false; -- attr
    ['&'] = false; -- xlink table
    ['?'] = false; -- errors
    ['*'] = false; -- module
    ['+'] = false; -- misc info (definition/declaration)

    ['<'] = function (o, spec, mode) --{{{

        mode = tonumber(mode) or 0x0f
        -- 0x40 extension: <?php ?> <%= %>
        -- 0x20 keep comment
        -- 0x10 scheme
        -- 0x08 trim text (default)
        -- 0x04 mp: TODO (default)
        -- 0x02 mp: sloppy <_ _> (default)
        -- 0x01 mp: escape \' \" \< \> (default)

        if type(spec) == 'table' then -- partial table-tree (0: data/stamp)
            for k, v in pairs(spec) do
                if type(k) == 'number' or o[k] ~= nil then o[k] = v end -- new data setting
            end
        elseif type(spec) == 'string' then -- '' for text

            local p = mp.new {
                Scheme = (mode & 0x10 > 0) and scheme or nil,
                StartElement = starttag,
                EndElement = endtag,
                CharacterData = (mode & 0x08 > 0) and cleantext or text,
                Comment = (mode & 0x20 > 0) and comment or nil,
                Extension = (mode & 0x40 > 0) and extension or nil,
                Closing = closing,
                mode = mode,
                ext = '<?php ?> <%= %>', -- weird stuff
                stack = {o} -- {{}}
            }

            if spec == '' then
                o[0] = p
                o.parse = parse
            else
                local file, msg, status, line
                file, msg = io.open(spec, 'r')
                if file then -- clean up xml/html mess
                    msg = file:read('*all')
                    file:close()
                    -- local status, msg, line, col, pos = p:parse(msg)
                    status, msg, line = p:parse(msg)
                    if status then status, msg, line = p:parse() end
                    if not status then o['?'] = {msg..' #'..line} else p:close() end
                else
                    o['?'] = {msg}
                end
            end
            if spec == '' then tinsert(docs, o) else docs[spec] = o end
        end
    end; --}}}

    ['>'] = function (o) for i = 0, #o do o[i] = nil end end;

    ['^'] = {
        __call = function (o) return setmetatable(we.dup(o), getmetatable(o)) end;
    };

    parse = false; -- implemented in friend function

    xpath = function (o, path, doc)
        return (xPath(1, procXpath(path), doc or o)) -- only first
    end;

    -- output
    drop = function (o, fxml) -- {{{ drop fxml=1/html
        if not fxml then return we.var2str(o) end
        local res = {fxml == 1 and '<?xml version="1.0" encoding="UTF-8"?>' or nil}
        local docl = o['&']
        local docn = 0
        repeat
            for j = 1, #o do tinsert(res, wXml(o[j])) end
            docn = docn + 1
            o = docl and docl[docn]
        until not o
        return tconcat(res, '\n')
    end;-- }}}

    -- member functions supporting cascade oo style
    select = function (o, path)
        path = procXpath(path)
        path.invidual = true
        return class:new(o, (xPath(1, path, o)))
    end;

    remove = function (o, path)
        path = procXpath(path)
        path.invidual = true
        -- path.remove = #path >> 1
        path.remove = math.floor(#path / 2)
        return o, xPath(1, path, o) -- if the removed is needed
    end;
} -- }}}
-- }}}

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

            if (not doc['&']) or (doc['&'][0] ~= stamp) then -- attr
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
                local paths = procXpath(strmatch(xpath or '', '#xpointer%((.*)%)'))
                link, xpath = xPath(1, paths, docs[link])

                if #link == 0 then -- error message
                    href ='broken path <'..doc['.']..'> '..tostring(xpath)
                    if not docs[xml]['?'] then docs[xml]['?'] = {} end
                    tinsert(docs[xml]['?'], href)
                    doc['&'] = nil
                else
                    doc['&'] = link -- xlink
                    link[0] = stamp
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
end; -- }}}

local lom = setmetatable({}, {
    __metatable = true;
    __tostring = function () return we.tbl2str(docs, '\n') end;
    __call = function (c, spec, mode) -- {{{ dom object creator
        if type(spec) == 'string' then -- '' for incremental text
            spec = we.normpath(spec)
            if c.doc[spec] then return c.doc[spec] end
        elseif type(spec) ~= 'table' then -- closing
            return buildxlink() -- error message table
        end
        return dom(spec, mode)
    end; -- }}}
    __index = {
        doc = docs;
        singleton = function (str)
            for k in pairs(singleton) do singleton[k] = nil end -- reset
            for k in strgmatch(str, '%S+') do singleton[k] = true end
        end;
    }
})

lom.singleton('area base br col command embed hr img input keygen link meta param source track wbr')
-- ================================================================== --
lom.api = class[dom].__index; -- dom's member function extension

lom.api.data = function (o, data) -- attach {{{
    o[0] = type(data) == 'table' and data or {data}
    return o
end -- }}}

local function setAttr (t, var, val) -- {{{
    t['@'] = t['@'] or {}
    if t['@'][var] == nil then tinsert(t['@'], var) end
    t['@'][var] = val
end -- }}}

lom.api.attr = function (o, var, val) -- {{{ get/set
    if val then -- cascade coding
        local c = type(val) == 'function' and type(o[0]) == 'table'
        for i = 1, c and mmin(#o, #(o[0])) or #o do
            setAttr(o[i], var, c and val(o[0], i) or val)
        end
        return o
    end
    local vals = {}
    for _, t in ipairs(o) do
        tinsert(vals, t['@'] and t['@'][var] or false)
    end
    return vals
end -- }}}

lom.api.map = function (o, func) -- map data and process nodes {{{
    if o[0] and type(func) == 'function' then
        for i = 1, mmin(#o, #(o[0])) do
            o[0][i] = func(o[0], i, o) -- pass o as well
        end
    end
    return o
end -- }}}

lom.api.text = function (o, txt) -- {{{
    for i = 1, #o do
        if type(o[i]) == 'table' then tinsert(o[i], txt) end
    end
    return o
end -- }}}

lom.api.arrange = function (o, ele, i) -- {{{ also remove/append TODO
    if type(o[1]) == 'table' then
        tinsert(o[1], ((tonumber(i) or 0) -1) % (#(o[1]) + 1) + 1, {['.'] = ele})
    end
    return o
end -- }}}

-- ================================================================== --
-- service for checking object model and demo/debug -- {{{
if arg and #arg > 0 and strfind(arg[0] or '', 'lom.lua$') then
    local doc = lom(arg[1] == '-' and '' or arg[1], 0xff)
    if arg[1] == '-' then doc:parse(io.stdin:read('a')):parse() end
    -- lom(true)
    print(doc['?'] and tconcat(doc['?'], '\n') or doc:drop())
end -- }}}

return lom
-- vim:ts=4:sw=4:sts=4:et:fdm=marker:fdl=1:sbr=-->
