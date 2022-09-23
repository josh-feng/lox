/* lua simple/sloppy markup(x/html) parser
** Josh Feng (C) MIT license 2022
** The first part is solely an SML parser in C.
** The second part is a lua module, as a coding example of the first part
*/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lsmp.h"

#ifdef DEBUG
#define DBG(l,x);  if (DEBUG >= l) {x}
#else
#define DBG(l,x);
#endif

const char *SML_ErrorString[] = {
  "OK", /* OK */
};

/* general link {{{ */
static Glnk *stackGlnk = NULL;
static Glnk *GlnkTop = NULL;

#define GLNKINCRSIZE    16

static void stGlnkGrow () {
  Glnk *p = (Glnk *) malloc(sizeof(Glnk) * GLNKINCRSIZE);
  p->next = stackGlnk; stackGlnk = p;
  (++p)->next = GlnkTop;
  int c = GLNKINCRSIZE - 1;
  while (--c) { p++; p->next = p - 1; }
  GlnkTop = p;
}

static Glnk *stGlnkPop () {
  if (!GlnkTop) stGlnkGrow();
  Glnk *p = GlnkTop; GlnkTop = GlnkTop->next;
  return p;
}

static void stGlnkPush (Glnk *p) {
  p->next = GlnkTop; GlnkTop = p;
}

static void stGlnkFree () {
  while (stackGlnk) {
    Glnk *p = stackGlnk;
    stackGlnk = stackGlnk->next;
    free(p);
  }
}
/* }}} */

/* process instruction p = lsmp.new(callbacks) {{{
assert(p:parse[[<to><?lua how is this passed to <here>? ?></to>]])
new --> parser
  Scheme = scheme,
  StartElement = starttag,
  EndElement = endtag,
  CharData = text,
  Comment = comment,
  Extension = extension,
  mode = flags,
  ext = "<?php ?>",
  stack = {o} -- {{}}
*/

SML_Parser SML_ParserCreate (void *ud, int mode, const char *ext) {
  SML_Parser p = (SML_Parser) malloc(sizeof(struct SML_ParserStruct));
  if (!p) return p;
  p->ud = ud;
  p->buf = NULL;
  p->len = p->size = p->r = p->c = p->i = p->n = 0;
  p->mode = (mode & M_MODES) | S_TEXT;
  p->quote = '\0';

  int c = 0; /* prepare extension start/end token arrays */
  if (ext) {
    char *s = (char *) ext;
    while (*s) {
      if (*s <= ' ' || *s > '~') c++;
      s++;
    }
    char **psz = (char **) malloc((size_t) ++c);
    p->szExts = (const char **) psz;
    *psz++ = s = (char *) malloc((size_t) (s - ext) + 1);
    while ((*s++ = *ext)) {
      if (*ext <= ' ' || *ext > '~') *((*psz++ = s) - 1) = '\0';
      ext++;
    }
    p->Exts = c >>= 1;
    p->lszExts = (char *) malloc(c);
    while (c--)
    p->lszExts[c] = (char) strlen(p->szExts[2 * c + 1]);
  }
  else {
    p->szExts = NULL;
    p->lszExts = NULL;
    p->Exts = 0;
  }

  p->elem = NULL;
  p->attr = NULL;
  p->level = 0; /* < <* .. > > */
  return p;
}

void SML_ParserFree (SML_Parser p) {
  free(p->buf);
  if (p->szExts) {
    free((void *)(*(p->szExts)));
    free(p->szExts);
    free(p->lszExts);
  }
  free(p);
}

static const char **SML_attr (SML_Parser p) { /* n + 1(NULL) */
  int c = 0;
  const char **szAttr;
  Glnk *attr = p->attr;
  while (attr) { c++; attr = attr->next; }
  szAttr = (const char **) malloc(sizeof(char *) * (c + 1));
  szAttr[c--] = (const char *) NULL;
  attr = p->attr;
  while (attr) {
    szAttr[c--] = (const char *) attr->data; /* key (+ value) */
    Glnk *tmp = attr; attr = attr->next; stGlnkPush(tmp);
  }
  p->attr = NULL;
  return szAttr;
} /* }}} */

