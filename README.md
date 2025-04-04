# lox (lua object model for x/html)


X/HTML is wacky and verbose. To parse an x/html file to a lua table
and be able to recover it back to the orignal x/html, it needs some design.
Our design uses pretty much the same document object model
as in luaExpat's `lom.lua`; however, we use OOP to add some structures
and functionalities on our *dom*, and we also implement
a simple/sloppy markup language parser (*tag soup parser*) for an expat replacement.

More info will be added to the [wiki page](https://github.com/josh-feng/lox/wiki/LOX-(Lua-Object-model-for-X-html))


A quick example for the following file `example.xml`

```html
<tag>
    <b b='1' c='2' />
    <!-- comment -->
    <b>
        text1
        <b b='b' />
    </b>
    text2
    <tag />
</tag>
```


lox will convert it as a lua table (doc):

> ] ./lom.lua example.xml

```lua
{ ['.'] = 'tag',
  { ['.'] = 'b', ['@'] = {b = '1', c = '2'} },
  '\0 comment ',
  { ['.'] = 'b',
    'text1',
    { ['.'] = 'b', ['@'] = {b = 'b'} },
  },
  'text2',
  { ['.'] = 'tag' },
}
```


- `['.']` entry assigns the tag name.
- Subnodes (elements) are placed consecutively in the table.
- The comment text will be prefixed with `\0` character.
- Attributes are recorded in `['@']` entry; however, their order is ignored.

It's useful for servers supporting lua, such as nginx/openresty (or apache + lua module).

## Requirements and Install


- lua >= 5.3
- pool: <https://github.com/josh-feng/pool>
- ~~luaExpat: <http://www.keplerproject.org/luaexpat> or <https://github.com/LuaDist/luaexpat>~~
- ~~posix <https://github.com/luaposix/luaposix/> (required by `us.lua`)~~

To install, run `makefile`, and copy files to lua's folders. Or check

- <https://luarocks.org/modules/josh-feng/lox>


## lom.lua (lua object model)

Module `lom.lua` uses `pool.lua` and `us.lua` to create 'doc' objects

```lua
lom = require('lom')

doc1 = lom('/path/to/file.html') -- doc object

doc2 = lom('')                   -- text/data are supplied to parser consecutively
doc2:parse(xmltxt_1)
-- ...
doc2:parse(xmltxt_n):parse()     -- final call to close the parsing

doc3 = lom(doc2)                 -- dom from a table

lom(true) -- build xlink among doc's
```

Calling `lom` with everything else will trigger the **buildxlink** procedure, which will build the xpointer links:


xpath and xlinks can be shown in following cross-linked xml files

`a.xml`

```html
<a>
    <b>we</b>
    <c attr='1 + 1' c=b attr=3 />
    <d />
    <e attr />
    <b>us</b>
</a>
```

`b.xml`

```html
<b>
    <aa />
    <!--NB-->
    <c xlink:href="a.xml#xpointer(/a/b)" />
    some text
    <?php say what?>
    <d xlink:href="a.xml#xpointer(/a/b/)" />
</b>
```

The `lom` will parse these 2 files and then build the xlink:

```lua
lom = require('lom')
doca = lom('a.xml', 0x0f)   -- default mode: skip comments
docb = lom('b.xml', 0x0f)   -- default mode: skip comments

lom(true)

print(doca:drop())
{
    {
        {"we", ["."] = "b"},
        {["."] = "c", ["@"] = {attr = "3", c = "b"}},
        {["."] = "d"},
        {["."] = "e", ["@"] = {attr = true}},
        {"us", ["."] = "b"},
        ["."] = "a"
    }
}

print(docb:drop())
{
    {
        {["."] = "aa"},
        {
            ["&"] = {{"we", ["."] = "b"}, {"us", ["."] = "b"}, [0] = 0.84018771676347},
            ["."] = "c",
            ["@"] = {["xlink:href"] = "a.xml#xpointer(/a/b)"}
        },
        "
            some text",
        {["&"] = {"we", "us", [0] = 0.84018771676347}, ["."] = "d", ["@"] = {["xlink:href"] = "a.xml#xpointer(/a/b/)"}},
        ["."] = "b"
    }
}
```

Lox's dom sometimes has a tag link entry, which is a table and contains entry `["&"][0]`, the time-stamp of building xlinks.


## Examples


`a.xml`

```html
<a>
    <b>we</b>
    <c />
    <d />
    <e>
    <a>
        <b a1="r">lom</b>
    </a>
    </e>
    <b>us</b>
</a>
```

`b.xml`

```html
<b>
    <aa />
    <c xlink:href="a.xml#xpointer(a/b)" />
    <d xlink:href="a.xml#xpointer(/a/b/)" />
</b>
```

The `select` method is demonstrated in the following various cases,
and a shorter form of is shown in the last case:

