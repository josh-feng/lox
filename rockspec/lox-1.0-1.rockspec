package = "lox"
version = "1.0-1"
source = {
   url = "git+https://github.com/josh-feng/lox",
   tag = "v1.0",
}
description = {
   summary = "lua object-model for xml",
   detailed = [[
      Lua Object-model for Xml

      Log:
      0.x parser/lua-dom/xpath
      1.x chain-lapi
   ]],
   homepage = "http://github.com/josh-feng/lox",
   license = "MIT",
   -- labels = {"xml", "linux"}
}
dependencies = {
   "lua >= 5.1",
   'LuaExpat >= 1.3',
   'luaposix >= 35',
   'pool >= 2.1',
}
build = {
   type = "builtin",
   modules = {
      lom = "src/lom.lua",        -- lua object model
      us  = "src/us.lua",         -- useful stuff
      xob = "src/XmlObject.lua",  -- xml object
   },
   copy_directories = {"doc", "examples"}
}
