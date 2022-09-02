// lua simple markup(x/html) parser
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lsmp.h"

#include "lua.h"
#include "lauxlib.h"

#if (LUA_VERSION_NUM >= 54)
#define lua_newuserdata(L, u)  lua_newuserdatauv(L, u, 1)
#define lua_setuservalue(L, i) lua_setiuservalue(L, i, 1)
#define lua_getuservalue(L, i) lua_getiuservalue(L, i, 1)
#endif

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
//  CharacterData = mode < 0 and text or cleantext,
//  Comment = mode < 1 and comment or nil,
//  mode = 0, -- 0/strict, 1/non-strict, 2/sloppy, 3/allpass (_nonstrict = true)
//  stack = {o} -- {{}}
//
//
// NB: https://github.com/lunarmodules/luaexpat/blob/master/src/lxplib.c



SML_Parser SML_ParserCreate (void *p) {

}

enum MPState {  // state machin
  MPSpre,       // initialized
  MPSok,        // state while parsing
  MPSfinished,  // state after finished parsing
  MPSerror,
  MPSstring     // state while reading a string
};

struct lsmp_ud {
  lua_State *L;
  SML_Parser parser;    // associated lsmp
  int errorref;         // reference to error message
  enum MPState state;
  luaL_Buffer *b;       // to concatenate sequences of cdata pieces
  int fNoBuffer;        // whether to buffer cdata pieces
};

static int hasfield (lua_State *L, const char *fname) {
  lua_pushstring(L, fname);
  lua_gettable(L, 1);
  int res = !lua_isnil(L, -1);
  lua_pop(L, 1);
  return res;
}

// Check whether there is a Lua handle for a given event: If so,
// put it on the stack (to be called later), and also push `self'
static int getHandle (lsmp_ud *mpu, const char *handle) {
  lua_State *L = xpu->L;
  if (xpu->state == XPSstring) dischargestring(xpu);
  if (xpu->state == XPSerror)
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


// Auxiliary function to call a Lua handle
static void docall (lsmp_ud *mpu, int nargs, int nres) {
  lua_State *L = xpu->L;
  assert(xpu->state == XPSok);
  if (lua_pcall(L, nargs + 1, nres, 0) != 0) {
    xpu->state = XPSerror;
    xpu->errorref = luaL_ref(L, LUA_REGISTRYINDEX);  /* error message */
  }
}


static lsmp_ud *checkparser (lua_State *L, int idx) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, idx, ParserType);
  luaL_argcheck(L, mpu, idx, "lsmp expected");
  luaL_argcheck(L, mpu->parser, idx, "parser is closed");
  return mpu;
}

// Check whether there is pending Cdata, and call its handle if necessary
static void dischargestring (lxp_userdata *xpu) {
  assert(xpu->state == XPSstring);
  xpu->state = XPSok;
  luaL_pushresult(xpu->b);
  docall(xpu, 1, 0);
}

static void f_CharData (void *ud, const char *s, int len) {
  lxp_userdata *xpu = (lxp_userdata *)ud;
  if (xpu->state == XPSok) {
    if (getHandle(xpu, CharDataKey) == 0) return;  /* no handle */
    if(xpu->bufferCharData != 0) {
      xpu->state = XPSstring;
      luaL_buffinit(xpu->L, xpu->b);
    } else {
      lua_pushlstring(xpu->L, s, len);
      docall(xpu, 1, 0);
    }
  }
  if (xpu->state == XPSstring)
    luaL_addlstring(xpu->b, s, len);
}


static void f_Comment (void *ud, const char *data) {
  lxp_userdata *xpu = (lxp_userdata *)ud;
  if (getHandle(xpu, CommentKey) == 0) return;  /* no handle */
  lua_pushstring(xpu->L, data);
  docall(xpu, 1, 0);
}

static void f_StartElement (void *ud, const char *name, const char **attrs) {
  lsmp_ud *mpu = (lsmp_ud *)ud;
  lua_State *L = mpu->L;
  int lastspec = XML_GetSpecifiedAttributeCount(xpu->parser) / 2;
  int i = 1;
  if (getHandle(xpu, StartElementKey) == 0) return;  /* no handle */
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
  docall(xpu, 2, 0);  /* call function with self, name, and attributes */
}


static void f_EndElement (void *ud, const char *name) {
  lxp_userdata *xpu = (lxp_userdata *)ud;
  if (getHandle(xpu, EndElementKey) == 0) return;  /* no handle */
  lua_pushstring(xpu->L, name);
  docall(xpu, 1, 0);
}

static int parser_gc (lua_State *L) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, 1, ParserType);
  luaL_argcheck(L, mpu, 1, "expat parser expected");
  lsmpclose(L, xpu);
  return 0;
}

static int getcallbacks (lua_State *L) {
  checkparser(L, 1);
  lua_getuservalue(L, 1);
  return 1;
}

