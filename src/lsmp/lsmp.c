/* lua simple/sloppy markup(x/html) parser
The first part is solely SML parser, it can be used without lua
The second part is for lua module, and shows a usage example of the first part
*/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lsmp.h"

#define GLNKSIZE    16

static Glnk *stackGlnk = NULL;
static Glnk *stGlnk = NULL;

static void stGlnkGrow () {
  Glnk *p = (Glnk *) malloc(sizeof(Glnk) * GLNKSIZE);
  p->next = stackGlnk; stackGlnk = p;
  (++p)->next = stGlnk;
  int c = GLNKSIZE - 1;
  while (--c) { p++; p->next = p - 1; }
  stGlnk = p;
}

static Glnk *stGlnkPop () {
  if (!stGlnk) stGlnkGrow();
  Glnk *p = stGlnk; stGlnk = stGlnk->next;
  return p;
}

static void stGlnkPush (Glnk *p) {
  free(p->data);
  p->data = NULL;
  p->next = stGlnk; stGlnk = p;
}

static void stGlnkFree () {
  /* while (stGlnk) { free(stGlnk->data); stGlnk = stGlnk->next; } */
  while (stackGlnk) { Glnk *p = stGlnk; stGlnk = stGlnk->next; free(p); }
}

// BNF:
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
//  CharData =
//  Comment =
//  Scheme =
//  Extension =
//  stack = {o} -- {{}}
//  singletons = "",
//
// NB: https://github.com/lunarmodules/luaexpat/blob/master/src/lxplib.c

SML_Parser SML_ParserCreate (void *ud) {
  SML_Parser p = (SML_Parser) malloc(sizeof(struct SML_ParserStruct));
  p->ud = ud;
  p->buf = NULL;
  p->len = p->size = p->r = p->c = p->i = 0;
  p->mode = M_STRICT;
  return p;
}

void SML_SetEncoding (SML_Parser p, const char *coding) {
  /* TODO */
}

const char *SML_ErrorString (SML_Parser p) {
  /* TODO */
  return NULL;
}

#define incr(x,p);  switch (*x++) { case '\n': p->r++; p->c = 0; default: p->c++; p->i++; }

/* heurestic smp (sloppy markup parser) */
/* markup <...> / text ... (!DOCTYPE [] scheme + quotation) */
enum MPState SML_Parse (SML_Parser p, const char *s, int len) {
  if (len && (p->mode & S_STATES) == S_DONE) return MPSerror;
  BYTE fEnd = (s == NULL);

  if (len) { /* merge/add to p->buf */
    if (p->size < p->len + len) p->buf = (char *) realloc(p->buf, p->size = p->len + len);
    memcpy(p->buf + p->len, s, len);
  }
  else if (!fEnd) {
    return MPSok;
  }

  /* adjust last parsed result */
  if (((p->mode & S_STATES) == S_TEXT) && p->len && p->buf[p->len] == '<') {
    p->len--;
    len++;
  }
  s = (const char *) p->buf;
  char *c = p->buf + p->len;
  char bc = p->len ? *(c - 1) : '\0';
  char *e = p->buf + (p->len += len);
  char q = p->quote;

  BYTE escape = p->mode & M_ESCAPE;
  BYTE sloppy = p->mode & M_SLOPPY;
  BYTE anytag = p->mode & M_ANYTAG; /* !DOCTYPE [] */
  // printf("%d (%x, %x, %x) %d\n", p->mode, s, c, e, len);

