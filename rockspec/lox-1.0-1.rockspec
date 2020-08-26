package = "lox"
version = "1.0-1"
source = {
   url = "git+https://github.com/josh-feng/lox",
}
description = {
   summary = "Lua Objective XML (LOX)",
   detailed = [[
      LOX use XML parser to provide XML object.
      Lua's document object model
      xpath
      chain.api
   ]],
   homepage = "http://github.com/josh-feng/lox",
   license = "MIT",
}
dependencies = {
   "lua >= 5.1",
   "luaExpat",
   "pool"
}
build = {
   type = "builtin",
   modules = {
       -- lox = { -- RML c parser: c module written in C/++
       --    sources = {"src/lrp.cpp"},
       --    defines = {},
       --    libraries = {},
       --    incdirs = {"src"},
       --    libdirs = {"src"}
       -- }
   },
   install = {
       lua = {
           lox = "src/lox.lua", -- lox
           ['lox.btx'] = "src/btx.lua", -- btx
           -- ['lox.lrm'] = "src/lrm.lua", -- rml object model
           -- ['lox.lrps'] = "src/lrps.lua", -- rml parser
       },
   },
   copy_directories = {"doc", "test"}
}
