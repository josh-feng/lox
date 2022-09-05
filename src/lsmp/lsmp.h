/* simple/sloppy markup(html/xml/xhtml) parser is a SAX XML parser */
#ifndef __lsmp_h
#define __lsmp_h

#define StartCdataKey     "StartCdataSection"
#define EndCdataKey       "EndCdataSection"
#define CharDataKey       "CharData"
#define CommentKey        "Comment"
#define StartElementKey   "StartElement"
#define EndElementKey     "EndElement"

#define M_strict    0x00

typedef void (*SML_StartElementHdlr) (void *ud, const char *name, const char **atts);
typedef void (*SML_EndElementHdlr)   (void *ud, const char *name);
typedef void (*SML_CharDataHdlr)     (void *ud, const char *s, int len);
typedef void (*SML_CommentHdlr)      (void *ud, const char *s, int len);

typedef struct SML_ParserStruct {
  void *ud; /* userdata */
  char *buf;
  unsigned int r, c; /* row and column */
  unsigned int i;    /* byte index */
  SML_CharDataHdlr     fd;
  SML_StartElementHdlr fs;
  SML_EndElementHdlr   fe;
  SML_CommentHdlr      fc;
  int mode;
  const char *encoding;
  const char *singletons; /* strstr */
} *SML_Parser;

enum MPState { /* parser status */
  MPSpre,      /* initialized */
  MPSok,       /* state while parsing */
  MPSfinished, /* state after finished parsing */
  MPSerror,
  MPSstring    /* state while reading a string */
};

/* markup <...> / text ...

  0x00 strict 
  0x01 escape    \\ \< \> \" \' \x         
  0x02 space     <_ _>
  0x04 quotation dominant ".." '..'
  0x08 
  0x10 
  0x20 
  0x40 
  0x80 
 
  <..1<..2>  0x00/q  <..1<..2>
  <..1>..2>  0x00/q  <..1> and ..2>

  MPSpre -> quote -> MPSok
         -> text  -> MPSok
         -> tag   -> MPSok
  MPSstring ->
  MPSok
  MPSerror
  MPSfinished
*/

/*
void SML_SetElementHdlr  (SML_Parser, SML_StartElementHdlr, SML_EndElementHdlr);
void SML_SetCharDataHdlr (SML_Parser, SML_CharDataHdlr);
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