  while (c != e) {
    switch (p->mode & S_STATES) {
      case S_TEXT: /* text */
        while ((c != e) && (('<' != *c) ||
             (escape && (((c == s) && bc == '\\') || ((c != s) && *(c - 1) == '\\'))) ||
             ((c + 1 != e) && (bc = *(c + 1)) && sloppy && /* !good tag */
               !(bc == '!' || bc == '/' || bc == '?' || bc == '_' ||
                 (bc >= 'A' && bc <= 'Z') || (bc >= 'a' && bc <= 'z'))))) {
          incr(c, p);
        }
        if (c != e) {
          if (c != s) {
            /* p->ft(p->ud, s, c - s); */
            // printf("(%s)", strndup(s, c - s));
          }
          // printf("(%x, %x, %x)\n", s, c, e);
          s = (const char *) c;
          p->mode = (p->mode & M_MODES) | S_MARKUP;
        }
        else if (fEnd) {
          if (c != s) {
            /* p->ft(p->ud, s, c - s); */
            // printf("(%s)", s);
          }
          p->mode = (p->mode & M_MODES) | S_DONE;
        }
        break;

      case S_MARKUP: /* tag */
        // 1. comment or cdata -> S_CDATA
        // 2. token -> determine ending
        // 3. " -> S_STRING
        //
        //      '!'
        //          '[CDATA[' ==> S_CDATA
        //              ']]>' ==> S_TEXT
        //          '--' ==> S_CDATA
        //              '-->' ==> S_TEXT
        //      tokens: ==> S_MARKUP
        //          '"' ==> S_STRING ==> '"' ==> S_MARKUP
        //          '>' ==> S_TEXT
        //
        // <?php ?> Extensions
        if (p->mode & F_TOKEN) { /* token found */
          while (c != e) {
            if (*c == q || (!q && (*c == '"' || *c == '\''))) {
              // p->mode = (p->mode & M_MODES) | S_STRING;
              // break;
            }
            else if (*c == ' ') { /* space collect attributes */
              if (s != c) {
                Glnk *attr = stGlnkPop();
                attr->next = p->attr; p->attr = attr;
                attr->data = (void *) strndup(s, c - s);
              }
              s = (const char *) (c + 1);
            }
            else if (*c == '>') {
              if (p->elem[0] == '!') { /* <!.. ...> */
                /* p->fd(p->ud, s, c - s); */
              }
              else if (p->elem[0] == '/') { /* clean attribute */
                /* p->fe(p->ud, s); */
              }
              else { /* <*.. ...> */
                /* p->fs(p->ud, s, c - s); */
              }
              free(p->elem);
              p->elem = NULL; // TODO free szExts
              p->mode = (p->mode & M_MODES) | S_TEXT;
              incr(c, p);
              s = (const char *) c;
              break;
            }
            incr(c, p);
          }
        }
        else { /* search token */
          while (c != e) {
            if (((c == s + 3) && 0 == strncmp(s, "<!--", 4)) ||
                ((c == s + 8) && 0 == strncmp(s, "<![CDATA[", 9))) {
              p->mode = (p->mode & M_MODES) | S_CDATA;
              incr(c, p);
              s = (const char *) c;
              printf("COMMENT\n");
              break;
            }
            else if ((bc = *c) && (bc <= ' ' || (bc > '~' && bc <= 0x7F))) { /* space */
              int i, n = c - s;
              for (i = 0; i < p->Exts; i++) if (strncmp(s, p->szExts[2 * i], n) == 0) break;
              p->iExt = i;
              p->elem = strndup(++s, c - s);
              if (i < p->Exts) {
                p->mode = (p->mode & M_MODES) | S_CDATA;
              }
              else { /* TODO tag */
                p->mode |= F_TOKEN;
                if (anytag) {
                }
              }
              incr(c, p);
              s = (const char *) c;
              printf("TOKEN %s\n", p->elem);
              break;
            }
            else if (bc == '>') {
              if (c == ++s) { /* TODO <> */
                p->mode = (p->mode & M_MODES) | S_TEXT;
              }
              else { /* TODO <*> */
                p->mode |= F_TOKEN;
                p->elem = strndup(s, c - s);
                printf("TOKEN %s\n", p->elem);
              }
              incr(c, p);
              s = (const char *) c;
              break;
            }
            if (*(c++) == '\n') { p->r++; p->c = 0; }
            p->c++;
            p->i++;
          }
        }
        break;

      case S_STRING: /* string in tag */
        while (c != e && *c != q) {
          incr(c, p);
        }
        p->mode = (p->mode & M_MODES) | S_MARKUP;
        break;

      case S_CDATA: /* CDATA, COMMENT, and other Extensions */
        while (c != e) {
          if (*c == '>') {
            if (p->elem) {
              // if (0 == strncmp(c - 2, p->szExts[p->iExt], 3)) {
              //   p->fx(p->ud, s, c - s); /* TODO extension */
              //   break;
              // }
            }
            else if (0 == strncmp(c - 2, "]]>", 3)) {
              // p->ft(p->ud, s, c - s); /* cdata text */
              break;
            }
            else if ((0 == strncmp(c - 2, "-->", 3)) && c > s + 7 ) {
              // p->fc(p->ud, s, c - s); /* comment */
              break;
            }
          }
          incr(c, p);
        }
        if (c != e) {
          p->mode = (p->mode & M_MODES) | S_TEXT;
          incr(c, p);
          s = (const char *) c;
        }
        break;
    }
  }

