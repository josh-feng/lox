# lox (lua object model for xml)

XML is wacky and verbose. To parse an xml file to a lua table
and be able to recover it back to the orignal xml, it needs some design:

    <tag>                     { ['.'] = 'tag',
        <b b='1' c='2' />       { ['.'] = 'b', ['@'] = {'b', 'c', b = '1', c = '2'} },
        <!-- comment -->        '\0 comment ',
        <b>                     { ['.'] = 'b',
            text1                   'text1',
            <b b='b' />             { ['.'] = 'b', ['@'] = {'b', b = 'b'} },
        </b>                    },
        text2                   'text2',
    </tag>                    }


## lom


Module lom.lua uses 'pool.lua' and 'us.lua' to create 'doc' objects

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

- file path
- nil
- table (from xpath)

Everything eles will trigger the buildxlink procedure.


## buildxlink / xpath / api




## XmlObject