#define incr(x,p);  switch (*x++) { case '\n': p->n = p->c; p->r++; p->c = 0; default: p->c++; p->i++; }

const char *mus = "<"; /* markup start */
const char *mue = ">"; /* markup end */

/* heurestic smp (sloppy markup parser) {{{ */
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
    if (p->c) { p->c--; } else { p->r--; p->c = p->n; } /* or col of last line */
    p->i--;
  }
  s = (const char *) p->buf;
  char *c = p->buf + p->len;
  char bc = p->len ? *(c - 1) : '\0'; /* character before c */
  char *e = p->buf + (p->len += len);
  char q = p->quote;

  BYTE escape = p->mode & M_ESCAPE;
  BYTE sloppy = p->mode & M_SLOPPY;
  DBG(2, printf("%d (%x, %x, %x) %d\n", p->mode, s, c, e, len););

  while (c != e) {
    switch (p->mode & S_STATES) {
      case S_TEXT: /* text {{{ */
        while ((c != e) && (('<' != *c) ||
             (escape && (((c == s) && bc == '\\') || ((c != s) && *(c - 1) == '\\'))) ||
             ((c + 1 != e) && (bc = *(c + 1)) && sloppy && /* !good tag */
               !(bc == '!' || bc == '/' || bc == '?' || bc == '_' ||
                 (bc >= 'A' && bc <= 'Z') || (bc >= 'a' && bc <= 'z'))))) {
          incr(c, p);
        }
        if (c != e) { /* found markup < */
          if (c != s) p->ft(p->ud, s, c - s); /* release text */
          s = (const char *) c; /* shift start-pointer */
          p->mode = (p->mode & M_MODES) | S_MARKUP;
        }
        else if (fEnd) {
          if (c != s) p->ft(p->ud, s, c - s);
          p->mode = (p->mode & M_MODES) | S_DONE;
        }
        break; /* text }}} */

      case S_MARKUP: /* markup {{{ */
        /* 1. comment or cdata -> S_CDATA
        ** 2. token -> determine ending
        ** 3. " -> S_STRING
        **      '!'
        **          '[CDATA[' ==> S_CDATA
        **              ']]>' ==> S_TEXT
        **          '--' ==> S_CDATA
        **      tokens: ==> S_MARKUP
        **          '"' ==> S_STRING ==> '"' ==> S_MARKUP
        **          '>' ==> S_TEXT
        ** <?php ?> Extensions
        */
        if (p->mode & F_TOKEN) { /* token found */
          while (c != e) {
            if (*c == q || (!q && (*c == '"' || *c == '\''))) {
              p->mode = (p->mode & M_MODES) | S_STRING;
              p->quote = q = *c;
              incr(c, p);
              break;
            }
            else if (*c <= ' ' || *c == '<') { /* < or space */
              BYTE opening = 0;
              Glnk *attr;
              if (*c == '<') {
                p->level++;
                opening = 1;
              }
              if (s != c) { /* collect attributes */
                attr = stGlnkPop(); attr->next = p->attr; p->attr = attr;
                attr->data = (void *) s; *c = '\0';
              }
              if (opening) {
                attr = stGlnkPop(); attr->next = p->attr; p->attr = attr;
                attr->data = (void *) mus;
              }
              s = (const char *) c + 1;
            }
            else if (*c == '>') {
              BYTE closing = 0;
              Glnk *attr;
              if (p->level > 0) {
                p->level--;
                if (s != c) { /* collect attributes */
                  attr = stGlnkPop(); attr->next = p->attr; p->attr = attr;
                  attr->data = (void *) s; *c = '\0';
                }
                attr = stGlnkPop(); attr->next = p->attr; p->attr = attr;
                attr->data = (void *) mue;
                s = (const char *) c + 1;
              }
              else { /* closing */
                bc = p->elem[0];

                if (s != c && s != p->elem) { /* last attr */
                  *c = '\0';
                  if (bc == '?' && *(c - 1) == '?') {
                    *(c - 1) = '\0';
                  }
                  else if (((bc == '_') ||
                           ((bc >= 'a') && (bc <= 'z')) ||
                           ((bc >= 'A') && (bc <= 'Z'))) &&
                           *(c - 1) == '/') {
                    *(c - 1) = '\0';
                    closing = 0x01; /* also closing */
                  }
                  if (*s != '\0') {
                    attr = stGlnkPop(); attr->next = p->attr; p->attr = attr;
                    attr->data = (void *) s;
                  }
                }

                if (bc == '/') { /* clean attribute */
                  attr = p->attr;
                  while (attr) { /* clean attr or error if strict */
                    Glnk *tmp = attr; attr = attr->next; stGlnkPush(tmp);
                  }
                  p->fe(p->ud, p->elem);
                }
                else if ((bc != '_') &&
                        !((bc >= 'a') && (bc <= 'z')) &&
                        !((bc >= 'A') && (bc <= 'Z'))) {
                  /* scheme/definition/declaration <!.. ...> <?.. ...> etc */
                  p->fd(p->ud, p->elem, SML_attr(p));
                }
                else { /* regular tag <*.. ...> */
                  p->fs(p->ud, p->elem, SML_attr(p));
                  if (closing) p->fe(p->ud, p->elem);
                }
                free(p->elem);
                p->elem = NULL;
                p->mode = (p->mode & M_MODES) | S_TEXT;
                incr(c, p);
                s = (const char *) c;
                break;
              }
            }
            incr(c, p);
          }
        }
        else { /* searching token */
          while (c != e) {
            if (((c == s + 3) && 0 == strncmp(s, "<!--", 4)) ||
                ((c == s + 8) && 0 == strncmp(s, "<![CDATA[", 9))) {
              p->mode = (p->mode & M_MODES) | S_CDATA;
              incr(c, p);
              s = (const char *) c;
              break;
            }
            else if ((bc = *c) && (bc <= ' ' || bc == 0x7F)) { /* space or del */
              int i, n = c - s++;
              for (i = 0; i < p->Exts; i++) if (strncmp(s, p->szExts[2 * i], n) == 0) break;
              p->iExt = i;
              p->elem = strndup(s, c - s);
              if (i < p->Exts) {
                p->mode = (p->mode & M_MODES) | S_CDATA;
              }
              else {
                p->mode |= F_TOKEN;
              }
              incr(c, p);
              s = (const char *) c;
              DBG(1, printf("TOKEN1 (%s)%x\n", p->elem, p->mode););
              break;
            }
            else if (bc == '>') {
              if (c == ++s) { /* <> */
                p->mode = (p->mode & M_MODES) | S_TEXT;
                incr(c, p);
              }
              else { /* <*> */
                p->mode |= F_TOKEN;
                p->elem = strndup(s, c - s);
                DBG(1, printf("TOKEN2 (%s)%x\n", p->elem, p->mode););
              }
              s = (const char *) c;
              break;
            }
            if (*(c++) == '\n') { p->n = p->c; p->r++; p->c = 0; }
            p->c++;
            p->i++;
          }
        }
        break; /* markup }}} */

      case S_STRING: /* string in tag {{{ */
        while (c != e && (*c != q || *(c - 1) == '\\')) {
          incr(c, p);
        }
        if (c != e) {
          p->mode = (p->mode & M_MODES) | S_MARKUP | F_TOKEN;
          p->quote = q = '\0'; /* reset quote */
          incr(c, p);
        }
        break; /* string in tag }}} */

      case S_CDATA: /* CDATA, COMMENT, and other Extensions {{{ */
        while (c != e) {
          if (*c == '>') {
            if (p->elem) { /* ext tag */
              int l = p->lszExts[p->iExt];
              if (0 == strncmp(c - l + 1, p->szExts[p->iExt], l)) {
                *(c - l + 1) = '\0';
                p->fx(p->ud, p->elem, s, c - l + 1 - s);
                break;
              }
            }
            else if (0 == strncmp(c - 2, "]]>", 3)) {
              *(c - 2) = '\0';
              p->ft(p->ud, s, c - 2 - s); /* cdata text */
              break;
            }
            else if ((0 == strncmp(c - 2, "-->", 3)) && c > s + 7 ) {
              *(c - 2) = '\0';
              p->fc(p->ud, s, c - 2 - s); /* comment */
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
        break; /* CDATA, COMMENT, and other Extensions }}} */
    }
  }

  if (fEnd) {
    p->mode = (M_MODES & p->mode) | ((c == e) ? S_DONE : S_ERROR);
    return (c == e) ? MPSfinished : MPSerror;
  }
  return MPSok;
} /* }}} */

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
  lua_gettable(L, 2);
  if (lua_isnil(L, -1)) return 0;
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "lsmp '%s' callback is not a function", handle);
  }
  lua_pushvalue(L, 1);  /* 1st arg (ud) in every call (self) */
  return 1;
}

