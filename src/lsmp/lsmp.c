// lua simple markup(x/html) parser
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

enum MPState {  // state machin
  MPSpre,       // initialized
  MPSok,        // state while parsing
  MPSfinished,  // state after finished parsing
  MPSerror,
  MPSstring     // state while reading a string
};

SML_Parser SML_ParserCreate (void *p) {
}

/*
SML_ParserCreate
SML_SetCharDataHdlr
SML_SetCommentHdlr
SML_SetElementHdlr
SML_SetUserData
SML_Parse
SML_ErrorString
SML_ParserFree
*/

/*************** lua library related ***************/

#include "lua.h"
#include "lauxlib.h"

#if (LUA_VERSION_NUM >= 54)
#define lua_newuserdata(L, u)  lua_newuserdatauv(L, u, 1)
#define lua_setuservalue(L, i) lua_setiuservalue(L, i, 1)
#define lua_getuservalue(L, i) lua_getiuservalue(L, i, 1)
#endif


#if !defined(lua_pushliteral)
#define  lua_pushliteral(L, s)	\
	lua_pushstring(L, "" s, (sizeof(s)/sizeof(char))-1)
#endif

#define ParserType	"MarkupParser"

/* lua full userdata */
typedef struct lsmp_userdata {
  lua_State *L;
  SML_Parser parser;    /* associated lsmp */
  int errorref;         /* reference to error message */
  enum MPState state;
  luaL_Buffer *b;       /* to concatenate sequences of cdata pieces */
  int fNoBuffer;        /* whether to buffer cdata pieces */
  int mode; /* TODO */
} lsmp_ud;

// Auxiliary function to call a Lua handle
static void docall (lsmp_ud *mpu, int nargs, int nres) {
  lua_State *L = mpu->L;
  assert(mpu->state == MPSok);
  if (lua_pcall(L, nargs + 1, nres, 0) != 0) {
    mpu->state = MPSerror;
    mpu->errorref = luaL_ref(L, LUA_REGISTRYINDEX);  /* error message */
  }
}

/* Check whether there is pending Cdata, and call its handle if necessary */
static void dischargestring (lsmp_ud *mpu) {
  assert(mpu->state == MPSstring);
  mpu->state = MPSok;
  luaL_pushresult(mpu->b);
  docall(mpu, 1, 0);
}

// Check whether there is a Lua handle for a given event: If so,
// put it on the stack (to be called later), and also push `self'
static int getHandle (lsmp_ud *mpu, const char *handle) {
  lua_State *L = mpu->L;
  if (mpu->state == MPSstring) dischargestring(mpu);
  if (mpu->state == MPSerror)
    return 0;  /* some error happened before; skip all handles */
  lua_pushstring(L, handle);
  lua_gettable(L, 3);
  if (lua_toboolean(L, -1) == 0) {
    lua_pop(L, 1);
    return 0;
  }
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "lxp '%s' callback is not a function", handle);
  }
  lua_pushvalue(L, 1);  /* first argument in every call (self) */
  return 1;
}

/*********** SAX event driven ***********/

SML_CharDataHdlr f_CharData (void *ud, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (mpu->state == MPSok) {
    if (getHandle(mpu, CharDataKey) == 0) return;  /* no handle */
    if (mpu->fNoBuffer == 0) {
      mpu->state = MPSstring;
      luaL_buffinit(mpu->L, mpu->b);
    } else {
      lua_pushlstring(mpu->L, s, len);
      docall(mpu, 1, 0);
    }
  }
  if (mpu->state == MPSstring) luaL_addlstring(mpu->b, s, len);
}

SML_CommentHdlr f_Comment (void *ud, const char *data) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, CommentKey) == 0) return;  /* no handle */
  lua_pushstring(mpu->L, data);
  docall(mpu, 1, 0);
}

