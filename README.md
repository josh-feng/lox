# lox (lua object model for xml)

XML is wacky and verbose. To parse an xml file to a lua table
and be able to recover it back to the orignal xml, it needs some design:

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

stored as a lua table (doc):

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

It's useful for servers supporting lua, such as nginx/openresty (or apache + lua module)

For client side, javascript is supported by most browsers, there are some
new browsers supporting lua internally, such as `luakit`.
We will see if running lua inside a browser (console) is possible.

## Requirements

- lua >= 5.1
- luaExpat: <http://luaExpat> or <https://github.com/LuaDist/luaexpat>
- pool: <https://github.com/josh-feng/pool.git>

## lom


Module `lom.lua` uses `pool.lua` and `us.lua` to create 'doc' objects

```lua
lom = require('lom')

doc1 = lom('/path/to/file.xml' or '')   -- doc object

doc2 = lom()
doc2:parse(xmltxt_1)
-- ...
doc2:parse(xmltxt_n):parse()

lom(true) -- buildxlink
```

There are 3 ways to create document object

- file path: `doc = lom('/path/to/file')`

- nil: `doc = lom()`

- table (from xpath): `doc = lom({...})`

Everything eles will trigger the **buildxlink** procedure, which will build the xpointer links.


## buildxlink / xpath / api


xpath and xlinks


## XmlObject

Attribute `proc` invoke module/class instantiation.

    <tag proc='module1'> ... </tag>

where `module1.lua` will return another *derived* XmlObject class.