static int reporterror (lsmp_ud *mpu) {
  lua_State *L = mpu->L;
  SML_Parser p = mpu->parser;
  lua_pushnil(L); // status
  lua_pushstring(L, SML_ErrorString(p));
  lua_pushinteger(L, p->r + 1);
  lua_pushinteger(L, p->c + 1);
  lua_pushinteger(L, p->i + 1);
  return 5;
}

static int parse_aux (lua_State *L, lsmp_ud *mpu, const char *s, size_t len) {
  luaL_Buffer b;
  mpu->L = L;
  mpu->state = XPSok;
  mpu->b = &b;
  lua_settop(L, 2);
  getcallbacks(L);
  int status = SML_Parse(mpu->parser, s, (int)len, s == NULL);
  if (mpu->state == XPSstring) dischargestring(xpu);
  if (mpu->state == XPSerror) {  /* callback error? */
    lua_rawgeti(L, LUA_REGISTRYINDEX, mpu->errorref);  /* get original msg. */
    lua_error(L);
  }
  if (s == NULL) mpu->state = XPSfinished;
  if (status) {
    lua_settop(L, 1);  /* return parser userdata on success */
    return 1;
  }
  return reporterror(mpu); /* error */
}

static void lsmpclose (lua_State *L, lsmp_ud *mpu) {
  luaL_unref(L, LUA_REGISTRYINDEX, mpu->errorref);
  mpu->errorref = LUA_REFNIL;
  if (mpu->parser) SML_ParserFree(xpu->parser);
  mpu->parser = NULL;
}

static int lsmp_pos (lua_State *L) {
  lxp_userdata *xpu = checkparser(L, 1);
  XML_Parser p = xpu->parser;
  lua_pushinteger(L, SML_GetCurrentLineNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentColumnNumber(p) + 1);
  lua_pushinteger(L, SML_GetCurrentByteIndex(p) + 1);
  return 3;
}

static int lsmp_getcurrentbytecount (lua_State* L) {
  lsmp_ud *mpu = checkparser(L, 1);
  lua_pushinteger(L, XML_GetCurrentByteCount(xpu->parser));
  return 1;
}

static int lsmp_parse (lua_State *L) {
  lsmp_ud *mpu = checkparser(L, 1);
  size_t len;
  const char *s = luaL_optlstring(L, 2, NULL, &len);
  if (mpu->state == MPSfinished) {
    if (s) {
      lua_pushnil(L);
      lua_pushliteral(L, "cannot parse - document is finished");
      return 2;
    }
    lua_settop(L, 1);
    return 1;
  }
  return parse_aux(L, xpu, s, len);
}

static int lsmp_close (lua_State *L) {
  lsmp_ud *mpu = (lsmp_ud *) luaL_checkudata(L, 1, ParserType);
  luaL_argcheck(L, mpu, 1, "expat parser expected");
  int status = (mpu->state != XPSfinished) ? parse_aux(L, mpu, NULL, 0) : 1;
  lsmpclose(L, xpu);
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
  mpu->fNoBufferCharData = lua_toboolean(L, 2);
  SML_Parser p = mpu->parser = SML_ParserCreate(NULL);
  if (!p) luaL_error(L, "SML_ParserCreate failed");

  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, 1);
  lua_setuservalue(L, -2);

  SML_SetUserData(p, mpu);
  if (hasfield(L, CharDataKey))
    SML_SetCharacterDataHandler(p, f_CharData);
  if (hasfield(L, StartElementKey) || hasfield(L, EndElementKey))
    SML_SetElementHandler(p, f_StartElement, f_EndElement);
  if (hasfield(L, CommentKey))
    SML_SetCommentHandler(p, f_Comment);

  return 1;
}

static int lsmp_wraper (lua_State *L) { // wrapper
  lua_remove(L, 1); // pop the module table
  return lsmp_creator(L);
}

static const struct luaL_Reg parser_meths[] = {
  {"parse", lsmp_parse},
  {"close", lsmp_close},
  {"pos", lsmp_pos},
  {"getcallbacks", getcallbacks},
  {"__gc", parser_gc},
  {NULL, NULL}
};

static const struct luaL_Reg lsmp_funcs[] = {
  {"new", lsmp_creator}, // cbt (callback table)
  {NULL, NULL}
};

static const struct luaL_Reg lsmp_mt[] = { // metatable
  {"__call", lsmp_wraper}, // lsmp, cbt
  {NULL, NULL}
};

LUA_API int luaopen_lsmp ( lua_State *L ) {
  luaL_newmetatable(L, ParserType); // parser object/userdata
  luaL_setfuncs (L, parser_meths, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index"); // merge methods into metatable
  lua_pop (L, 1); // remove parser type metatable

  luaL_newlib(L, lsmp_funcs); // the module table
  luaL_newlib(L, lsmp_mt);    // the module metatable
  lua_pushstring(L, "MIT (c) Josh Feng " __DATE__);
  lua_setfield(L, -2, "__metatable");
  lua_setmetatable(L, -2);
  return 1;
}