SML_StartElementHdlr f_StartElement (void *ud, const char *name, const char **attrs) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  lua_State *L = mpu->L;
  int lastspec = XML_GetSpecifiedAttributeCount(mpu->parser) / 2;
  int i = 1;
  if (getHandle(mpu, StartElementKey) == 0) return;  /* no handle */
  lua_pushstring(L, name);
  lua_newtable(L);
  while (*attrs) {
    if (i <= lastspec) {
      lua_pushinteger(L, i++);
      lua_pushstring(L, *attrs);
      lua_settable(L, -3);
    }
    lua_pushstring(L, *attrs++);
    lua_pushstring(L, *attrs++);
    lua_settable(L, -3);
  }
  docall(mpu, 2, 0);  /* call function with self, name, and attributes */
}

SML_EndElementHdlr f_EndElement (void *ud, const char *name) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, EndElementKey) == 0) return;  /* no handle */
  lua_pushstring(mpu->L, name);
  docall(mpu, 1, 0);
}


/**************** required and their assistant ****************/

static lsmp_ud *checkparser (lua_State *L, int idx) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, idx, ParserType);
  luaL_argcheck(L, mpu, idx, "lsmp expected");
  luaL_argcheck(L, mpu->parser, idx, "parser is closed");
  return mpu;
}

static int getcallbacks (lua_State *L) {
  checkparser(L, 1);
  lua_getuservalue(L, 1);
  return 1;
}

static int parse_aux (lua_State *L, lsmp_ud *mpu, const char *s, size_t len) {
  luaL_Buffer b;
  mpu->L = L;
  mpu->state = MPSok;
  mpu->b = &b;
  lua_settop(L, 2);
  getcallbacks(L);
  int status = SML_Parse(mpu->parser, s, (int)len, s == NULL);
  if (mpu->state == MPSstring) dischargestring(mpu);
  if (mpu->state == MPSerror) {  /* callback error? */
    lua_rawgeti(L, LUA_REGISTRYINDEX, mpu->errorref);  /* get original msg. */
    lua_error(L);
  }
  if (s == NULL) mpu->state = MPSfinished;
  if (status) {
    lua_settop(L, 1);  /* return parser userdata on success */
    return 1;
  }
  /* error */
  SML_Parser p = mpu->parser;
  lua_pushnil(L); /* status */
  lua_pushstring(L, SML_ErrorString(p));
  lua_pushinteger(L, SML_GetCurrentLineNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentColumnNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentByteIndex(p) + 1);
  return 5;
}

static int lsmp_pos (lua_State *L) {
  lsmp_ud *mpu = checkparser(L, 1);
  SML_Parser p = mpu->parser;
  lua_pushinteger(L, SML_GetCurrentLineNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentColumnNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentByteIndex(p) + 1);
  return 3;
}

static int lsmp_parse (lua_State *L) {
  lsmp_ud *mpu = checkparser(L, 1);
  size_t len;
  const char *s = luaL_optlstring(L, 2, NULL, &len);
  if (mpu->state == MPSfinished && mpu->mode == 0) {
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

static int lsmp_close (lua_State *L) { /* TODO */
  /* lsmp_ud *mpu = (lsmp_ud *) luaL_testudata(L, 1, ParserType); */
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, 1, ParserType);
  luaL_argcheck(L, mpu, 1, "expat parser expected");
  int status = (mpu->state != MPSfinished) ? parse_aux(L, mpu, NULL, 0) : 1;

  /* lsmpclose */
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
  mpu->fNoBuffer= lua_toboolean(L, 2);
  SML_Parser p = mpu->parser = SML_ParserCreate(NULL);
  if (!p) luaL_error(L, "SML_ParserCreate failed");

  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, 1);
  lua_setuservalue(L, -2);
  /* TODO set other parser options */
  /*
  lua_getfield(L, 1, "singleton");
  lua_getfield(L, 1, "mode");
  */

  SML_SetUserData(p, mpu);
  SML_SetCharDataHdlr(p, f_CharData);
  SML_SetElementHdlr(p, f_StartElement, f_EndElement);
  SML_SetCommentHdlr(p, f_Comment);
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
  lua_pushstring(L, "MIT (c) Josh Feng " __DATE__);
  lua_setfield(L, -2, "__metatable");
  lua_setmetatable(L, -2);
  return 1;
}
