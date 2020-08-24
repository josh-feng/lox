#!/usr/bin/env lua
-- ======================================================================== --
-- XML Lua Object Model (LOM)
-- doc = {['.'] = tag, ['@'] = attrOrder, ...} -- comment = '\0' .. string
-- ref: lua element tree http://etree.luaforge.net/
local lxp = require 'lxp'  -- the standard Lua Expat module

local class = require 'pool'
local btx = require 'lox.btx' -- basic utility toolbox

local next, assert, type = next, assert, type
local strlen, strsub, strmatch, strgmatch = string.len, string.sub, string.match, string.gmatch
local strrep, strgsub, strfind = string.rep, string.gsub, string.find
local tinsert, tremove, tconcat = table.insert, table.remove, table.concat

local Lox = class { -- {{{ Lua Objective Xml
    doc = false; -- node

    --
    filter = function (o) end;
    map = function (o) end;

    --
    getElementById = function (o, id)
    end;

    textContent = function (o, txt)
        for i = 1, #o.doc do
            for j = 1, #(o.doc[i]) do o.doc[i][j] = nil end
            tinsert(o.doc[i], txt)
        end
        return o
    end;

    append = function (o, tag) -- append a tag
        local node = {['.'] = tag}
        tinsert(o.doc, node)
        return node;
    end;

    selectAll = function (o, tag) -- return array

    end;

    select = function (o, tag) -- return first found

    end;

    attr = function (o, key, val)
        if val == nil then return o.doc[key] end
        o.doc[key] = val
        return o
    end;

    dump = function (o, indent) -- {{{ -0/none, +/indent

        local xmlstr = function (s, fenc) -- {{{
            -- encode: gzip -c | base64 -w 128
            -- decode: base64 -i -d | zcat -f
            -- return '<!-- base64 -i -d | zcat -f -->{{{'..
            --     tun.popen(s, 'tun.gzip -c | base64 -w 128'):read('*all')..'}}}'
            s = tostring(s)
            if strfind(s, '\n') or (strlen(s) > 1024) then -- large text
                if fenc or strfind(s, ']]>') then -- enc flag or hostile strings
                    local status, stdout, stderr = tun.popen(s, 'gzip -c | base64 -w 128')
                    return '<!-- base64 -i -d | zcat -f -->{{{'..stdout..'}}}'
                else
                    -- return (strfind(s, '"') or strfind(s, "'") or strfind(s, '&') or
                    --         strfind(s, '<') or strfind(s, '>')) and '<![CDATA[\n'..s..']]>' or s
                    return (strfind(s, '&') or strfind(s, '<') or strfind(s, '>')) and '<![CDATA[\n'..s..']]>' or s
                end
            else -- escape characters
                return strgsub(strgsub(strgsub(strgsub(strgsub(s,
                    '&', '&amp;'), '"', '&quot;'), "'", '&apos;'), '<', '&lt;'), '>', '&gt;')
            end
        end -- }}}

        local function dumpLox (node) -- {{{ DOM: tbm = {['.'] = tag; ['@'] = attrOrder; ...}
            if 'string' == type(node) then
                if strfind(node, '^\0') then return '<!--'..strsub(node, 2)..'-->' end
                return xmlstr(node)
            end
            local res = {}
            if node['@'] then
                for k in strgmatch(node['@'], '%S+') do tinsert(res, k..'="'..strgsub(node[k], '"', '\\"')..'"') end
            end
            res = '<'..node['.']..(#res > 0 and ' '..tconcat(res, ' ') or '')
            if #node == 0 then return res..' />' end
            res = {res..'>'}
            for i = 1, #node do tinsert(res, dumpLox(node[i]) or nil) end
            if #res == 2 and #(res[2]) < 100 and not strfind(res[2], '\n') then
                return res[1]..res[2]..'</'..node['.']..'>'
            end
            return indent
                and strgsub(tconcat(res, '\n'), '\n', '\n'..indent)..'\n</'..node['.']..'>'
                or tconcat(res, '')..'</'..node['.']..'>'
        end -- }}}

        indent = tonumber(indent) or 2 -- 0: no indentation
        indent = indent > 0 and strrep(' ', indent)
        local res = {}
        for _, doc in ipairs(o.doc) do tinsert(res, dumpLox(doc) or nil) end
        return (fxml == 1 and '' or '<?xml version="1.0" encoding="UTF-8"?>\n')..tconcat(res, '\n')
    end; -- }}}

    xpath = function (o, doc, path) -- {{{ return doc/xml-node table, missingTag
        if (not path) or path == '' or #doc == 0 then return doc, path end
        -- NB: xpointer does not have standard treatment -- A/B, /A/B[@attr="val",@bb='4']

        local tag, attr
        tag, path = strmatch(path, '([^/]+)(.*)$')
        tag, attr = strmatch(tag, '([^%[]+)%[?([^%]]*)')
        attr = str2tbl(attr)
        local idx = tonumber(attr[#attr]) -- idx: []/all, [-]/last, [0]/merged, [+]/first
        local xn = {} -- xml-node (doc)
        repeat -- collect along the metatable (if mode is defined)
            for i = 1, #doc do -- no metatable
                local mt = doc[i]
                if type(mt) == 'table' and mt['.'] == tag and btx.matchtbl(mt, attr) then
                    if idx and idx < 0 then xn[1] = nil end -- clean up
                    if path ~= '' or idx == 0 then
                        repeat -- collect along the metatable (NB: ipairs will dupe metatable)
                            for j = 1, #mt do if type(mt[j]) == 'table' or path == '' then tinsert(xn, mt[j]) end end
                            mt = getmetatable(mt)
                            if mt then mt = mt.__index end
                        until not mt
                    else
                        tinsert(xn, mt)
                    end
                    if idx and idx > 0 then break end
                end
            end
            if idx and idx > 0 and #xn > 0 then break end
            doc = getmetatable(doc)
            if doc then doc = doc.index end
        until not doc
        if path == '' and idx == 0 then xn['.'] = tag; xn = {xn} end
        return o:xpath(xn, path)
    end; -- }}}

    ['<'] = function (o, assign, trim) -- {{{
        if type(assign) == 'table' then o.doc = assign; return end
        if type(assign) ~= 'string' then error('Not supported format', 2) end
        trim = tonumber(trim) or 0 -- trim -/intact, 0/end-space, +/blank
        local doctree = {} -- doctree --> topxml

        local ParseStr = function (txt) -- {{{
            local node = {} -- working variable: root node (node == token == tag == table)

            local lomcallbacks = {
                StartElement = function (parser, name, attr) -- {{{
                    attr['.'] = node;
                    if #attr > 0 then
                        attr['@'] = tconcat(attr, ' ')
                        for i = 1, #attr do attr[i] = nil; end
                    end
                    tinsert(node, attr)
                    node = attr
                end; -- }}}
                EndElement = function (parser, name) node, node['.'] =  node['.'], name end;
                CharacterData = function (parser, s) -- {{{
                    if trim < 0 then
                        tinsert(node, s)
                    elseif strmatch(s, '%S') then
                        s = strmatch(strgsub(s, '%s*\n', '\n'), '^\n*(.-)\n*$')
                        tinsert(node, trim > 0 and strmatch(s, '(%S.-)%s*$') or s)
                    end
                end; -- }}}
                Comment = function (parser, s)
                    if strmatch(s, '%S') and trim <= 0 then tinsert(node, '\0'..s) end
                end;
            }

            local plom = lxp.new(lomcallbacks)
            local fmt = strfind(txt, '%s*<?xml')
            if not fmt then plom:parse('<?xml version="1.0" encoding="utf-8"?><!DOCTYPE html>') end
            local status, msg, line, col, pos = plom:parse(txt) -- passed nil if failed
            plom:parse()
            plom:close() -- seems destroy the lxp obj
            node['?'] = status and {} or {msg..' #'..line}
            return node
        end -- }}}

        local ParseXml = function (xmlfile) -- {{{ lua table form
            if not doctree[xmlfile] then
                local file, msg = io.open(xmlfile, 'r')
                if file then
                    doctree[xmlfile] = ParseStr(file:read('*all'))
                    file:close()
                else
                    doctree[xmlfile] = {['?'] = {msg}}
                end
            end
            return doctree[xmlfile]
        end; -- }}}

        local function buildLnk (xn, xml) -- {{{ lua table form
            local v = xn['@'] and xn['xlink:href']
            if v then -- attr

                local link, xpath = strmatch(v, '^([^#]*)(.*)') -- {{{ file_link, tag_path
                if link == '' then -- back to this root node
                    link = xml
                else -- new file
                    if strsub(link, 1, 1) ~= '/' then link = strgsub(xml, '[^/]*$', '')..link end
                    link = btx.normpath(link)
                end -- }}}

                if not doctree[link] then buildLnk(ParseXml(link), link) end
                link, xpath = o:xpath(doctree[link], strmatch(xpath or '', '#xpointer%((.*)%)'))

                if #link == 1 then -- the linked table
                    local meta = link[1]
                    repeat -- loop detect {{{
                        meta = getmetatable(meta) and getmetatable(meta).__index
                        if meta == xn then break end
                    until not meta -- }}}
                    if meta then
                        tinsert(doctree[xml]['?'], 'loop '.. v) -- error message
                    elseif xn ~= link[1] then
                        setmetatable(xn, {__index = link[1]})
                        buildLnk(link[1], xml)
                    end
                else -- error msg
                    tinsert(doctree[xml]['?'], 'broken <'..xn['.']..'> '..xpath..':'..#link..':'..v)
                end
            end
            for i = 1, #xn do if type(xn[i]) == 'table' then buildLnk(xn[i], xml) end end -- continous override
        end -- }}}

        local topxml = strfind(assign, '<') and '' or assign
        doctree[topxml] = topxml == '' and ParseStr(assign) or ParseXml(btx.normpath(topxml))
        if #(doctree[topxml]['?']) == 0 then buildLnk(doctree[topxml], topxml) end -- no error msg
        o.doc = doctree[topxml]
        o.doc['*'] = doctree -- all files involded (doctree)
    end; -- }}}
} -- }}}

-- local o = Lox('file.html')
-- o:attr('', ''):style('', '')
-- o:xpath() -- select
-- o:select():attr():attr():style -- select

if arg and #arg > 0 then print(Lox(arg[1]):dump()) end -- service

return Lox
-- vim: ts=4 sw=4 sts=4 et foldenable fdm=marker fmr={{{,}}} fdl=1
