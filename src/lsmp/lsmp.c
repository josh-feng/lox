/* lua simple/sloppy markup(x/html) parser
The first part is solely SML parser, it can be used without lua
The second part is for lua module, and shows a usage example of the first part
*/
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lsmp.h"

// BNF:
//
// '<' ==> S_MARK
//      '?'
//          '?>' ==> prev
//          '>'  ==> prev   --> error/warning
//      '!'
//          '[CDATA[' ==> S_DATA
//              ']]>'
//          '--' ==> S_CMNT
//              '-->' ==> prev
//          'DOCTYPE'
//              '>' ==> prev
//          '>'  ==> prev   --> error/warning
//      %a
//          '>' ==> S_DATA
//          '/>' ==> prev
//      '/'
//          '>' ==> prev
//      '>'         --> error/warning
// '&'
//      ';'         --> translate
//
// states: ps/previous-state cs/current-state
// S_NONE
// S_CMNT
// S_MARK (markup part)
// S_DATA
//
// posr/porc: row/column
//
// process instruction p = lxp.new(callbacks)
// assert(p:parse[[
// <to>
//   <?lua how is this passed to <here>? ?>
// </to>
// ]])
// new --> parser
//  StartElement = starttag,
//  EndElement = endtag,
//  CharData = mode < 0 and text or cleantext,
//  Comment = mode < 1 and comment or nil,
//  mode = 0, -- 0/strict, 1/non-strict, 2/sloppy, 3/allpass (_nonstrict = true)
//  stack = {o} -- {{}}
//
//
// NB: https://github.com/lunarmodules/luaexpat/blob/master/src/lxplib.c

SML_Parser SML_ParserCreate (void *opt) {
  SML_Parser p = (SML_Parser) malloc(sizeof(struct SML_ParserStruct));
  p->buf = NULL;
  return p;
}

void SML_SetEncoding (SML_Parser p, const char *coding) {
  /* TODO */
}

const char *SML_ErrorString (SML_Parser p) {
  /* TODO */
  return NULL;
}

/* the smp (sloppy markup parser) */
enum MPState SML_Parse (SML_Parser p, const char *s, int len, int fEnd) {
  char *c = (char *) s;
  while (len--) {
    switch (*c) { /* utf8 */
      case '<': break;
      case '>': break;
    }
    if (1) p->fs(p->ud, NULL, NULL);
    if (1) p->fe(p->ud, NULL);
    if (1) p->fd(p->ud, NULL, 0);
    if (1) p->fc(p->ud, NULL, 0);
    c++;
  }
  /* realloc p->buf */
  return MPSok;
}

void SML_ParserFree (SML_Parser p) {
  free(p->buf);
  p->buf = NULL;
  free(p);
}

/***************************************************************/
/********************* lua library related *********************/
/***************************************************************/

#include "lua.h"
#include "lauxlib.h"

#if (LUA_VERSION_NUM > 503)
#define lua_newuserdata(L, u)    lua_newuserdatauv(L, u, 1)
#define lua_setuservalue(L, i)   lua_setiuservalue(L, i, 1)
#define lua_getuservalue(L, i)   lua_getiuservalue(L, i, 1)
#endif


#if !defined(lua_pushliteral)
#define  lua_pushliteral(L, s)  \
  lua_pushstring(L, "" s, (sizeof(s)/sizeof(char))-1)
#endif

#define ParserType  "MarkupParser"

typedef struct lsmp_userdata {
  lua_State *L;
  SML_Parser parser;    /* associated sml p */
  int errorref;         /* reference to error message */
  enum MPState state;
} lsmp_ud;

/* Auxiliary function to call a Lua handle */
static void docall (lsmp_ud *mpu, int nargs, int nres) {
  lua_State *L = mpu->L;
  assert(mpu->state == MPSok);
  if (lua_pcall(L, nargs, nres, 0) != 0) {
    mpu->state = MPSerror;
    mpu->errorref = luaL_ref(L, LUA_REGISTRYINDEX);  /* error message */
  }
}

/*
Check whether there is a Lua handle for a given event: If so,
put it on the stack (to be called later), and also push `self'
*/
static int getHandle (lsmp_ud *mpu, const char *handle) {
  lua_State *L = mpu->L;
  if (mpu->state == MPSerror) return 0;
  lua_getuservalue(L, 1);
  lua_pushstring(L, handle);
  lua_gettable(L, 3);
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "lxp '%s' callback is not a function", handle);
  }
  lua_pushvalue(L, 1);  /* 1st arg (ud) in every call (self) */
  return 1;
}

/*********** SAX event handler ************ ud + string */

void f_CharData (void *ud, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, CharDataKey) && mpu->state == MPSok) {
    lua_pushlstring(mpu->L, s, len);
    docall(mpu, 1 + 1, 0);
  }
}

void f_Comment (void *ud, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, CommentKey)) {
    lua_pushlstring(mpu->L, s, len);
    docall(mpu, 1 + 1, 0);
  }
}

void f_StartElement (void *ud, const char *name, const char **attrs) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, StartElementKey)) {
    lua_State *L = mpu->L;
    lua_pushstring(L, name);
    lua_newtable(L);
    int i = 1;
    while (*attrs) {
      lua_pushinteger(L, i++);
      lua_pushstring(L, *attrs);
      lua_settable(L, -3);
      if (*(attrs + 1)) {
        lua_pushstring(L, *attrs++);
        lua_pushstring(L, *attrs++);
        lua_settable(L, -3);
      }
    }
    /* call function with self, name, and attributes */
    docall(mpu, 1 + 2, 0);
  }
}

