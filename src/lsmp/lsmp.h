// lua simple markup(html/xml/xhtml) parser is a SAX XML parser
#ifndef __lsmp_h
#define __lsmp_h

struct SML_ParserStruct { // associated lsmp (XML_ParserStruct is in libexpat's xmlparse.c)
  void *ud;
  char *buf;
  // TODO 
  int mode;
  unsigned int r, c; // row and column
  unsigned int i;    // byte index

};
typedef SML_ParseStruct (*SML_Parser);

typedef void (*SML_StartElementHandler)  (void *ud, const char *name, const char **atts);
typedef void (*SML_EndElementHandler)    (void *ud, const char *name);
typedef void (*SML_CharacterDataHandler) (void *ud, const char *s, int len);
typedef void (*SML_CommentHandler)       (void *ud, const char *data);

typedef void (*SML_SetElementHandler) (SML_Parser, SML_StartElementHandler, SML_EndElementHandler);
typedef void (*SML_SetCharacterDataHandler) (SML_Parser, SML_CharacterDataHandler);
typedef void (*SML_SetCommentHandler (SML_Parser, SML_CommentHandler);

void SML_SetUserData (SML_Parser, void *ud);
void SML_SetCharacterDataHandler (SML_Parser, SML_CharDataHdlr);
void SML_SetElementHandler (SML_Parser, SML_StartElementHdlr, SML_EndElementHdlr);
void SML_SetCommentHandler (SML_Parser, SML_CommentHdlr);

void         SML_SetEncoding            (SML_Parser, const char *);
unsigned int SML_GetCurrentLineNumber   (SML_Parser);
unsigned int SML_GetCurrentColumnNumber (SML_Parser);
unsigned int SML_GetCurrentByteIndex    (SML_Parser);

SML_Parser  SML_ParserCreate (void *);
int         SML_Parse        (SML_Parser, const char *s, int len, bool fEnd);
const char *SML_ErrorString  (SML_Parser);
void        SML_ParserFree   (SML_Parser);

////////////////////////////////////////////////////////////


#if !defined(lua_pushliteral)
#define  lua_pushliteral(L, s)	\
	lua_pushstring(L, "" s, (sizeof(s)/sizeof(char))-1)
#endif

#define ParserType	"MarkupParser"

// #define StartCdataKey             "StartCdataSection"
// #define EndCdataKey               "EndCdataSection"
#define CharDataKey               "CharacterData"
#define CommentKey                "Comment"
// #define DefaultKey                "Default"
// #define DefaultExpandKey          "DefaultExpand"
#define StartElementKey           "StartElement"
#define EndElementKey             "EndElement"
// #define ExternalEntityKey         "ExternalEntityRef"
// #define StartNamespaceDeclKey     "StartNamespaceDecl"
// #define EndNamespaceDeclKey       "EndNamespaceDecl"
// #define NotationDeclKey           "NotationDecl"
// #define NotStandaloneKey          "NotStandalone"
// #define ProcessingInstructionKey  "ProcessingInstruction"
// #define UnparsedEntityDeclKey     "UnparsedEntityDecl"
// #define StartDoctypeDeclKey       "StartDoctypeDecl"
// #define XmlDeclKey                "XmlDecl"

LUA_API int luaopen_lsmp (lua_State *L);
#endif