  if (fEnd) {
    p->mode = (M_MODES & p->mode) | ((c == e) ? S_DONE : S_ERROR);
    return (c == e) ? MPSfinished : MPSerror;
  }
  return MPSok;
}

void SML_ParserFree (SML_Parser p) {
  free(p->buf);
  if (p->szExts) { free((void *)(*(p->szExts))); free(p->szExts); }
  free(p);
}

int SML_szArray (const char ***psz, const char *sz) {
  int c = 0;
  if (sz) {
    char *s = (char *) sz;
    while (*s) {
      if (*s <= ' ' || *s > '~') c++;
      s++;
    }
    char **p = (char **) malloc((size_t) ++c);
    *psz = (const char **) p;
    *p++ = s = (char *) malloc((size_t) (s - sz));
    while ((*s++ = *sz)) {
      if (*sz <= ' ' || *sz > '~') *((*p++ = s) - 1) = '\0';
      sz++;
    }
  }
  else {
    *psz = NULL;
  }
  return (c >> 1); /* div by 2 for pairs */
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
#define lua_pushliteral(L, s)    lua_pushstring(L, "" s, (sizeof(s)/sizeof(char))-1)
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
    luaL_error(L, "lsmp '%s' callback is not a function", handle);
  }
  lua_pushvalue(L, 1);  /* 1st arg (ud) in every call (self) */
  return 1;
}

/*********** SAX event handler ************ ud + string */

void f_CharData (void *ud, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  printf("OK here\n");
  if (getHandle(mpu, CharacterDataKey) && mpu->state == MPSok) {
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

void f_Extension (void *ud, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, ExtensionKey)) {
    lua_pushlstring(mpu->L, s, len);
    docall(mpu, 1 + 1, 0);
  }
}

void f_Scheme (void *ud, const char *name, const char **attrs) {
  stGlnkPush(NULL);
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
  mpu->state = SML_Parse(mpu->parser, s, (int) len); /* state */
  if (mpu->state == MPSerror) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, mpu->errorref);  /* get original msg. */
    lua_error(L);
  }
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
  if (mpu->state == MPSfinished) {
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
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, 1);
  lua_setuservalue(L, -2);

  mpu->L = L;
  mpu->state = MPSpre;
  mpu->errorref = LUA_REFNIL;
  SML_Parser p = mpu->parser = SML_ParserCreate(mpu);
  if (!p) luaL_error(L, "SML_ParserCreate failed");

  /* initialize sml parser */
  lua_getfield(L, 1, "mode");
  p->mode = (lua_tointeger(L, -1) & M_MODES) | S_TEXT;
  lua_remove(L, -1);
  lua_getfield(L, 1, "extension");
  p->Exts = (BYTE) SML_szArray(&(p->szExts), lua_tolstring(L, -1, NULL));
  lua_remove(L, -1);
  p->ft = f_CharData;
  p->fs = f_StartElement;
  p->fe = f_EndElement;
  p->fc = f_Comment;
  p->fd = f_Scheme;
  p->fx = f_Extension;
  return 1;
}

static int lsmp_wraper (lua_State *L) {
  lua_remove(L, 1); /* pop the module table */
  return lsmp_creator(L);
}

static int lsmp_gc (lua_State *L) {
  stGlnkFree();
  return 0;
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
  {"__gc", lsmp_gc}, /* clean stack */
  {NULL, NULL}
};

LUA_API int luaopen_lsmp (lua_State *L) {
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
