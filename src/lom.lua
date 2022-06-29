#!/usr/bin/env lua
-- ================================================================== --
-- LOM (Lua Object Model): on top of 'lxp'
-- DOM: doc = {['.'] = tag; ['@'] = {}; ['&'] = {{}..}; '\0comment', ...}
-- Usage example:
--      lom = require('lom')
--      doc = lom(xmlfile or '')
--      doc = lom('') doc:parse(txt):parse()
--      lom(true) -- buildxlink
--      xmltxt = doc:drop(1)
-- ================================================================== --
local lxp = require('lxp') -- the standard Lua Expat module
local class = require('pool') -- https://github.com/josh-feng/pool.git
local we = require('us') -- working environment

local next, assert, type = next, assert, type
local strlen, strsub, strmatch, strgmatch = string.len, string.sub, string.match, string.gmatch
local strrep, strgsub, strfind = string.rep, string.gsub, string.find
local tinsert, tremove, tconcat = table.insert, table.remove, table.concat
local mmin = math.min
-- ================================================================== --
local lom = {doc = {}} -- doctree for files, user's management {{{

local docs = lom.doc -- xml object list (hidden upvalue)

-- html no end tag
local singleton = 'area base br col command embed hr img input keygen link meta param source track wbr '

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
    tinsert(stack[#stack], '\0'..txt)
end -- }}}

