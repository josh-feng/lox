/* simple/sloppy markup(html/xml/xhtml) parser is a SAX XML parser */
#ifndef __lsmp_h
#define __lsmp_h

#include <stdio.h>

typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;

#define StartElementKey   "StartElement"
#define EndElementKey     "EndElement"
#define CharDataKey       "CharData"
#define CommentKey        "Comment"
#define ExtensionKey      "Extension"
#define SchemeKey         "Scheme"

enum MPState { /* parser status */
  MPSpre,      /* initialized */
  MPSok,       /* state while parsing */
  MPSstring,   /* state while reading a string */
  MPSfinished, /* state after finished parsing */
  MPSerror
};

#define M_STRICT    0x00
#define M_ESCAPE    0x01 /* \< \> \" \' */
#define M_SLOPPY    0x02 /* <_ _> */
#define M_QUOTES    0x04 /* flexible ".." '..' */
#define M_ANYTAG    0x08 /* other/non-standard tag [] */
#define M_MODES     0x0F

#define S_TEXT      0x00
#define S_CDATA     0x10 /* CDATA, COMMENT, and Extension*/
#define S_MARKUP    0x20
#define S_CSCHEM    0x30
#define S_STRING    0x40 /* in MARKUP */
#define S_ERROR     0x50
#define S_DONE      0x60
#define S_STATES    0x70

#define F_TOKEN     0x80 /* got tag name */

typedef void (*SML_StartElementHdlr) (void *ud, const char *name, const char **atts);
typedef void (*SML_EndElementHdlr)   (void *ud, const char *name);
typedef void (*SML_CharDataHdlr)     (void *ud, const char *s, int len);
typedef void (*SML_CommentHdlr)      (void *ud, const char *s, int len);
typedef void (*SML_ExtensionHdlr)    (void *ud, const char *s, int len);
typedef void (*SML_SchemeHdlr)       (void *ud, const char *name, const char **atts);

typedef struct GlnkStruct {
  GlnkStruct *next;
  void *data;
} Glnk;

typedef struct SML_ParserStruct {
  void *ud; /* userdata */
  char *buf;
  unsigned int len, size;
  unsigned int r, c, i; /* row, column, byte index */
  SML_CharDataHdlr     ft; /* text <!CDATA[ ]]> */
  SML_StartElementHdlr fs; /* markup tag start */
  SML_EndElementHdlr   fe; /* markup tag end */
  SML_CommentHdlr      fc; /* comment <!-- --> */
  SML_SchemeHdlr       fd; /* definition <! [] > */
  SML_ExtensionHdlr    fx; /* extension <? ?> */

  BYTE mode; /* mode + state */
  char quote;

  BYTE iExt; /* == Exts if no */
  BYTE Exts; /* 255 */
  const char **szExts; /* pair <? ?> */

  char *elem;
  Glnk *attr;
} *SML_Parser;

/* markup <...> / text ...

  <..1<..2>  0x00/q  <..1<..2>
  <..1>..2>  0x00/q  <..1> and ..2>

  MPSpre   -> <     -> MPSok
  MPSok    -> quote -> MPSstring
           -> >     -> MPSpre
  MPString -> quote -> MPSok
  MPSerror
  MPSfinished

  '<' ==>
       '!'
           '[CDATA[' ==> S_CDATA
               ']]>' ==> S_TEXT
           '--' ==> S_CDATA
               '-->' ==> S_TEXT
       tokens: ==> S_MARKUP
           '"' ==> S_STRING ==> '"' ==> S_MARKUP
           '>' ==> S_TEXT
*/

/*
void SML_SetElementHdlr  (SML_Parser, SML_StartElementHdlr, SML_EndElementHdlr);
void SML_SetCharDataHdlr (SML_Parser, SML_CharDataHdlr);
void SML_SetCommentHdlr  (SML_Parser, SML_CommentHdlr);
void SML_SetCommentHdlr  (SML_Parser, SML_CommentHdlr);
*/

#define SML_GetCurrentLineNumber(p)     ((p)->r)
#define SML_GetCurrentColumnNumber(p)   ((p)->c)
#define SML_GetCurrentByteIndex(p)      ((p)->i)

SML_Parser    SML_ParserCreate (void *opt);
void          SML_SetEncoding  (SML_Parser p, const char *coding);
enum MPState  SML_Parse        (SML_Parser p, const char *s, int len, int fEnd);
const char   *SML_ErrorString  (SML_Parser p);
void          SML_ParserFree   (SML_Parser p);

#endif
