#######################################################################
# Lua Simple/Sloppy Markup Parser
PROJECT = lsmp
LIBNAME = $(PROJECT).so

LUA_V    ?= 5.3
LUA_LDIR ?= /usr/share/lua/$(LUA_V)
LUA_CDIR ?= /usr/lib/lua/$(LUA_V)
LUA_INC  ?= /usr/include/lua$(LUA_V)

CFLAGS = -I$(LUA_INC)
LFLAGS =

DESTDIR = /usr/local/share/lua/$(LUA_V)
#######################################################################
#########  DO NOT MODIFY THE FOLLOWING UNLESS NECESSARY  ##############
#######################################################################
# DEBUG = -g -DDEBUG=2
OBJS = $(PROJECT)/lsmp.o

CC      ?= cc
RM      ?= rm -rf
INSTALL ?= install

CFLAGS += -pedantic -Wall -O2 -fPIC -DPIC -I/usr/include

CF = $(CFLAGS) $(DEBUG) -DCFLAGS='"$(CFLAGS)"'
CF += -DRELEASE='"$(shell echo $(PROJECT) | tr a-z A-Z)"'

LF = -shared
LF += $(LFLAGS)

lib: $(LIBNAME)

$(LIBNAME): $(PROJECT)/*
	$(CC) $(CF) -o $@ $(PROJECT)/lsmp.c $(LF)

install:
	$(INSTALL) -D $(LIBNAME) $(DESTDIR)/$(LUA_CDIR)/$(LIBNAME)
	$(INSTALL) -D lom.lua $(DESTDIR)/$(LUA_LDIR)/$(PROJECT)/lom.lua

clean:
	$(RM) src/$(LIBNAME) $(OBJS)
	$(RM) ./$(LIBNAME)
