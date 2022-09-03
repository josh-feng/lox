/* lua simple/sloppy markup(html/xml/xhtml) parser is a SAX XML parser */
#ifndef __lsmp_h
#define __lsmp_h

/* associated lsmp (XML_ParserStruct is in libexpat's xmlparse.c) */
struct SML_ParserStruct {
  void *ud;
  char *buf;
  /* TODO */
  const char *singletons;
  int mode; /* byte flags */
  unsigned int r, c; /* row and column */
  unsigned int i;    /* byte index */
  const char *endoding;
};
typedef struct SML_ParserStruct *SML_Parser;

typedef void (*SML_StartElementHdlr) (void *ud, const char *name, const char **atts);
typedef void (*SML_EndElementHdlr)   (void *ud, const char *name);
typedef void (*SML_CharDataHdlr)     (void *ud, const char *s, int len);
typedef void (*SML_CommentHdlr)      (void *ud, const char *data);

/*
typedef void (*SML_SetUserData)     (SML_Parser, void *ud);
typedef void (*SML_SetElementHdlr)  (SML_Parser, SML_StartElementHdlr, SML_EndElementHdlr);
typedef void (*SML_SetCharDataHdlr) (SML_Parser, SML_CharDataHdlr);
typedef void (*SML_SetCommentHdlr)  (SML_Parser, SML_CommentHdlr);
*/

void SML_SetUserData     (SML_Parser, void *ud);
void SML_SetElementHdlr  (SML_Parser, SML_StartElementHdlr, SML_EndElementHdlr);
void SML_SetCharDataHdlr (SML_Parser, SML_CharDataHdlr);
void SML_SetCommentHdlr  (SML_Parser, SML_CommentHdlr);

#define SML_GetCurrentLineNumber(p)     ((p)->r)
#define SML_GetCurrentColumnNumber(p)   ((p)->c)
#define SML_GetCurrentByteIndex(p)      ((p)->i)

SML_Parser  SML_ParserCreate (void *);
void        SML_SetEncoding  (SML_Parser, const char *);
int         SML_Parse        (SML_Parser, const char *s, int len, int fEnd);
const char *SML_ErrorString  (SML_Parser);
void        SML_ParserFree   (SML_Parser);

#define StartCdataKey             "StartCdataSection"
#define EndCdataKey               "EndCdataSection"
#define CharDataKey               "CharData"
#define CommentKey                "Comment"
#define StartElementKey           "StartElement"
#define EndElementKey             "EndElement"

#endif