void f_EndElement (void *ud, const char *name) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, EndElementKey)) {
    lua_pushstring(mpu->L, name);
    docall(mpu, 1 + 1, 0);
  }
}

/**************** required and their assistant ****************/

static int getcallbacks (lua_State *L) {
  lua_getuservalue(L, 1);
  return 1;
}

static int parse_aux (lua_State *L, lsmp_ud *mpu, const char *s, size_t len) {
  mpu->L = L;
  lua_settop(L, 2); /* ud + string */
  mpu->state = SML_Parse(mpu->parser, s, (int) len, s == NULL); /* state */
  if (mpu->state == MPSerror) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, mpu->errorref);  /* get original msg. */
    lua_error(L);
  }
  if (s == NULL) mpu->state = MPSfinished;
  if (mpu->state != MPSerror) {
    lua_settop(L, 1);  /* return parser userdata on success */
    return 1;
  }
  /* error */
  SML_Parser p = mpu->parser;
  lua_pushnil(L);
  lua_pushstring(L, SML_ErrorString(p));
  lua_pushinteger(L, SML_GetCurrentLineNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentColumnNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentByteIndex(p) + 1);
  return 5;
}

static int lsmp_pos (lua_State *L) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, 1, ParserType);
  SML_Parser p = mpu->parser;
  luaL_argcheck(L, p, 1, "parser is closed");
  lua_pushinteger(L, SML_GetCurrentLineNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentColumnNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentByteIndex(p) + 1);
  return 3;
}

static int lsmp_parse (lua_State *L) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, 1, ParserType);
  luaL_argcheck(L, mpu->parser, 1, "parser is closed");
  size_t len;
  const char *s = luaL_optlstring(L, 2, NULL, &len);
  if (mpu->state == MPSfinished && mpu->parser->mode == 0) {
    if (s) {
      lua_pushnil(L);
      lua_pushliteral(L, "cannot parse - document is finished");
      return 2;
    }
    lua_settop(L, 1);
    return 1;
  }
  return parse_aux(L, mpu, s, len);
}

static int lsmp_close (lua_State *L) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, 1, ParserType);
  int status = (mpu->state != MPSfinished) ? parse_aux(L, mpu, NULL, 0) : 1;

  luaL_unref(L, LUA_REGISTRYINDEX, mpu->errorref);
  mpu->errorref = LUA_REFNIL;
  if (mpu->parser) SML_ParserFree(mpu->parser);
  mpu->parser = NULL;

  if (status > 1) luaL_error(L, "error closing parser: %s", lua_tostring(L, 1 - status));
  lua_settop(L, 1);
  return 1;
}

static int lsmp_creator (lua_State *L) {
  lsmp_ud *mpu = (lsmp_ud *) lua_newuserdata(L, sizeof(lsmp_ud));
  luaL_getmetatable(L, ParserType);
  lua_setmetatable(L, -2);

  mpu->L = L;
  mpu->state = MPSpre;
  mpu->errorref = LUA_REFNIL;
  SML_Parser p = mpu->parser = SML_ParserCreate(NULL);
  if (!p) luaL_error(L, "SML_ParserCreate failed");

  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, 1);
  lua_setuservalue(L, -2);

  /* initialize sml parser */
  p->ud = mpu;
  lua_getfield(L, 1, "singletons");
  p->singletons = lua_tolstring(L, -1, NULL);
  lua_getfield(L, 1, "mode");
  p->mode = lua_tointeger(L, -1);
  p->r = p->c = p->i = 0;
  p->fd = f_CharData;
  p->fs = f_StartElement;
  p->fe = f_EndElement;
  p->fc = f_Comment;
  return 1;
}

static int lsmp_wraper (lua_State *L) {
  lua_remove(L, 1); /* pop the module table */
  return lsmp_creator(L);
}

static const struct luaL_Reg parser_meths[] = {
  {"parse", lsmp_parse},
  {"close", lsmp_close},
  {"pos", lsmp_pos},
  {"getcallbacks", getcallbacks},
  {"__gc", lsmp_close},
  {NULL, NULL}
};

static const struct luaL_Reg lsmp_funcs[] = {
  {"new", lsmp_creator}, /* cbt (callback table) */
  {NULL, NULL}
};

static const struct luaL_Reg lsmp_mt[] = {
  {"__call", lsmp_wraper}, /* lsmp, cbt */
  {NULL, NULL}
};

LUA_API int luaopen_lsmp ( lua_State *L ) {
  luaL_newmetatable(L, ParserType); /* parser object/userdata */
  luaL_setfuncs (L, parser_meths, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index"); /* merged into metatable */
  lua_pop (L, 1); /* remove parser type metatable */

  luaL_newlib(L, lsmp_funcs); /* the module table */
  luaL_newlib(L, lsmp_mt);    /* the module metatable */
  lua_pushstring(L, "Lua " LUA_VDIR); /* + compiling info */
  lua_setfield(L, -2, "__metatable");
  lua_setmetatable(L, -2);
  lua_pushstring(L, "MIT License (c) " __DATE__); /* + version */
  lua_setfield(L, -2, "lic");
  return 1;
}
