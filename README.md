# lox (lua object model for x/html)


X/HTML is wacky and verbose. To parse an x/html file to a lua table
and be able to recover it back to the orignal x/html, it needs some design.
Our design uses pretty much the same as the document object model
in luaExpat's `lxp.lom`; however, we use OOP to add some structures
and functionalities on our dom, and we also implement
a simple/sloppy markup language parser for an expat replacement.

More info will be added to the [wiki page](https://github.com/josh-feng/lox/wiki/LOX-(Lua-Object-model-for-X-html))


A quick example:

For the following file `example.xml`
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
  { ['.'] = 'b', ['@'] = {'b', 'c', b = '1', c = '2'} },
  '\0 comment ',
  { ['.'] = 'b',
    'text1',
    { ['.'] = 'b', ['@'] = {'b', b = 'b'} },
  },
  'text2',
  { ['.'] = 'tag' },
}
```

**NB:** the comment text will be prefixed with `\0` character.

It's useful for servers supporting lua, such as nginx/openresty (or apache + lua module).

## Requirements and Install


- lua >= 5.3
- pool: <https://github.com/josh-feng/pool>
- ~~luaExpat: <http://www.keplerproject.org/luaexpat> or <https://github.com/LuaDist/luaexpat>~~
- ~~posix <https://github.com/luaposix/luaposix/> (required by `us.lua`)~~

To install, run `makefile`, and copy files to lua's folders. Or check

- <https://luarocks.org/modules/josh-feng/lox>

# lom

Module `lom.lua` uses `pool.lua` and `us.lua` to create 'doc' objects

```lua
lom = require('lom')

doc1 = lom('/path/to/file.html') -- doc object

doc2 = lom('')                   -- text/data are supplied to parser
doc2:parse(xmltxt_1)
-- ...
doc2:parse(xmltxt_n):parse()

doc3 = lom(doc2)                 -- dom from a table

lom(true) -- buildxlink
```

There are 3 ways to create document objects with different initial arguments

- file path: `doc = lom('/path/to/file')`

    parser will process the whole xml file.

- empty string: `doc = lom('')`

    xml text fragments can be supplied to parser in sequence: `doc:parse(txt)`.
    If calling its paser with `nil`, the parser stage will be closed,
    and the doc is fully processed: `doc:parse()`.

- table (from xpath): `doc = lom({...})`

    the doc is a **dom** class instance, initialized with the supplied table.

Internally dom is a `pool` class with the following structure:

```lua
dom = class {
    ['.'] = false; -- tag name
    ['@'] = false; -- attr
    ['&'] = false; -- xlink table
    ['?'] = false; -- errors
    ['*'] = false; -- module
    ['+'] = false; -- schema / declaration
}
```

with some member functions

function | description
---------|-------------
parse | parse the text
xpath | return a table
drop  | output as a string
select | return the nodes
remove | remove nodes


Calling `lom` with everything else will trigger the **buildxlink** procedure, which will build the xpointer links:

```lua
lom(true)
```

### Non-ending tags

Html singletons are
`area`, `base`, `br`, `col`, `command`, `embed`, `hr`, `img`, `input`, `keygen`, `link`,
`meta`, `param`, `source`, `track`, `wbr`.

### Boolean attributes

HTML attributes with no values will confuse the luaexpat.

`allowfullscreen`, `async`, `autofocus`, `autoplay`, `checked`, `controls`, `default`, `defer`, `disabled`, `formnovalidate`, `ismap`, `itemscope`, `loop`, `multiple`, `muted`, `nomodule`, `novalidate`, `open`, `playsinline`, `readonly`, `required`, `reversed`, `selected`, `truespeed`

### Escaped characters (in javascript)

`<` and `>`

## buildxlink / xpath / api

Some standards for xpath
- <https://developer.mozilla.org/en-US/docs/Web/XPath>
- <https://www.w3schools.com/xml/xpath_intro.asp>

Lox partially implement XPATH standards. Some functionality
can be achieved by doc `lom.api` extensions.

xpath and xlinks can be shown in following cross-linked xml files

`a.xml`

```html
<a>
    <b>we</b>
    <c />
    <d />
    <e />
    <b>us</b>