local function parse (o, txt) -- friend function {{{
    local p = o[0]
    local status, msg, line, col, pos = p:parse(txt) -- pass nil if failed
    if not (txt and status) then
        if not status then o['?'] = {msg..' #'..line} end
        p:close() -- seems destroy the lxp obj
        o[0] = nil
        o.parse = nil
    end
    return o -- for cascade oop
end --}}}
-- ================================================================== --
local function xPath (c, paths, doc) -- {{{ return doc/xml-node table, ending index
    local path = paths[c]
    if (not path) or path[0] == '/' or path[0] == '' or #doc == 0 then return doc, c end
    -- xpath syntax: NB: xpointer does not have standard treatment
    -- /A/B[@attr="val",@bb='4']
    -- anywhere/A/B[-3]/-2/3
    local anywhere = strsub(path[0], 1, 1) ~= '/'
    local tag = anywhere and path[0] or strsub(path[0], 2, #path[0])

    local idx = tonumber(path)
    if idx then return xPath(c + 1, paths, {doc[(idx - 1) % #doc + 1]}) end

    local autopass = true
    for i = 1, #path do
        if autopass then autopass = (type(path[i]) == 'number') else break end
    end

    local xn = {} -- xml-node (doc)
    local docl = doc['&']
    local docn = docl and 0
    repeat
        for i = 1, #doc do
            local mt = doc[i]
            if type(mt) == 'table' then
                if mt['.'] == tag and (autopass or we.match(mt['@'], path)) then
                    tinsert(xn, mt)
                elseif anywhere and (#mt > 0 or mt['&']) then
                    local mtl = mt['&']
                    local mtn = mtl and 0
                    repeat
                        local sub = xPath(c, paths, mt)
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

    if #path > 0 and #xn > 0 then -- collect the indixed table
        local nxn = {}
        for i = 1, #path do
            if type(path[i]) == 'number' then
                tinsert(nxn, xn[(path[i] - 1) % #xn + 1])
            end
        end
        if #nxn ~= 0 then xn = nxn end -- collected
    end
    -- not final: break to further search
    if #xn > 0 and c ~= #paths and path ~= '' then
        local nxn = {}
        for i = 1, #xn do
            local mt = xn[i]
            for j = 1, #mt do tinsert(nxn, mt[j]) end
        end
        xn = nxn
    end
    return xPath(c + 1, paths, xn)
end -- }}}

local function procXpath (path) -- {{{ XPath language parser
    -- xpath: path0/path1/path3[att1,att2=txt,attr3='txt']
    local t = {}
    repeat
        local elem, attr
        elem, path = strmatch(path, '^(/?[%w_:]*)(.*)$')
        elem = {[0] = elem}
        if strsub(path, 1, 1) == '[' then
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
        end
        tinsert(t, elem)
    until path == ''
    return t
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

local function wXml (node) -- {{{
    if 'string' == type(node) then
        return strsub(node, 1, 1) ~= '\0' and node or '<!--'..node..'-->'
    end
    local res = {}
    if node['@'] then
        for _, k in ipairs(node['@']) do tinsert(res, k..'="'..strgsub(node['@'][k], '"', '\\"')..'"') end
    end
    res = '<'..node['.']..(#res > 0 and ' '..tconcat(res, ' ') or '')
    if #node == 0 then return res..' />' end
    res = {res..'>'}
    for i = 1, #node do tinsert(res, type(node[i]) == 'table' and wXml(node[i]) or xmlstr(node[i])) end
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

    ['<'] = function (o, spec, mode) --{{{
        if type(spec) == 'table' then -- partial table-tree (0: data/stamp)
            for k, v in pairs(spec) do
                if k ~= 0 then o[k] = v end -- new data setting
            end
        elseif type(spec) == 'string' then -- '' for text
            mode = tonumber(mode) or 0
            local p = lxp.new {
                StartElement = starttag,
                EndElement = endtag,
                CharacterData = mode < 1 and text or cleantext,
                Comment = mode < 1 and comment or nil,
                _nonstrict = true,
                stack = {o} -- {{}}
            }

            if spec == '' then
                o[0] = p
                o.parse = parse
            else
                local file, msg = io.open(spec, 'r')
                if file then -- clean up xml/html mess
                    msg = strgsub(file:read('*all'), '&nbsp;', '')
                    file:close()
                    msg = strgsub(strgsub(msg, '<script[^>]*>%s*</script>', ''),
                        '<script([^>]*)>(.-)</script>', '<script%1><![CDATA[%2]]></script>')
                    if mode > 0 then -- html replay no-end tag with />
                        for st in strgmatch(singleton, '(%S+) ') do
                            msg = strgsub(msg, '<('..st..'[^>]-)/?>', '<%1 />')
                        end
                    end
                    local status, msg, line, col, pos = p:parse(msg)
                    if status then status, msg, line = p:parse() end
                    if not status then o['?'] = {msg..' #'..line} else p:close() end
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

    ['>'] = function (o) for i = 0, #o do o[i] = nil end end;

    parse = false; -- implemented in friend function

    xpath = function (o, path, doc)
        return (xPath(1, procXpath(path), doc or o)) -- only first
    end;

    -- output
    drop = function (o, fxml) -- {{{ drop fxml=1/html
        if not fxml then return we.var2str(o) end
        local res = {fxml == 1 and '<?xml version="1.0" encoding="UTF-8"?>' or nil}
        local docl = o['&']
        local docn = docl and 0
        repeat
            for j = 1, #o do tinsert(res, wXml(o[j])) end
            if docn then docn = docn < #docl and docn + 1 end
            o = docn and docl[docn]
        until not o
        return tconcat(res, '\n')
    end;-- }}}

    -- member functions supporting cascade oo style
    select = function (o, path)
        return class:new(o, o:xpath(path))
    end;
} -- }}}

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

setmetatable(lom, {
    __metatable = true;
    __tostring = function (c) return we.tbl2str(lom.doc, '\n') end;
    __call = function (c, spec, mode) -- {{{ dom object creator
        if type(spec) == 'string' then -- '' for incremental text
            spec = we.normpath(spec)
            if docs[spec] then return docs[spec] end
        elseif spec and type(spec) ~= 'table' then -- closing
            return buildxlink() -- TODO error message
        end
        return dom(spec, mode)
    end; -- }}}
})
-- }}}
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

lom.api.filter = function (o, val) -- TODO adjust
    return o
end

lom.api.enter = function (o) -- TODO
    return o
end

lom.api.exit = function (o) -- TODO
    return o
end

-- ================================================================== --
-- service for checking object model and demo/debug -- {{{
if arg and #arg > 0 and strfind(arg[0] or '', 'lom.lua$') then
    local doc = lom(arg[1] == '-' and '' or arg[1], arg[1] and strfind(arg[1], '%.html$'))
    if arg[1] == '-' then doc:parse(io.stdin:read('a')):parse() end
    lom(true)
    print(doc['?'] and tconcat(doc['?'], '\n') or doc:drop())
end -- }}}

return lom
-- vim:ts=4:sw=4:sts=4:et:fen:fdm=marker:fmr={{{,}}}:fdl=1