/*********** SAX event handler ************ ud + string {{{ */

void f_CharData (void *ud, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
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

void f_Extension (void *ud, const char *name, const char *s, int len) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, ExtensionKey)) {
    lua_pushstring(mpu->L, name);
    lua_pushlstring(mpu->L, s, len);
    docall(mpu, 1 + 2, 0);
  }
}

void f_Scheme (void *ud, const char *name, const char **attrs) {
  lsmp_ud *mpu = (lsmp_ud *) ud;
  if (getHandle(mpu, SchemeKey)) {
    lua_State *L = mpu->L;
    lua_pushstring(L, name);
    lua_newtable(L);
    int i = 1;
    while (*attrs) {
      lua_pushinteger(L, i++);
      lua_pushstring(L, *(attrs++));
      lua_settable(L, -3); /* leave lua callback to parse attr */
    }
    /* call function with self, name, and attributes */
    docall(mpu, 1 + 2, 0);
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
      lua_pushstring(L, *(attrs++));
      lua_settable(L, -3); /* leave lua callback to parse attr */
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
} /* }}} */

/**************** required and their assistant *************** {{{ */

static int getcallbacks (lua_State *L) {
  lua_getuservalue(L, 1);
  return 1;
}

static int parse_aux (lua_State *L, lsmp_ud *mpu, const char *s, size_t len) {
  mpu->L = L;
  lua_settop(L, 1);
  mpu->state = SML_Parse(mpu->parser, s, (int) len);
  if (mpu->state == MPSerror) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, mpu->errorref);  /* get original msg. */
    lua_error(L);

    SML_Parser p = mpu->parser;
    lua_pushnil(L);
    lua_pushstring(L, SML_ErrorString[0]);
    lua_pushinteger(L, SML_GetCurrentLineNumber(p) + 1);
    lua_pushinteger(L, SML_GetCurrentColumnNumber(p) + 1);
    lua_pushinteger(L, SML_GetCurrentByteIndex(p) + 1);
    return 5;
  }
  lua_settop(L, 1);  /* return parser userdata on success */
  return 1;
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

  lua_getfield(L, 1, "mode");
  int mode = lua_tointeger(L, -1);
  lua_remove(L, -1);
  lua_getfield(L, 1, "ext");
  const char *ext = lua_tolstring(L, -1, NULL);
  lua_remove(L, -1);

  mpu->L = L;
  mpu->state = MPSok;
  mpu->errorref = LUA_REFNIL;
  SML_Parser p = mpu->parser = SML_ParserCreate(mpu, mode, ext);
  if (!p) luaL_error(L, "SML_ParserCreate failed");

  p->ft = f_CharData;
  p->fs = f_StartElement;
  p->fe = f_EndElement;
  p->fc = f_Comment;
  p->fd = f_Scheme;
  p->fx = f_Extension;
  return 1;
}

static int lsmp_wraper (lua_State *L) {
  lua_remove(L, 1); /* pop the lsmp module table */
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
  lua_pushstring(L, "Lua " LUA_VDIR ": " CFLAGS);
  lua_setfield(L, -2, "__metatable");
  lua_setmetatable(L, -2);
  lua_pushstring(L, RELEASE " MIT License (c) " __DATE__);
  lua_setfield(L, -2, "lic");
  return 1;
} /* }}} */
/*vim:ts=4:sw=4:sts=4:et:fen:fdm=marker:fmr={{{,}}}:fdl=1:cms=*/
