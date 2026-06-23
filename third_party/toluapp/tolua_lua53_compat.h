/* tolua++ was written for Lua 5.1. We're embedding 5.3. A handful of
 * Lua 5.1 APIs moved or renamed; shim them here so tolua's runtime
 * compiles unmodified. Include this header first in each tolua_*.c. */
#ifndef WAR1_TOLUA_LUA53_COMPAT_H
#define WAR1_TOLUA_LUA53_COMPAT_H

#include "lua.h"

/* Lua 5.2 split function envs out. For userdata (and occasionally
 * tables), the closest 5.3 equivalent is lua_{get,set}uservalue.
 * tolua++ uses lua_getfenv/setfenv on userdata (for peer tables).
 * Map to uservalue, which does the job for userdata. */
#ifndef lua_getfenv
#define lua_getfenv(L, idx) lua_getuservalue((L), (idx))
#endif
#ifndef lua_setfenv
#define lua_setfenv(L, idx) (lua_setuservalue((L), (idx)), 1)
#endif

/* lua_objlen was renamed. */
#ifndef lua_objlen
#define lua_objlen(L, idx) lua_rawlen((L), (idx))
#endif

#endif
