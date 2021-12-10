#!/usr/bin/env lua
-- ======================================================================== --
local class = require("pool")
local lom = require("lom")
local tun = require("util")

-- object corresponding to a xml tag w/ a class attribute
--
-- this is a basic class for all xml objects
-- developer can use it as the paradigm
-- function/method Build and Validate are implemented by the developer
--
-- @param o         xmlobject self
-- @param engine    engine object
-- @param node      xml data node
-- @return          this basic class
local XmlObject = class { -- class module paradigm
    id = "";
    engine = false; -- engine class
    node = false;   -- lom

    mode = false;   -- strict/checking mode
    skip = false;   -- (backward compatible)
    debug = false;  -- engine generate output
    dtd = "";

    -- assign the xmlobject's engine and lom-node
    -- create all modules/objects for the subnodes (non-recursively)
    ["<"] = function (o, engine, node) -- {{{ constructor
        o.engine = engine
        o.node = node -- node

        local nodel = node['&']
        local nodec = nodel and 0
        repeat
            for i = 1, #node do -- {{{
                local subn = node[i]
                local suba = type(subn) == 'table' and subn['@'] -- subnode attr
                if suba and suba.mod then -- skip node content/text: assign the object
                    subn['*'] = assert(require(suba.mod), "fail loading "..suba.mod)(engine, subn)
                    if tun.check(suba.strict) then -- check unused-tag {{{ honor metatable
                        for j = 1, #subn do -- some of them are text/comments:
                            local v = subn[j]
                            if type(v) == 'table' and not string.find(subn['*'].dtd, v['.']) then
                                o:Info("WRN: undefined "..subn['.'].."["..(suba.name or "").."]"..v['.'])
                            end
                        end
                    end -- }}}
                end
            end -- }}}
            if nodec then nodec = nodec < #nodel and nodec + 1 end
            node = nodec and nodel[nodec]
        until not node

        -- standard basic attributes/elements
        o.mode = tonumber(o:XmlAttribute('mode')) or 0
        o.skip = tun.check(o:XmlValue('Skip[1]'))
        o.debug = tun.check(o:XmlValue('Debug[1]'))
    end; -- }}}

    -- ===================== xmlobject utility subroutine ================= --
    Info = function (o, msg) -- {{{
        local node = o.node
        tun.info('('..(node['.'] or '.')..'['..(node['@'].name or '')..'])'..
            (msg and ' '..tostring(msg) or ''))
    end; ---}}}

    -- ====== tag handling functions: order kept, but keys destroyed ====== --
    XmlAttribute = function (o, attr, errmsg) -- {{{
        return o.node['@'] and o.node['@'][attr] or
            (ermsg and error("Missing attribute("..o.node['.']..'@'..attr..") "..tostring(errmsg), 2))
    end; -- }}}

    XmlElement = function (o, elem, cls, errmsg) -- {{{ default class/object on element
        local tag, pos = string.match(elem, '([^/]+)(.*)$')
        if pos ~= '' then error('ERR: compound element ('..elem..')', 2) end
        elem, pos = string.match(tag, '([^%[]+)%[?([^%]]*)')
        o.dtd = string.find(o.dtd, elem) and o.dtd or (elem..' '..o.dtd)
        tag = o.node:xpath(tag) -- tagtbl -- original (possible metatable)
        if type(cls) == 'string' then
            for i = 1, #tag do -- {{{
                local subn = tag[i] -- subnode
                local suba = subn['@']
                if not (suba and suba.mod) then -- assign the object
                    subn['*'] = assert(require(cls), 'fail loading '..cls)(o.engine, subn)
                    if tun.check(suba.strict) then -- check unused-tag {{{ honor metatable
                        for j = 1, #subn do -- some of them are text/comments:
                            local v = subn[j]
                            if type(v) == 'table' and not string.find(subn['*'].dtd, v['.']) then
                                o:Info('WRN: undefined '..elem..'['..(suba.name or '')..']'..cls..':'..v['.'])
                            end
                        end
                    end -- }}}
                end
            end -- }}}
        end
        return (#tag == 0 and errmsg) and error('No <'..elem..'>: '..tostring(errmsg)) or tag
    end; -- }}}

    XmlValue = function (o, elem, errmsg) -- {{{ xml (string) value -- tagtbl
        local tag, pos = string.match(elem, '([^/]+)(.*)$')
        if pos ~= '' then error('ERR: compound element ('..elem..')', 2) end
        elem, pos = string.match(tag, '([^%[]+)%[?([^%]]*)')
        o.dtd = string.find(o.dtd, elem) and o.dtd or (elem..' '..o.dtd)
        tag = o.node:xpath(tag) -- tagtbl -- original (possible metatable)
        if #tag == 0 then
            return errmsg and err('Missing ('..o.node['.']..'.'..elem..') '..tostring(errmsg), 2)
        end
        elem = {}
        for i = 1, #tag do
            for _, v in ipairs(tag[i]) do if type(v) == 'string' then table.insert(elem, v) end end
        end
        return elem
    end; -- }}}

    Run = function (o, elem, ...) -- build the node element/subnode {{{
        if type(elem) == 'string' then -- run all subnode class/object' Build if attr meets
            local tag, pos = string.match(elem, '([^/]+)(.*)$')
            if pos ~= '' then error('ERR: compound element ('..elem..')', 2) end
            elem, pos = string.match(tag, '([^%[]+)%[?([^%]]*)')
            tag = o.node:xpath(tag) -- tagtbl
            if #tag == 0 then return o:Info('WRN: Run empty '.. elem) end -- if not defined
            for i = 1, #tag do
                local sub = tag[i]['*']
                if sub then -- NB: tag info
                    sub:Build(elem, ...)
                    if sub.debug then sub:Validate(elem, ...) end
                end
            end
        elseif type(elem) == 'table' then -- specific tagtbl (xml-node)
            local sub = elem['*']
            if sub then
                sub:Build(elem['.'], ...)
                if sub.debug then sub:Validate(elem['.'], ...) end
            end
        end
    end; -- }}}

    -- ===================== customize (place holdr) ====================== --
    Build = function (o, dbgmsg) -- self: based on the node content to modify the engine data {{{
        if o.skip then return dbgmsg and o:Info(dbgmsg) end -- infra CODING debug message
        local node, engine, data = o.node, o.engine, o.engine.data
        local nodel = node['&']
        local nodec = nodel and 0
        repeat
            for i = 1, #node do if type(node[i]) == 'table' then o:Run(node[i]) end end
            if nodec then nodec = nodec < #nodel and nodec + 1 end
            node = nodec and nodel[nodec]
        until not node
        if o.debug then end -- BKM
    end; -- }}}

    Validate = function (o, elem) -- BKM: check the element formats/values {{{
        local valid = {}
        for k, v in pairs(o) do table.insert(valid, k..' = '..tostring(v)) end
        o:Info('Validate '..elem..'\n  '..table.concat(valid, '\n  '))
    end; -- }}}
}

-- {{{ ==================  demo and self-test (QA)  ==========================
local o = lom.xml({{['.'] = 'T', ' Fab  '; {['.'] = 'T', '8 '}},})
local q = XmlObject(nil, o) -- nil engine
if -- failing conditins:
    q:XmlValue('T[0]')[1] ~= ' Fab  '
    or q:XmlElement('T[2]')[1][2][1] ~= '8 '
then error('QA failed.', 1) end -- }}}

return XmlObject
-- vim:ts=4:sw=4:sts=4:et:fen:fdm=marker:fmr={{{,}}}:fdl=1