</a>
```

`b.xml`

```html
<b>
    <aa />
    <c xlink:href="a.xml#xpointer(/a/b)" />
    <d xlink:href="a.xml#xpointer(/a/b/)" />
</b>
```

The `lom` will parse these 2 files and then build the xlink:

```lua
lom = require('lom')
we = require('us')

doc_a = lom('a.xml')
doc_b = lom('b.xml')
lom(true) -- build the links

print(doc_b:drop())

{
    {
        {["."] = "aa"},
        {
            ["&"] = {{"we", ["."] = "b"}, {"us", ["."] = "b"}, 0 = 0.84018771676347},
            ["."] = "c",
            ["@"] = {"xlink:href", ["xlink:href"] = "a.xml#xpointer(/a/b)"}
        },
        {
            ["&"] = {"we", "us", 0 = 0.84018771676347},
            ["."] = "d",
            ["@"] = {"xlink:href", ["xlink:href"] = "a.xml#xpointer(/a/b/)"}
        },
        ["."] = "b"
    }
}

print(doc_b:select('b/c'):drop())

{
    {
        ["&"] = {{"we", ["."] = "b"}, {"us", ["."] = "b"}, 0 = 0.84018771676347},
        ["."] = "c",
        ["@"] = {"xlink:href", ["xlink:href"] = "a.xml#xpointer(/a/b)"}
    }
}
```

Lox's dom sometimes has a tag link entry, which is a table and contains `["&"][0]`. It is the time-stamp for building xlinks.

Lox's dom support the following methods: `parse`, `xpath`, `drop`, and `select`.


xpath examples:

unit|example|description
------|-----|------
\#|\[id="x",**3**,proc="reader"\]|the 3rd element



### Examples


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

```lua
lom = require('lom')
doca = lom('a.xml')
docb = lom('b.xml')
lom(true)
print(doca:select('/a/b/'):drop())      --> {"we", "us"}
print(doca:select('/a/b'):drop())       --> {{"we", ["."] = "b"}, {"us", ["."] = "b"}}
print(doca:select('a/b/'):drop())       --> {"we", "us", "lom"}
print(doca:select('a/b'):drop())        --> {{"we", ["."] = "b"}, {"us", ["."] = "b"}, {"lom", ["."] = "b", ["@"] = {"a1", a1 = "r"}}}
print(doca:select('a/b[a1=r]/'):drop()) --> {"lom"}
print(doca:select('a/b[a1=r]'):drop())  --> {{"lom", ["."] = "b", ["@"] = {"a1", a1 = "r"}}}
print(docb:drop())
-->
{
    {
        {["."] = "aa"},
        {
            ["&"] = {{"we", ["."] = "b"}, {"us", ["."] = "b"}, {"lom", ["."] = "b", ["@"] = {"a1", a1 = "r"}}, 0 = 0.84018771676347},
            ["."] = "c",
            ["@"] = {"xlink:href", ["xlink:href"] = "a.xml#xpointer(a/b)"}
        },
        {
            ["&"] = {"we", "us", 0 = 0.84018771676347},
            ["."] = "d",
            ["@"] = {"xlink:href", ["xlink:href"] = "a.xml#xpointer(/a/b/)"}
        },
        ["."] = "b"
    }
}
```


## XmlObject

Attribute `proc` invoke module/class instantiation.

    <tag proc='module1'> ... </tag>

where `module1.lua` will return another *derived* XmlObject class.

```lua
lom = require('lom')
we = require('us')
XmlObject = require('XmlObject')

doc = lom('doc.xml')
lom(true) -- build the links

x = XmlObject(doc)
```

# lsmp

Lua Simple/Sloppy Markup Parser (lsmp) is a quick and dirty replacement
for expat.
It's a SAX parser, so uses some callback functions.
Please read `lom.lua` to understand its usage.

XML expat coding reference:

- <https://strophe.im/libstrophe/doc/0.10.0/expat_8h.html>

## build with makefile

Please modify `makefile` based on your system.


# Note

TODO

- XmlObject
- error handling
