/* simple/sloppy markup(html/xml/xhtml) parser
** Josh Feng (C) MIT 2022
*/
#ifndef __lsmp_h
#define __lsmp_h

#include <stdio.h>

typedef __uint8_t  BYTE;

#define StartElementKey   "StartElement"
#define EndElementKey     "EndElement"
#define CharacterDataKey  "CharacterData"
#define CommentKey        "Comment"
#define ExtensionKey      "Extension"
#define SchemeKey         "Scheme"

typedef void (*SML_StartElementHdlr) (void *ud, const char *name, const char **atts);
typedef void (*SML_EndElementHdlr)   (void *ud, const char *name);
typedef void (*SML_CharDataHdlr)     (void *ud, const char *s, int len);
typedef void (*SML_CommentHdlr)      (void *ud, const char *s, int len);
typedef void (*SML_ExtensionHdlr)    (void *ud, const char *s, int len);
typedef void (*SML_SchemeHdlr)       (void *ud, const char *name, const char **atts);

/* mode */
#define M_STRICT    0x00
#define M_ESCAPE    0x01 /* \< \> \" \' */
#define M_SLOPPY    0x02 /* <_ _> */
#define M_QUOTES    0x04 /* flexible ".." '..' */
#define M_ANYTAG    0x08 /* other/non-standard tag [] */
#define M_MODES     0x0F

/* state */
#define S_TEXT      0x00
#define S_CDATA     0x10 /* CDATA, COMMENT, and Extension*/
#define S_MARKUP    0x20
#define S_STRING    0x30 /* in MARKUP */
#define S_ERROR     0x40
#define S_DONE      0x50
#define S_STATES    0x70

/* flag */
#define F_TOKEN     0x80 /* tag name found */

typedef struct Glnk Glnk;
struct Glnk { Glnk *next; void *data; };

typedef struct SML_ParserStruct {
  void *ud;                /* userdata */
  char *buf;
  unsigned int len, size;
  unsigned int r, c, i, n; /* row, column, byte index, pre-col */
  SML_CharDataHdlr     ft; /* text <!CDATA[ ]]> */
  SML_StartElementHdlr fs; /* markup tag start */
  SML_EndElementHdlr   fe; /* markup tag end */
  SML_CommentHdlr      fc; /* comment <!-- --> */
  SML_SchemeHdlr       fd; /* definition <!* [] > */
  SML_ExtensionHdlr    fx; /* extension <? ?> */

  BYTE mode;  /* mode + state + flag */
  char quote; /* "' */

  const char **szExts; /* pair <? ?> ... */
  char *lszExts;       /* length of closing token */
  BYTE Exts;           /* # of pairs */
  BYTE iExt;           /* found index */

  char *elem;
  Glnk *attr;
  int  level;
} *SML_Parser;

#define SML_GetCurrentLineNumber(p)     ((p)->r + 1)
#define SML_GetCurrentColumnNumber(p)   ((p)->c + 1)
#define SML_GetCurrentByteIndex(p)      ((p)->i + 1)

enum MPState { /* parser status */
  MPSok,       /* state while parsing */
  MPSstring,   /* state while reading a string */
  MPSfinished, /* state after finished parsing */
  MPSerror
};

SML_Parser    SML_ParserCreate (void *ud, int mode, const char *ext);
enum MPState  SML_Parse        (SML_Parser p, const char *s, int len);
const char   *SML_ErrorString  (SML_Parser p);
void          SML_ParserFree   (SML_Parser p);

#endif
/*vim:ts=4:sw=4:sts=4:et:fen:fdm=marker:fmr={{{,}}}:fdl=1:cms=*/