```lua
#!/bin/env lua
lom = require('lom')

doca = lom('a.xml')
docb = lom('b.xml')
lom(true)
print(doca:select('/a/b'):drop())       --> {{"we", ["."] = "b"}, {"us", ["."] = "b"}}
print(doca:select('/a/b/'):drop())      --> {"we", "us", ["."] = "b"}
print(doca:select('a/b'):drop())        --> {{"we", ["."] = "b"}, {"us", ["."] = "b"}, {"lom", ["."] = "b", ["@"] = {a1 = "r"}}}
print(doca:select('a/b/'):drop())       --> {"we", "us", "lom", ["."] = "b", ["@"] = {a1 = "r"}}
print(doca:select('a/b[a1=r]'):drop())  --> {{"lom", ["."] = "b", ["@"] = {a1 = "r"}}}
print(doca:select('a/b[a1=r]/'):drop()) --> {"lom", ["."] = "b", ["@"] = {a1 = "r"}}
print(doca:drop())
--[[
{
    {
        {"we", ["."] = "b"},
        {["."] = "c"},
        {["."] = "d"},
        {{{"lom", ["."] = "b", ["@"] = {a1 = "r"}}, ["."] = "a"}, ["."] = "e"},
        {"us", ["."] = "b"},
        ["."] = "a"
    }
}
--]]
print(docb:drop())
--[[
{
    {
        {["."] = "aa"},
        {
            ["&"] = {{"we", ["."] = "b"}, {"us", ["."] = "b"}, {"lom", ["."] = "b", ["@"] = {a1 = "r"}}, [0] = 0.84018771676347},
            ["."] = "c",
            ["@"] = {["xlink:href"] = "a.xml#xpointer(a/b)"}
        },
        {
            ["&"] = {
                {"we", ["."] = "b"},
                {["."] = "c"},
                {["."] = "d"},
                {{{"lom", ["."] = "b", ["@"] = {a1 = "r"}}, ["."] = "a"}, ["."] = "e"},
                {"us", ["."] = "b"},
                ["."] = "a",
                [0] = 0.84018771676347
            },
            ["."] = "d",
            ["@"] = {["xlink:href"] = "a.xml#xpointer(/a/)"}
        },
        ["."] = "b"
    }
}
--]]
print(docb:select('b/c'):drop())
--[[
{
    {
        ["&"] = {{"we", ["."] = "b"}, {"us", ["."] = "b"}, {"lom", ["."] = "b", ["@"] = {a1 = "r"}}, [0] = 0.84018771676347},
        ["."] = "c",
        ["@"] = {["xlink:href"] = "a.xml#xpointer(a/b)"}
    }
}
--]]
print(docb('b/d'):drop())
--[[
{
    {
        ["&"] = {
            {"we", ["."] = "b"},
            {["."] = "c"},
            {["."] = "d"},
            {{{"lom", ["."] = "b", ["@"] = {a1 = "r"}}, ["."] = "a"}, ["."] = "e"},
            {"us", ["."] = "b"},
            ["."] = "a",
            [0] = 0.84018771676347
        },
        ["."] = "d",
        ["@"] = {["xlink:href"] = "a.xml#xpointer(/a/)"}
    }
}
--]]

we = require('us')
print(we.var2str(doca()))         --> {a = {b = {"we", "us"}, c = "", d = "", e = {a = {b = {"lom", ["@"] = {a1 = "r"}}}}}}
print(we.var2str(docb('b/d')()))  --> {d = {["@"] = {["xlink:href"] = "a.xml#xpointer(/a/)"}}}

```

As shown in the last two commands,
besides the short form for `select`, calling the doc object without any argument will
return a converted *simplified* lua table, with convention key/value pair.
Further example is shown below for `c.html`

```html
<a>
  <c s=1>
    <b>text1</b>
    <b>text1</b>
  </c>
  text4
  <c>
    <b>text3</b>
    <b u=3>text3</b>
  </c>
  text5
</a>
<a t=2>
  <b>text2</b>
</a>
```

and its simplified table:

```lua
docc = lom('c.xml')
print(we.var2str(docc()))
--[[
{
    a = {
        {
            "   text4", "   text5",
            c = {{["@"] = {s = "1"}, b = {"text1", "text1"}}, {b = {"text3", {"text3", ["@"] = {u = "3"}}}}}
        },
        {["@"] = {t = "2"}, b = "text2"}
    }
}
--]]
```


## lsmp.so (lua sloppy markup parser)

Lua Simple/Sloppy Markup Parser (lsmp) is a quick and dirty replacement
for expat.
It's a SAX parser, so uses some callback functions.
Please read `lom.lua` to understand its usage.

### build with makefile

Please modify `makefile` based on your system.


## us.lua

Useful Stuff module contains a funciton `var2str` to print out a table value,
which is *useful* for debugging.
To make sure self-referenced table is well treated,
we turn on the safe mode:

```lua
we = require('us') -- working environments

a = {a = 1}
print(we.var2str(a))
-->
    {a = 1}

a.b = a
print(we.var2str(a, true)) -- safe mode
-->
    table0x5590a12caeb0 = {a = 1}
    table0x5590a12caeb0.b = table0x5590a12caeb0

print(we.var2str(a, 'a', {safe = true})) -- name with safe mode
-->
    a = {a = 1}
    a.b = a
```

## XmlObject.lua

Attribute `proc` invoke module/class instantiation.

    <tag proc='module1'> ... </tag>

where `module1.lua` will return another *derived* XmlObject class.

```lua
lom = require('lom')
XmlObject = require('XmlObject')

doc = lom('doc.xml')
lom(true) -- build the links

x = XmlObject(doc)
```


# []()
<!-- vim:ts=4:sw=4:sts=4:et:fdm=marker:fdl=1:sbr=-->
