/*
** Lua binding: stratagus
** Generated automatically by tolua++-1.0.93 on Thu Apr 23 20:04:52 2026.
*/

#ifndef __cplusplus
#include "stdlib.h"
#endif
#include "string.h"

#include "tolua++.h"

/* Exported function */
TOLUA_API int  tolua_stratagus_open (lua_State* tolua_S);

#include "stratagus.h"
#include <memory>
#include "video.h"
typedef std::shared_ptr<CGraphic> CGraphicPtr;
#include "ai.h"
#include "font.h"
#include "game.h"
#include "iolib.h"
#include "map.h"
#include "minimap.h"
#include "movie.h"
#include "netconnect.h"
#include "player.h"
#include "sound.h"
#include "sound_server.h"
#include "ui.h"
#include "unit.h"
#include "unit_manager.h"
#include "unittype.h"
#include "video.h"
#include "movie.h"
#ifdef _MSC_VER
#pragma warning(disable:4800)
#endif
using std::string;
using std::vector;
bool IsReplayGame();
void StartMap(const string &str, bool clean = true);
void StartReplay(const string &str, bool reveal = false);
void StartSavedGame(const string &str);
int SaveReplay(const std::string &filename);
#include "results.h"
void StopGame(GameResults result);
#include "settings.h"
#include "tileset.h"
#include "network.h"
extern string NetworkMapName;
extern string NetworkMapFragmentName;
void NetworkGamePrepareGameSettings(void);
#include "pathfinder.h"
#include "translate.h"
int GetNumOpponents(int player);
int GetTimer();
void ActionVictory();
void ActionDefeat();
void ActionDraw();
void ActionSetTimer(int cycles, bool increasing);
void ActionStartTimer();
void ActionStopTimer();
void SetTrigger(int trigger);
using namespace gcn;
extern void InitVideo();
extern void ShowFullImage(const std::string& name, unsigned int timeOutInSecond);
using CGraphicPtr = std::shared_ptr<CGraphic>;
void Load(CGraphicPtr* self, bool grayscale) { (*self)->Load(grayscale); }
void Resize(CGraphicPtr* self, int w, int h) { (*self)->Resize(w, h); }
void ResizeKeepRatio(CGraphicPtr* self, int w, int h) { (*self)->ResizeKeepRatio(w, h); }
void SetPaletteColor(CGraphicPtr* self, int idx, int r, int g, int b) { (*self)->SetPaletteColor(idx, r, g, b); }
void OverlayGraphic(CGraphicPtr* self, CGraphicPtr g, bool mask) { (*self)->OverlayGraphic(g.get(), mask); }
using CPlayerColorGraphicPtr = std::shared_ptr<CPlayerColorGraphic>;
void Load(CPlayerColorGraphicPtr* self, bool grayscale) { (*self)->Load(grayscale); }
void Resize(CPlayerColorGraphicPtr* self, int w, int h) { (*self)->Resize(w, h); }
void SetPaletteColor(CPlayerColorGraphicPtr* self, int idx, int r, int g, int b) { (*self)->SetPaletteColor(idx, r, g, b); }
void OverlayGraphic(CPlayerColorGraphicPtr* self, CGraphicPtr g, bool mask) { (*self)->OverlayGraphic(g.get(), mask); }
using MngPtr = std::shared_ptr<Mng>;
void Load(MngPtr* self) { (*self)->Load(); }
void Draw(MngPtr* self, int w, int h) { (*self)->Draw(w, h); }
void Reset(MngPtr* self) { (*self)->Reset(); }
extern std::string CliMapName;

/* function to release collected object via destructor */
#ifdef __cplusplus

static int tolua_collect_SDL_Color (lua_State* tolua_S)
{
 SDL_Color* self = (SDL_Color*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CUIUserButton (lua_State* tolua_S)
{
 CUIUserButton* self = (CUIUserButton*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CUIButton (lua_State* tolua_S)
{
 CUIButton* self = (CUIButton*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_MngPtr (lua_State* tolua_S)
{
 MngPtr* self = (MngPtr*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CPlayer (lua_State* tolua_S)
{
 CPlayer* self = (CPlayer*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_SettingsPresets (lua_State* tolua_S)
{
 SettingsPresets* self = (SettingsPresets*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CColor (lua_State* tolua_S)
{
 CColor* self = (CColor*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CNetworkHost (lua_State* tolua_S)
{
 CNetworkHost* self = (CNetworkHost*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CPlayerColorGraphicPtr (lua_State* tolua_S)
{
 CPlayerColorGraphicPtr* self = (CPlayerColorGraphicPtr*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CResourceInfo (lua_State* tolua_S)
{
 CResourceInfo* self = (CResourceInfo*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CGraphicPtr (lua_State* tolua_S)
{
 CGraphicPtr* self = (CGraphicPtr*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_Movie (lua_State* tolua_S)
{
 Movie* self = (Movie*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}

static int tolua_collect_CFiller (lua_State* tolua_S)
{
 CFiller* self = (CFiller*) tolua_tousertype(tolua_S,1,0);
	Mtolua_delete(self);
	return 0;
}
#endif


/* function to register type */
static void tolua_reg_types (lua_State* tolua_S)
{
 tolua_usertype(tolua_S,"CPlayerColorGraphic");
 tolua_usertype(tolua_S,"LuaActionListener");
 tolua_usertype(tolua_S,"vector<CUIButton>");
 tolua_usertype(tolua_S,"CFontColor");
 tolua_usertype(tolua_S,"Settings");
 tolua_usertype(tolua_S,"CTileset");
 tolua_usertype(tolua_S,"CResourceInfo");
 tolua_usertype(tolua_S,"vector<CFiller>");
 tolua_usertype(tolua_S,"CIcon");
 tolua_usertype(tolua_S,"CMapInfo");
 tolua_usertype(tolua_S,"CFont");
 tolua_usertype(tolua_S,"CServerSetup");
 tolua_usertype(tolua_S,"CMapArea");
 tolua_usertype(tolua_S,"MngPtr");
 tolua_usertype(tolua_S,"SettingsPresets");
 tolua_usertype(tolua_S,"CUnit");
 tolua_usertype(tolua_S,"CViewport");
 tolua_usertype(tolua_S,"ServerSetupStateRacesArray");
 tolua_usertype(tolua_S,"CButtonPanel");
 tolua_usertype(tolua_S,"CStatusLine");
 tolua_usertype(tolua_S,"CMinimap");
 tolua_usertype(tolua_S,"SDL_Color");
 tolua_usertype(tolua_S,"ButtonStyle");
 tolua_usertype(tolua_S,"CUIButton");
 tolua_usertype(tolua_S,"CPreference");
 tolua_usertype(tolua_S,"CFiller");
 tolua_usertype(tolua_S,"CUserInterface");
 tolua_usertype(tolua_S,"vector<string>");
 tolua_usertype(tolua_S,"CColor");
 tolua_usertype(tolua_S,"CUpgrade");
 tolua_usertype(tolua_S,"CGraphic");
 tolua_usertype(tolua_S,"CInfoPanel");
 tolua_usertype(tolua_S,"Vec2i");
 tolua_usertype(tolua_S,"CGraphicPtr");
 tolua_usertype(tolua_S,"CPlayerColorGraphicPtr");
 tolua_usertype(tolua_S,"CUIUserButton");
 tolua_usertype(tolua_S,"CVideo");
 tolua_usertype(tolua_S,"CUnitType");
 tolua_usertype(tolua_S,"PixelPos");
 tolua_usertype(tolua_S,"CUnitManager");
 tolua_usertype(tolua_S,"vector<int>");
 tolua_usertype(tolua_S,"Mng");
 tolua_usertype(tolua_S,"CMap");
 tolua_usertype(tolua_S,"CNetworkHost");
 tolua_usertype(tolua_S,"CUITimer");
 tolua_usertype(tolua_S,"CPlayer");
 tolua_usertype(tolua_S,"vector<CUIUserButton>");
 tolua_usertype(tolua_S,"Movie");
 tolua_usertype(tolua_S,"CPieMenu");
}

/* function: AiAttackWithForceAt */
#ifndef TOLUA_DISABLE_tolua_stratagus_AiAttackWithForceAt00
static int tolua_stratagus_AiAttackWithForceAt00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned int force = ((unsigned int)  tolua_tonumber(tolua_S,1,0));
  unsigned int x = ((unsigned int)  tolua_tonumber(tolua_S,2,0));
  unsigned int y = ((unsigned int)  tolua_tonumber(tolua_S,3,0));
  {
   AiAttackWithForceAt(force,x,y);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'AiAttackWithForceAt'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: AiAttackWithForce */
#ifndef TOLUA_DISABLE_tolua_stratagus_AiAttackWithForce00
static int tolua_stratagus_AiAttackWithForce00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned int force = ((unsigned int)  tolua_tonumber(tolua_S,1,0));
  {
   AiAttackWithForce(force);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'AiAttackWithForce'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  CFont */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFont_New00
static int tolua_stratagus_CFont_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CFont",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"CGraphicPtr",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  CGraphicPtr g = *((CGraphicPtr*)  tolua_tousertype(tolua_S,3,0));
  {
   CFont* tolua_ret = (CFont*)  CFont::New(ident,g);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CFont");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CFont */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFont_Get00
static int tolua_stratagus_CFont_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CFont",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CFont* tolua_ret = (CFont*)  CFont::Get(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CFont");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Height of class  CFont */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFont_Height00
static int tolua_stratagus_CFont_Height00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CFont",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CFont* self = (CFont*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Height'", NULL);
#endif
  {
   int tolua_ret = (int)  self->Height();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Height'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Width of class  CFont */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFont_Width00
static int tolua_stratagus_CFont_Width00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CFont",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CFont* self = (CFont*)  tolua_tousertype(tolua_S,1,0);
  const std::string text = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Width'", NULL);
#endif
  {
   int tolua_ret = (int)  self->Width(text);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Width'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  CFontColor */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFontColor_New00
static int tolua_stratagus_CFontColor_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CFontColor",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CFontColor* tolua_ret = (CFontColor*)  CFontColor::New(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CFontColor");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CFontColor */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFontColor_Get00
static int tolua_stratagus_CFontColor_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CFontColor",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CFontColor* tolua_ret = (CFontColor*)  CFontColor::Get(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CFontColor");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Colors of class  CFontColor */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CFontColor_Colors
static int tolua_get_stratagus_CFontColor_Colors(lua_State* tolua_S)
{
 int tolua_index;
  CFontColor* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CFontColor*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxFontColors)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  tolua_pushusertype(tolua_S,(void*)&self->Colors[tolua_index],"SDL_Color");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Colors of class  CFontColor */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CFontColor_Colors
static int tolua_set_stratagus_CFontColor_Colors(lua_State* tolua_S)
{
 int tolua_index;
  CFontColor* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CFontColor*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxFontColors)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Colors[tolua_index] = *((SDL_Color*)  tolua_tousertype(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: IsReplayGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_IsReplayGame00
static int tolua_stratagus_IsReplayGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  IsReplayGame();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsReplayGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: StartMap */
#ifndef TOLUA_DISABLE_tolua_stratagus_StartMap00
static int tolua_stratagus_StartMap00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isboolean(tolua_S,2,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const string str = ((const string)  tolua_tocppstring(tolua_S,1,0));
  bool clean = ((bool)  tolua_toboolean(tolua_S,2,true));
  {
   StartMap(str,clean);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StartMap'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: StartReplay */
#ifndef TOLUA_DISABLE_tolua_stratagus_StartReplay00
static int tolua_stratagus_StartReplay00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isboolean(tolua_S,2,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const string str = ((const string)  tolua_tocppstring(tolua_S,1,0));
  bool reveal = ((bool)  tolua_toboolean(tolua_S,2,false));
  {
   StartReplay(str,reveal);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StartReplay'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: StartSavedGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_StartSavedGame00
static int tolua_stratagus_StartSavedGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const string str = ((const string)  tolua_tocppstring(tolua_S,1,0));
  {
   StartSavedGame(str);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StartSavedGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SaveReplay */
#ifndef TOLUA_DISABLE_tolua_stratagus_SaveReplay00
static int tolua_stratagus_SaveReplay00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string filename = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   int tolua_ret = (int)  SaveReplay(filename);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SaveReplay'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameResult */
#ifndef TOLUA_DISABLE_tolua_get_GameResult
static int tolua_get_GameResult(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)GameResult);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameResult */
#ifndef TOLUA_DISABLE_tolua_set_GameResult
static int tolua_set_GameResult(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  GameResult = ((GameResults) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: StopGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_StopGame00
static int tolua_stratagus_StopGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  GameResults result = ((GameResults) (int)  tolua_tonumber(tolua_S,1,0));
  {
   StopGame(result);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StopGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameRunning */
#ifndef TOLUA_DISABLE_tolua_get_GameRunning
static int tolua_get_GameRunning(lua_State* tolua_S)
{
  tolua_pushboolean(tolua_S,(bool)GameRunning);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameRunning */
#ifndef TOLUA_DISABLE_tolua_set_GameRunning
static int tolua_set_GameRunning(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  GameRunning = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetGamePaused */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetGamePaused00
static int tolua_stratagus_SetGamePaused00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool paused = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetGamePaused(paused);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetGamePaused'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetGamePaused */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetGamePaused00
static int tolua_stratagus_GetGamePaused00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  GetGamePaused();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetGamePaused'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GamePaused */
#ifndef TOLUA_DISABLE_tolua_get_GamePaused
static int tolua_get_GamePaused(lua_State* tolua_S)
{
  tolua_pushboolean(tolua_S,(bool)GetGamePaused());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GamePaused */
#ifndef TOLUA_DISABLE_tolua_set_GamePaused
static int tolua_set_GamePaused(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  SetGamePaused(((bool)  tolua_toboolean(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetGameSpeed */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetGameSpeed00
static int tolua_stratagus_SetGameSpeed00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int speed = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   SetGameSpeed(speed);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetGameSpeed'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetGameSpeed */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetGameSpeed00
static int tolua_stratagus_GetGameSpeed00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetGameSpeed();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetGameSpeed'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameSpeed */
#ifndef TOLUA_DISABLE_tolua_get_GameSpeed
static int tolua_get_GameSpeed(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)GetGameSpeed());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameSpeed */
#ifndef TOLUA_DISABLE_tolua_set_GameSpeed
static int tolua_set_GameSpeed(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  SetGameSpeed(((int)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameObserve */
#ifndef TOLUA_DISABLE_tolua_get_GameObserve
static int tolua_get_GameObserve(lua_State* tolua_S)
{
  tolua_pushboolean(tolua_S,(bool)GameObserve);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameObserve */
#ifndef TOLUA_DISABLE_tolua_set_GameObserve
static int tolua_set_GameObserve(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  GameObserve = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameEstablishing */
#ifndef TOLUA_DISABLE_tolua_get_GameEstablishing
static int tolua_get_GameEstablishing(lua_State* tolua_S)
{
  tolua_pushboolean(tolua_S,(bool)GameEstablishing);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameEstablishing */
#ifndef TOLUA_DISABLE_tolua_set_GameEstablishing
static int tolua_set_GameEstablishing(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  GameEstablishing = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameCycle */
#ifndef TOLUA_DISABLE_tolua_get_unsigned_GameCycle
static int tolua_get_unsigned_GameCycle(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)GameCycle);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameCycle */
#ifndef TOLUA_DISABLE_tolua_set_unsigned_GameCycle
static int tolua_set_unsigned_GameCycle(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  GameCycle = ((unsigned long)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: FastForwardCycle */
#ifndef TOLUA_DISABLE_tolua_get_unsigned_FastForwardCycle
static int tolua_get_unsigned_FastForwardCycle(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)FastForwardCycle);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: FastForwardCycle */
#ifndef TOLUA_DISABLE_tolua_set_unsigned_FastForwardCycle
static int tolua_set_unsigned_FastForwardCycle(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  FastForwardCycle = ((unsigned long)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PlayerColor of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_get_SettingsPresets_PlayerColor
static int tolua_get_SettingsPresets_PlayerColor(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PlayerColor'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->PlayerColor);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PlayerColor of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_set_SettingsPresets_PlayerColor
static int tolua_set_SettingsPresets_PlayerColor(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PlayerColor'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->PlayerColor = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AIScript of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_get_SettingsPresets_AIScript
static int tolua_get_SettingsPresets_AIScript(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AIScript'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->AIScript);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AIScript of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_set_SettingsPresets_AIScript
static int tolua_set_SettingsPresets_AIScript(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AIScript'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->AIScript = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Race of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_get_SettingsPresets_Race
static int tolua_get_SettingsPresets_Race(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Race'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Race);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Race of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_set_SettingsPresets_Race
static int tolua_set_SettingsPresets_Race(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Race'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Race = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Team of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_get_SettingsPresets_Team
static int tolua_get_SettingsPresets_Team(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Team'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Team);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Team of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_set_SettingsPresets_Team
static int tolua_set_SettingsPresets_Team(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Team'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Team = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Type of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_get_SettingsPresets_Type
static int tolua_get_SettingsPresets_Type(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Type'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Type);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Type of class  SettingsPresets */
#ifndef TOLUA_DISABLE_tolua_set_SettingsPresets_Type
static int tolua_set_SettingsPresets_Type(lua_State* tolua_S)
{
  SettingsPresets* self = (SettingsPresets*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Type'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Type = ((PlayerTypes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetGameType of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_NetGameType
static int tolua_get_Settings_NetGameType(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NetGameType'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->NetGameType);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NetGameType of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_NetGameType
static int tolua_set_Settings_NetGameType(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NetGameType'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NetGameType = ((NetGameTypes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Presets of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_Settings_Presets
static int tolua_get_stratagus_Settings_Presets(lua_State* tolua_S)
{
 int tolua_index;
  Settings* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (Settings*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  tolua_pushusertype(tolua_S,(void*)&self->Presets[tolua_index],"SettingsPresets");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Presets of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_Settings_Presets
static int tolua_set_stratagus_Settings_Presets(lua_State* tolua_S)
{
 int tolua_index;
  Settings* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (Settings*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Presets[tolua_index] = *((SettingsPresets*)  tolua_tousertype(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Resources of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_Resources
static int tolua_get_Settings_Resources(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Resources'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Resources);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Resources of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_Resources
static int tolua_set_Settings_Resources(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Resources'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Resources = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NumUnits of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_NumUnits
static int tolua_get_Settings_NumUnits(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NumUnits'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->NumUnits);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NumUnits of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_NumUnits
static int tolua_set_Settings_NumUnits(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NumUnits'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NumUnits = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Opponents of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_Opponents
static int tolua_get_Settings_Opponents(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Opponents'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Opponents);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Opponents of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_Opponents
static int tolua_set_Settings_Opponents(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Opponents'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Opponents = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Difficulty of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_Difficulty
static int tolua_get_Settings_Difficulty(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Difficulty'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Difficulty);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Difficulty of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_Difficulty
static int tolua_set_Settings_Difficulty(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Difficulty'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Difficulty = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameType of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_GameType
static int tolua_get_Settings_GameType(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GameType'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->GameType);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameType of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_GameType
static int tolua_set_Settings_GameType(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GameType'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->GameType = ((GameTypes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: FoV of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_FoV
static int tolua_get_Settings_FoV(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FoV'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->FoV);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: FoV of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_FoV
static int tolua_set_Settings_FoV(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FoV'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->FoV = ((FieldOfViewTypes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: RevealMap of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_RevealMap
static int tolua_get_Settings_RevealMap(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'RevealMap'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->RevealMap);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: RevealMap of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_RevealMap
static int tolua_set_Settings_RevealMap(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'RevealMap'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->RevealMap = ((MapRevealModes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: DefeatReveal of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_DefeatReveal
static int tolua_get_Settings_DefeatReveal(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'DefeatReveal'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->DefeatReveal);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: DefeatReveal of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_DefeatReveal
static int tolua_set_Settings_DefeatReveal(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'DefeatReveal'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->DefeatReveal = ((RevealTypes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NoFogOfWar of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_NoFogOfWar
static int tolua_get_Settings_NoFogOfWar(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NoFogOfWar'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->NoFogOfWar);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NoFogOfWar of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_NoFogOfWar
static int tolua_set_Settings_NoFogOfWar(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NoFogOfWar'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NoFogOfWar = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Inside of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_Inside
static int tolua_get_Settings_Inside(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Inside'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->Inside);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Inside of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_Inside
static int tolua_set_Settings_Inside(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Inside'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Inside = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AiExplores of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_AiExplores
static int tolua_get_Settings_AiExplores(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiExplores'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->AiExplores);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AiExplores of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_AiExplores
static int tolua_set_Settings_AiExplores(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiExplores'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->AiExplores = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SimplifiedAutoTargeting of class  Settings */
#ifndef TOLUA_DISABLE_tolua_get_Settings_SimplifiedAutoTargeting
static int tolua_get_Settings_SimplifiedAutoTargeting(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SimplifiedAutoTargeting'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->SimplifiedAutoTargeting);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SimplifiedAutoTargeting of class  Settings */
#ifndef TOLUA_DISABLE_tolua_set_Settings_SimplifiedAutoTargeting
static int tolua_set_Settings_SimplifiedAutoTargeting(lua_State* tolua_S)
{
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SimplifiedAutoTargeting'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SimplifiedAutoTargeting = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: GetUserGameSetting of class  Settings */
#ifndef TOLUA_DISABLE_tolua_stratagus_Settings_GetUserGameSetting00
static int tolua_stratagus_Settings_GetUserGameSetting00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"Settings",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
  int i = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'GetUserGameSetting'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->GetUserGameSetting(i);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetUserGameSetting'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: SetUserGameSetting of class  Settings */
#ifndef TOLUA_DISABLE_tolua_stratagus_Settings_SetUserGameSetting00
static int tolua_stratagus_Settings_SetUserGameSetting00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"Settings",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isboolean(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  Settings* self = (Settings*)  tolua_tousertype(tolua_S,1,0);
  int i = ((int)  tolua_tonumber(tolua_S,2,0));
  bool v = ((bool)  tolua_toboolean(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'SetUserGameSetting'", NULL);
#endif
  {
   self->SetUserGameSetting(i,v);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetUserGameSetting'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameSettings */
#ifndef TOLUA_DISABLE_tolua_get_GameSettings
static int tolua_get_GameSettings(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&GameSettings,"Settings");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameSettings */
#ifndef TOLUA_DISABLE_tolua_set_GameSettings
static int tolua_set_GameSettings(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Settings",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  GameSettings = *((Settings*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Description of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_CMapInfo_Description
static int tolua_get_CMapInfo_Description(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Description'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Description);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Description of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_CMapInfo_Description
static int tolua_set_CMapInfo_Description(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Description'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Description = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Filename of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_CMapInfo_Filename
static int tolua_get_CMapInfo_Filename(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Filename'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Filename);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Filename of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_CMapInfo_Filename
static int tolua_set_CMapInfo_Filename(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Filename'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Filename = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Preamble of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_CMapInfo_Preamble
static int tolua_get_CMapInfo_Preamble(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Preamble'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Preamble);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Preamble of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_CMapInfo_Preamble
static int tolua_set_CMapInfo_Preamble(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Preamble'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Preamble = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Postamble of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_CMapInfo_Postamble
static int tolua_get_CMapInfo_Postamble(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Postamble'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Postamble);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Postamble of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_CMapInfo_Postamble
static int tolua_set_CMapInfo_Postamble(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Postamble'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Postamble = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MapWidth of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_CMapInfo_MapWidth
static int tolua_get_CMapInfo_MapWidth(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MapWidth'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MapWidth);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MapWidth of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_CMapInfo_MapWidth
static int tolua_set_CMapInfo_MapWidth(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MapWidth'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MapWidth = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MapHeight of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_CMapInfo_MapHeight
static int tolua_get_CMapInfo_MapHeight(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MapHeight'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MapHeight);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MapHeight of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_CMapInfo_MapHeight
static int tolua_set_CMapInfo_MapHeight(lua_State* tolua_S)
{
  CMapInfo* self = (CMapInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MapHeight'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MapHeight = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PlayerType of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CMapInfo_PlayerType
static int tolua_get_stratagus_CMapInfo_PlayerType(lua_State* tolua_S)
{
 int tolua_index;
  CMapInfo* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CMapInfo*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->PlayerType[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PlayerType of class  CMapInfo */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CMapInfo_PlayerType
static int tolua_set_stratagus_CMapInfo_PlayerType(lua_State* tolua_S)
{
 int tolua_index;
  CMapInfo* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CMapInfo*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->PlayerType[tolua_index] = ((PlayerTypes)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Name of class  CTileset */
#ifndef TOLUA_DISABLE_tolua_get_CTileset_Name
static int tolua_get_CTileset_Name(lua_State* tolua_S)
{
  CTileset* self = (CTileset*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Name);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Name of class  CTileset */
#ifndef TOLUA_DISABLE_tolua_set_CTileset_Name
static int tolua_set_CTileset_Name(lua_State* tolua_S)
{
  CTileset* self = (CTileset*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Name = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Info of class  CMap */
#ifndef TOLUA_DISABLE_tolua_get_CMap_Info
static int tolua_get_CMap_Info(lua_State* tolua_S)
{
  CMap* self = (CMap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Info'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Info,"CMapInfo");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Tileset of class  CMap */
#ifndef TOLUA_DISABLE_tolua_get_CMap_Tileset
static int tolua_get_CMap_Tileset(lua_State* tolua_S)
{
  CMap* self = (CMap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Tileset'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Tileset,"CTileset");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Map */
#ifndef TOLUA_DISABLE_tolua_get_Map
static int tolua_get_Map(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&Map,"CMap");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Map */
#ifndef TOLUA_DISABLE_tolua_set_Map
static int tolua_set_Map(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CMap",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  Map = *((CMap*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetTile */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetTile00
static int tolua_stratagus_SetTile00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,5,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,6,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int tile = ((int)  tolua_tonumber(tolua_S,1,0));
  int w = ((int)  tolua_tonumber(tolua_S,2,0));
  int h = ((int)  tolua_tonumber(tolua_S,3,0));
  int value = ((int)  tolua_tonumber(tolua_S,4,0));
  int elevation = ((int)  tolua_tonumber(tolua_S,5,0));
  {
   SetTile(tile,w,h,value,elevation);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetTile'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_X
static int tolua_get_CMinimap_X(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_X
static int tolua_set_CMinimap_X(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_Y
static int tolua_get_CMinimap_Y(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_Y
static int tolua_set_CMinimap_Y(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: W of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_W
static int tolua_get_CMinimap_W(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'W'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->W);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: W of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_W
static int tolua_set_CMinimap_W(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'W'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->W = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: H of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_H
static int tolua_get_CMinimap_H(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'H'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->H);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: H of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_H
static int tolua_set_CMinimap_H(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'H'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->H = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: WithTerrain of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_WithTerrain
static int tolua_get_CMinimap_WithTerrain(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'WithTerrain'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->WithTerrain);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: WithTerrain of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_WithTerrain
static int tolua_set_CMinimap_WithTerrain(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'WithTerrain'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->WithTerrain = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowSelected of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_ShowSelected
static int tolua_get_CMinimap_ShowSelected(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowSelected'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowSelected);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowSelected of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_ShowSelected
static int tolua_set_CMinimap_ShowSelected(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowSelected'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowSelected = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Transparent of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_get_CMinimap_Transparent
static int tolua_get_CMinimap_Transparent(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Transparent'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->Transparent);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Transparent of class  CMinimap */
#ifndef TOLUA_DISABLE_tolua_set_CMinimap_Transparent
static int tolua_set_CMinimap_Transparent(lua_State* tolua_S)
{
  CMinimap* self = (CMinimap*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Transparent'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Transparent = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: InitNetwork1 */
#ifndef TOLUA_DISABLE_tolua_stratagus_InitNetwork100
static int tolua_stratagus_InitNetwork100(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   InitNetwork1();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'InitNetwork1'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ExitNetwork1 */
#ifndef TOLUA_DISABLE_tolua_stratagus_ExitNetwork100
static int tolua_stratagus_ExitNetwork100(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ExitNetwork1();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ExitNetwork1'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: IsNetworkGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_IsNetworkGame00
static int tolua_stratagus_IsNetworkGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  IsNetworkGame();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsNetworkGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkSetupServerAddress */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkSetupServerAddress00
static int tolua_stratagus_NetworkSetupServerAddress00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string serveraddr = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  int port = ((int)  tolua_tonumber(tolua_S,2,0));
  {
   int tolua_ret = (int)  NetworkSetupServerAddress(serveraddr,port);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkSetupServerAddress'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkInitClientConnect */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkInitClientConnect00
static int tolua_stratagus_NetworkInitClientConnect00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   NetworkInitClientConnect();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkInitClientConnect'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkInitServerConnect */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkInitServerConnect00
static int tolua_stratagus_NetworkInitServerConnect00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int openslots = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   NetworkInitServerConnect(openslots);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkInitServerConnect'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkServerStartGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkServerStartGame00
static int tolua_stratagus_NetworkServerStartGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   NetworkServerStartGame();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkServerStartGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkProcessClientRequest */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkProcessClientRequest00
static int tolua_stratagus_NetworkProcessClientRequest00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   NetworkProcessClientRequest();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkProcessClientRequest'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetNetworkState */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetNetworkState00
static int tolua_stratagus_GetNetworkState00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetNetworkState();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetNetworkState'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkServerResyncClients */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkServerResyncClients00
static int tolua_stratagus_NetworkServerResyncClients00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   NetworkServerResyncClients();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkServerResyncClients'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkDetachFromServer */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkDetachFromServer00
static int tolua_stratagus_NetworkDetachFromServer00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   NetworkDetachFromServer();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkDetachFromServer'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator&[] of class  ServerSetupStateRacesArray */
#ifndef TOLUA_DISABLE_tolua_stratagus_ServerSetupStateRacesArray__seti00
static int tolua_stratagus_ServerSetupStateRacesArray__seti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"ServerSetupStateRacesArray",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  ServerSetupStateRacesArray* self = (ServerSetupStateRacesArray*)  tolua_tousertype(tolua_S,1,0);
  int idx = ((int)  tolua_tonumber(tolua_S,2,0));
  int tolua_value = ((int)  tolua_tonumber(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator&[]'", NULL);
#endif
  self->operator[](idx) =  tolua_value;
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.seti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  ServerSetupStateRacesArray */
#ifndef TOLUA_DISABLE_tolua_stratagus_ServerSetupStateRacesArray__geti00
static int tolua_stratagus_ServerSetupStateRacesArray__geti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"ServerSetupStateRacesArray",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  ServerSetupStateRacesArray* self = (ServerSetupStateRacesArray*)  tolua_tousertype(tolua_S,1,0);
  int idx = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   int tolua_ret = (int)  self->operator[](idx);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.geti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  ServerSetupStateRacesArray */
#ifndef TOLUA_DISABLE_tolua_stratagus_ServerSetupStateRacesArray__geti01
static int tolua_stratagus_ServerSetupStateRacesArray__geti01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const ServerSetupStateRacesArray",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  const ServerSetupStateRacesArray* self = (const ServerSetupStateRacesArray*)  tolua_tousertype(tolua_S,1,0);
  int idx = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   int tolua_ret = (int)  self->operator[](idx);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_ServerSetupStateRacesArray__geti00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ServerGameSettings of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_ServerGameSettings
static int tolua_get_CServerSetup_ServerGameSettings(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ServerGameSettings'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->ServerGameSettings,"Settings");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ServerGameSettings of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_ServerGameSettings
static int tolua_set_CServerSetup_ServerGameSettings(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ServerGameSettings'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Settings",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ServerGameSettings = *((Settings*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: CompOpt of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CServerSetup_CompOpt
static int tolua_get_stratagus_CServerSetup_CompOpt(lua_State* tolua_S)
{
 int tolua_index;
  CServerSetup* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CServerSetup*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->CompOpt[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: CompOpt of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CServerSetup_CompOpt
static int tolua_set_stratagus_CServerSetup_CompOpt(lua_State* tolua_S)
{
 int tolua_index;
  CServerSetup* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CServerSetup*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->CompOpt[tolua_index] = ((SlotOption)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Ready of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CServerSetup_Ready
static int tolua_get_stratagus_CServerSetup_Ready(lua_State* tolua_S)
{
 int tolua_index;
  CServerSetup* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CServerSetup*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->Ready[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Ready of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CServerSetup_Ready
static int tolua_set_stratagus_CServerSetup_Ready(lua_State* tolua_S)
{
 int tolua_index;
  CServerSetup* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CServerSetup*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Ready[tolua_index] = ((unsigned short)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ResourcesOption of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_ResourcesOption
static int tolua_get_CServerSetup_unsigned_ResourcesOption(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ResourcesOption'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_ResourcesOption());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ResourcesOption of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_ResourcesOption
static int tolua_set_CServerSetup_unsigned_ResourcesOption(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ResourcesOption'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_ResourcesOption((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitsOption of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_UnitsOption
static int tolua_get_CServerSetup_unsigned_UnitsOption(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UnitsOption'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_UnitsOption());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: UnitsOption of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_UnitsOption
static int tolua_set_CServerSetup_unsigned_UnitsOption(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UnitsOption'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_UnitsOption((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: FogOfWar of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_FogOfWar
static int tolua_get_CServerSetup_unsigned_FogOfWar(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FogOfWar'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_FogOfWar());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: FogOfWar of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_FogOfWar
static int tolua_set_CServerSetup_unsigned_FogOfWar(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FogOfWar'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_FogOfWar((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Inside of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_Inside
static int tolua_get_CServerSetup_unsigned_Inside(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Inside'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_Inside());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Inside of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_Inside
static int tolua_set_CServerSetup_unsigned_Inside(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Inside'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_Inside((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: RevealMap of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_RevealMap
static int tolua_get_CServerSetup_unsigned_RevealMap(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'RevealMap'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_RevealMap());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: RevealMap of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_RevealMap
static int tolua_set_CServerSetup_unsigned_RevealMap(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'RevealMap'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_RevealMap((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GameTypeOption of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_GameTypeOption
static int tolua_get_CServerSetup_unsigned_GameTypeOption(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GameTypeOption'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_GameTypeOption());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GameTypeOption of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_GameTypeOption
static int tolua_set_CServerSetup_unsigned_GameTypeOption(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GameTypeOption'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_GameTypeOption((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Difficulty of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_Difficulty
static int tolua_get_CServerSetup_unsigned_Difficulty(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Difficulty'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_Difficulty());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Difficulty of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_Difficulty
static int tolua_set_CServerSetup_unsigned_Difficulty(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Difficulty'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_Difficulty((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Opponents of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_unsigned_Opponents
static int tolua_get_CServerSetup_unsigned_Opponents(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Opponents'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->get_Opponents());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Opponents of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_set_CServerSetup_unsigned_Opponents
static int tolua_set_CServerSetup_unsigned_Opponents(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Opponents'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_Opponents((( unsigned char)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Race of class  CServerSetup */
#ifndef TOLUA_DISABLE_tolua_get_CServerSetup_Race_ptr
static int tolua_get_CServerSetup_Race_ptr(lua_State* tolua_S)
{
  CServerSetup* self = (CServerSetup*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Race'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->get_Race(),"ServerSetupStateRacesArray");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: LocalSetupState */
#ifndef TOLUA_DISABLE_tolua_get_LocalSetupState
static int tolua_get_LocalSetupState(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&LocalSetupState,"CServerSetup");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: LocalSetupState */
#ifndef TOLUA_DISABLE_tolua_set_LocalSetupState
static int tolua_set_LocalSetupState(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CServerSetup",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  LocalSetupState = *((CServerSetup*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ServerSetupState */
#ifndef TOLUA_DISABLE_tolua_get_ServerSetupState
static int tolua_get_ServerSetupState(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&ServerSetupState,"CServerSetup");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ServerSetupState */
#ifndef TOLUA_DISABLE_tolua_set_ServerSetupState
static int tolua_set_ServerSetupState(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CServerSetup",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  ServerSetupState = *((CServerSetup*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetLocalHostsSlot */
#ifndef TOLUA_DISABLE_tolua_get_NetLocalHostsSlot
static int tolua_get_NetLocalHostsSlot(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)NetLocalHostsSlot);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NetLocalHostsSlot */
#ifndef TOLUA_DISABLE_tolua_set_NetLocalHostsSlot
static int tolua_set_NetLocalHostsSlot(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  NetLocalHostsSlot = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetPlayerNameSize */
#ifndef TOLUA_DISABLE_tolua_get_NetPlayerNameSize
static int tolua_get_NetPlayerNameSize(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)NetPlayerNameSize);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Host of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_get_CNetworkHost_unsigned_Host
static int tolua_get_CNetworkHost_unsigned_Host(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Host'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Host);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Host of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_set_CNetworkHost_unsigned_Host
static int tolua_set_CNetworkHost_unsigned_Host(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Host'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Host = ((unsigned long)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Port of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_get_CNetworkHost_unsigned_Port
static int tolua_get_CNetworkHost_unsigned_Port(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Port'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Port);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Port of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_set_CNetworkHost_unsigned_Port
static int tolua_set_CNetworkHost_unsigned_Port(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Port'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Port = ((unsigned short)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PlyNr of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_get_CNetworkHost_unsigned_PlyNr
static int tolua_get_CNetworkHost_unsigned_PlyNr(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PlyNr'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->PlyNr);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PlyNr of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_set_CNetworkHost_unsigned_PlyNr
static int tolua_set_CNetworkHost_unsigned_PlyNr(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PlyNr'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->PlyNr = ((unsigned short)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PlyName of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_get_CNetworkHost_PlyName
static int tolua_get_CNetworkHost_PlyName(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PlyName'",NULL);
#endif
  tolua_pushstring(tolua_S,(const char*)self->PlyName);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PlyName of class  CNetworkHost */
#ifndef TOLUA_DISABLE_tolua_set_CNetworkHost_PlyName
static int tolua_set_CNetworkHost_PlyName(lua_State* tolua_S)
{
  CNetworkHost* self = (CNetworkHost*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PlyName'",NULL);
  if (!tolua_istable(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
 strncpy((char*)
self->PlyName,(const char*)tolua_tostring(tolua_S,2,0),NetPlayerNameSize-1);
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Hosts */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_Hosts
static int tolua_get_stratagus_Hosts(lua_State* tolua_S)
{
 int tolua_index;
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  tolua_pushusertype(tolua_S,(void*)&Hosts[tolua_index],"CNetworkHost");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Hosts */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_Hosts
static int tolua_set_stratagus_Hosts(lua_State* tolua_S)
{
 int tolua_index;
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  Hosts[tolua_index] = *((CNetworkHost*)  tolua_tousertype(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetworkMapName */
#ifndef TOLUA_DISABLE_tolua_get_NetworkMapName
static int tolua_get_NetworkMapName(lua_State* tolua_S)
{
  tolua_pushcppstring(tolua_S,(const char*)NetworkMapName);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NetworkMapName */
#ifndef TOLUA_DISABLE_tolua_set_NetworkMapName
static int tolua_set_NetworkMapName(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  NetworkMapName = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetworkMapFragmentName */
#ifndef TOLUA_DISABLE_tolua_get_NetworkMapFragmentName
static int tolua_get_NetworkMapFragmentName(lua_State* tolua_S)
{
  tolua_pushcppstring(tolua_S,(const char*)NetworkMapFragmentName);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NetworkMapFragmentName */
#ifndef TOLUA_DISABLE_tolua_set_NetworkMapFragmentName
static int tolua_set_NetworkMapFragmentName(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  NetworkMapFragmentName = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: NetworkGamePrepareGameSettings */
#ifndef TOLUA_DISABLE_tolua_stratagus_NetworkGamePrepareGameSettings00
static int tolua_stratagus_NetworkGamePrepareGameSettings00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   NetworkGamePrepareGameSettings();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'NetworkGamePrepareGameSettings'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AStarFixedUnitCrossingCost */
#ifndef TOLUA_DISABLE_tolua_get_AStarFixedUnitCrossingCost
static int tolua_get_AStarFixedUnitCrossingCost(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)GetAStarFixedUnitCrossingCost());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AStarFixedUnitCrossingCost */
#ifndef TOLUA_DISABLE_tolua_set_AStarFixedUnitCrossingCost
static int tolua_set_AStarFixedUnitCrossingCost(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  SetAStarFixedUnitCrossingCost(((int)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AStarMovingUnitCrossingCost */
#ifndef TOLUA_DISABLE_tolua_get_AStarMovingUnitCrossingCost
static int tolua_get_AStarMovingUnitCrossingCost(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)GetAStarMovingUnitCrossingCost());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AStarMovingUnitCrossingCost */
#ifndef TOLUA_DISABLE_tolua_set_AStarMovingUnitCrossingCost
static int tolua_set_AStarMovingUnitCrossingCost(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  SetAStarMovingUnitCrossingCost(((int)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AStarKnowUnseenTerrain */
#ifndef TOLUA_DISABLE_tolua_get_AStarKnowUnseenTerrain
static int tolua_get_AStarKnowUnseenTerrain(lua_State* tolua_S)
{
  tolua_pushboolean(tolua_S,(bool)AStarKnowUnseenTerrain);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AStarKnowUnseenTerrain */
#ifndef TOLUA_DISABLE_tolua_set_AStarKnowUnseenTerrain
static int tolua_set_AStarKnowUnseenTerrain(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  AStarKnowUnseenTerrain = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AStarUnknownTerrainCost */
#ifndef TOLUA_DISABLE_tolua_get_AStarUnknownTerrainCost
static int tolua_get_AStarUnknownTerrainCost(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)GetAStarUnknownTerrainCost());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AStarUnknownTerrainCost */
#ifndef TOLUA_DISABLE_tolua_set_AStarUnknownTerrainCost
static int tolua_set_AStarUnknownTerrainCost(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  SetAStarUnknownTerrainCost(((int)  tolua_tonumber(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Index of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Index
static int tolua_get_CPlayer_Index(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Index'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Index);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Index of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Index
static int tolua_set_CPlayer_Index(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Index'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Index = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Name of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Name
static int tolua_get_CPlayer_Name(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Name);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Name of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Name
static int tolua_set_CPlayer_Name(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Name = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Type of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Type
static int tolua_get_CPlayer_Type(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Type'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Type);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Type of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Type
static int tolua_set_CPlayer_Type(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Type'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Type = ((PlayerTypes) (int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Race of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Race
static int tolua_get_CPlayer_Race(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Race'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Race);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Race of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Race
static int tolua_set_CPlayer_Race(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Race'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Race = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AiName of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_AiName
static int tolua_get_CPlayer_AiName(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiName'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->AiName);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AiName of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_AiName
static int tolua_set_CPlayer_AiName(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiName'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->AiName = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: StartPos of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_StartPos
static int tolua_get_CPlayer_StartPos(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'StartPos'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->StartPos,"Vec2i");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: StartPos of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_StartPos
static int tolua_set_CPlayer_StartPos(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'StartPos'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Vec2i",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->StartPos = *((Vec2i*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: SetStartView of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_SetStartView00
static int tolua_stratagus_CPlayer_SetStartView00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const Vec2i",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const Vec2i* pos = ((const Vec2i*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'SetStartView'", NULL);
#endif
  {
   self->SetStartView(*pos);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetStartView'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Resources of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_Resources
static int tolua_get_stratagus_CPlayer_Resources(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->Resources[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Resources of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPlayer_Resources
static int tolua_set_stratagus_CPlayer_Resources(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Resources[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: StoredResources of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_StoredResources
static int tolua_get_stratagus_CPlayer_StoredResources(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->StoredResources[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: StoredResources of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPlayer_StoredResources
static int tolua_set_stratagus_CPlayer_StoredResources(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->StoredResources[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Incomes of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_Incomes
static int tolua_get_stratagus_CPlayer_Incomes(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->Incomes[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Incomes of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPlayer_Incomes
static int tolua_set_stratagus_CPlayer_Incomes(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Incomes[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Revenue of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_Revenue
static int tolua_get_stratagus_CPlayer_Revenue(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->Revenue[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitTypesCount of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_UnitTypesCount
static int tolua_get_stratagus_CPlayer_UnitTypesCount(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=UnitTypeMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->UnitTypesCount[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitTypesAiActiveCount of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_UnitTypesAiActiveCount
static int tolua_get_stratagus_CPlayer_UnitTypesAiActiveCount(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=UnitTypeMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->UnitTypesAiActiveCount[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AiEnabled of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_AiEnabled
static int tolua_get_CPlayer_AiEnabled(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiEnabled'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->AiEnabled);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AiEnabled of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_AiEnabled
static int tolua_set_CPlayer_AiEnabled(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiEnabled'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->AiEnabled = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NumBuildings of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_NumBuildings
static int tolua_get_CPlayer_NumBuildings(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NumBuildings'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->NumBuildings);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NumBuildings of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_NumBuildings
static int tolua_set_CPlayer_NumBuildings(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NumBuildings'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NumBuildings = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Supply of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Supply
static int tolua_get_CPlayer_Supply(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Supply'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Supply);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Supply of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Supply
static int tolua_set_CPlayer_Supply(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Supply'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Supply = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Demand of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Demand
static int tolua_get_CPlayer_Demand(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Demand'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Demand);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Demand of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Demand
static int tolua_set_CPlayer_Demand(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Demand'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Demand = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitLimit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_UnitLimit
static int tolua_get_CPlayer_UnitLimit(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UnitLimit'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->UnitLimit);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: UnitLimit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_UnitLimit
static int tolua_set_CPlayer_UnitLimit(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UnitLimit'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->UnitLimit = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: BuildingLimit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_BuildingLimit
static int tolua_get_CPlayer_BuildingLimit(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'BuildingLimit'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->BuildingLimit);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: BuildingLimit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_BuildingLimit
static int tolua_set_CPlayer_BuildingLimit(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'BuildingLimit'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->BuildingLimit = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TotalUnitLimit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_TotalUnitLimit
static int tolua_get_CPlayer_TotalUnitLimit(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalUnitLimit'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TotalUnitLimit);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TotalUnitLimit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_TotalUnitLimit
static int tolua_set_CPlayer_TotalUnitLimit(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalUnitLimit'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TotalUnitLimit = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Score of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_Score
static int tolua_get_CPlayer_Score(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Score'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Score);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Score of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_Score
static int tolua_set_CPlayer_Score(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Score'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Score = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TotalUnits of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_TotalUnits
static int tolua_get_CPlayer_TotalUnits(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalUnits'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TotalUnits);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TotalUnits of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_TotalUnits
static int tolua_set_CPlayer_TotalUnits(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalUnits'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TotalUnits = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TotalBuildings of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_TotalBuildings
static int tolua_get_CPlayer_TotalBuildings(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalBuildings'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TotalBuildings);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TotalBuildings of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_TotalBuildings
static int tolua_set_CPlayer_TotalBuildings(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalBuildings'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TotalBuildings = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TotalResources of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_TotalResources
static int tolua_get_stratagus_CPlayer_TotalResources(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->TotalResources[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TotalResources of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPlayer_TotalResources
static int tolua_set_stratagus_CPlayer_TotalResources(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->TotalResources[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TotalRazings of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_TotalRazings
static int tolua_get_CPlayer_TotalRazings(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalRazings'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TotalRazings);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TotalRazings of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_TotalRazings
static int tolua_set_CPlayer_TotalRazings(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalRazings'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TotalRazings = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TotalKills of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_TotalKills
static int tolua_get_CPlayer_TotalKills(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalKills'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TotalKills);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TotalKills of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_TotalKills
static int tolua_set_CPlayer_TotalKills(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TotalKills'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TotalKills = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SpeedResourcesHarvest of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_SpeedResourcesHarvest
static int tolua_get_stratagus_CPlayer_SpeedResourcesHarvest(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->SpeedResourcesHarvest[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SpeedResourcesHarvest of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPlayer_SpeedResourcesHarvest
static int tolua_set_stratagus_CPlayer_SpeedResourcesHarvest(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->SpeedResourcesHarvest[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SpeedResourcesReturn of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPlayer_SpeedResourcesReturn
static int tolua_get_stratagus_CPlayer_SpeedResourcesReturn(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->SpeedResourcesReturn[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SpeedResourcesReturn of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPlayer_SpeedResourcesReturn
static int tolua_set_stratagus_CPlayer_SpeedResourcesReturn(lua_State* tolua_S)
{
 int tolua_index;
  CPlayer* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPlayer*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->SpeedResourcesReturn[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SpeedBuild of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_SpeedBuild
static int tolua_get_CPlayer_SpeedBuild(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedBuild'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->SpeedBuild);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SpeedBuild of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_SpeedBuild
static int tolua_set_CPlayer_SpeedBuild(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedBuild'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SpeedBuild = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SpeedTrain of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_SpeedTrain
static int tolua_get_CPlayer_SpeedTrain(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedTrain'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->SpeedTrain);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SpeedTrain of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_SpeedTrain
static int tolua_set_CPlayer_SpeedTrain(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedTrain'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SpeedTrain = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SpeedUpgrade of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_SpeedUpgrade
static int tolua_get_CPlayer_SpeedUpgrade(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedUpgrade'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->SpeedUpgrade);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SpeedUpgrade of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_SpeedUpgrade
static int tolua_set_CPlayer_SpeedUpgrade(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedUpgrade'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SpeedUpgrade = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SpeedResearch of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_get_CPlayer_SpeedResearch
static int tolua_get_CPlayer_SpeedResearch(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedResearch'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->SpeedResearch);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SpeedResearch of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_set_CPlayer_SpeedResearch
static int tolua_set_CPlayer_SpeedResearch(lua_State* tolua_S)
{
  CPlayer* self = (CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SpeedResearch'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SpeedResearch = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: GetUnit of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_GetUnit00
static int tolua_stratagus_CPlayer_GetUnit00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'GetUnit'", NULL);
#endif
  {
   CUnit& tolua_ret = (CUnit&)  self->GetUnit(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUnit");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetUnit'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: GetUnitCount of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_GetUnitCount00
static int tolua_stratagus_CPlayer_GetUnitCount00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'GetUnitCount'", NULL);
#endif
  {
   int tolua_ret = (int)  self->GetUnitCount();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetUnitCount'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsEnemy of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_IsEnemy00
static int tolua_stratagus_CPlayer_IsEnemy00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CPlayer",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CPlayer* player = ((const CPlayer*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsEnemy'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsEnemy(*player);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsEnemy'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsEnemy of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_IsEnemy01
static int tolua_stratagus_CPlayer_IsEnemy01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CUnit",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CUnit* unit = ((const CUnit*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsEnemy'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsEnemy(*unit);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_CPlayer_IsEnemy00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsAllied of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_IsAllied00
static int tolua_stratagus_CPlayer_IsAllied00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CPlayer",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CPlayer* player = ((const CPlayer*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsAllied'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsAllied(*player);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsAllied'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsAllied of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_IsAllied01
static int tolua_stratagus_CPlayer_IsAllied01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CUnit",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CUnit* unit = ((const CUnit*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsAllied'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsAllied(*unit);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_CPlayer_IsAllied00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: HasSharedVisionWith of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_HasSharedVisionWith00
static int tolua_stratagus_CPlayer_HasSharedVisionWith00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CPlayer",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CPlayer* player = ((const CPlayer*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'HasSharedVisionWith'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->HasSharedVisionWith(*player);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'HasSharedVisionWith'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsTeamed of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_IsTeamed00
static int tolua_stratagus_CPlayer_IsTeamed00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CPlayer",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CPlayer* player = ((const CPlayer*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsTeamed'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsTeamed(*player);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsTeamed'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsTeamed of class  CPlayer */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayer_IsTeamed01
static int tolua_stratagus_CPlayer_IsTeamed01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CPlayer",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"const CUnit",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  const CPlayer* self = (const CPlayer*)  tolua_tousertype(tolua_S,1,0);
  const CUnit* unit = ((const CUnit*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsTeamed'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsTeamed(*unit);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_CPlayer_IsTeamed00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Players */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_Players
static int tolua_get_stratagus_Players(lua_State* tolua_S)
{
 int tolua_index;
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=PlayerMax)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  tolua_pushusertype(tolua_S,(void*)&Players[tolua_index],"const CPlayer");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ThisPlayer */
#ifndef TOLUA_DISABLE_tolua_get_ThisPlayer_ptr
static int tolua_get_ThisPlayer_ptr(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)ThisPlayer,"CPlayer");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetEffectsVolume */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetEffectsVolume00
static int tolua_stratagus_GetEffectsVolume00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetEffectsVolume();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetEffectsVolume'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetEffectsVolume */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetEffectsVolume00
static int tolua_stratagus_SetEffectsVolume00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int volume = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   SetEffectsVolume(volume);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetEffectsVolume'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetMusicVolume */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetMusicVolume00
static int tolua_stratagus_GetMusicVolume00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetMusicVolume();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetMusicVolume'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetMusicVolume */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetMusicVolume00
static int tolua_stratagus_SetMusicVolume00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int volume = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   SetMusicVolume(volume);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetMusicVolume'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetEffectsEnabled */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetEffectsEnabled00
static int tolua_stratagus_SetEffectsEnabled00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool enabled = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetEffectsEnabled(enabled);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetEffectsEnabled'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: IsEffectsEnabled */
#ifndef TOLUA_DISABLE_tolua_stratagus_IsEffectsEnabled00
static int tolua_stratagus_IsEffectsEnabled00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  IsEffectsEnabled();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsEffectsEnabled'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetMusicEnabled */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetMusicEnabled00
static int tolua_stratagus_SetMusicEnabled00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool enabled = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetMusicEnabled(enabled);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetMusicEnabled'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: IsMusicEnabled */
#ifndef TOLUA_DISABLE_tolua_stratagus_IsMusicEnabled00
static int tolua_stratagus_IsMusicEnabled00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  IsMusicEnabled();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsMusicEnabled'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: PlayFile */
#ifndef TOLUA_DISABLE_tolua_stratagus_PlayFile00
static int tolua_stratagus_PlayFile00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isusertype(tolua_S,2,"LuaActionListener",1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string name = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  LuaActionListener* listener = ((LuaActionListener*)  tolua_tousertype(tolua_S,2,NULL));
  {
   int tolua_ret = (int)  PlayFile(name,listener);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'PlayFile'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: PlayMusic */
#ifndef TOLUA_DISABLE_tolua_stratagus_PlayMusic00
static int tolua_stratagus_PlayMusic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string name = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   int tolua_ret = (int)  PlayMusic(name);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'PlayMusic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: StopMusic */
#ifndef TOLUA_DISABLE_tolua_stratagus_StopMusic00
static int tolua_stratagus_StopMusic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   StopMusic();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StopMusic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: IsMusicPlaying */
#ifndef TOLUA_DISABLE_tolua_stratagus_IsMusicPlaying00
static int tolua_stratagus_IsMusicPlaying00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  IsMusicPlaying();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsMusicPlaying'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetChannelVolume */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetChannelVolume00
static int tolua_stratagus_SetChannelVolume00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int channel = ((int)  tolua_tonumber(tolua_S,1,0));
  int volume = ((int)  tolua_tonumber(tolua_S,2,0));
  {
   int tolua_ret = (int)  SetChannelVolume(channel,volume);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetChannelVolume'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetChannelStereo */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetChannelStereo00
static int tolua_stratagus_SetChannelStereo00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int channel = ((int)  tolua_tonumber(tolua_S,1,0));
  int stereo = ((int)  tolua_tonumber(tolua_S,2,0));
  {
   SetChannelStereo(channel,stereo);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetChannelStereo'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: StopChannel */
#ifndef TOLUA_DISABLE_tolua_stratagus_StopChannel00
static int tolua_stratagus_StopChannel00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int channel = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   StopChannel(channel);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StopChannel'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: StopAllChannels */
#ifndef TOLUA_DISABLE_tolua_stratagus_StopAllChannels00
static int tolua_stratagus_StopAllChannels00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   StopAllChannels();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'StopAllChannels'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: Translate */
#ifndef TOLUA_DISABLE_tolua_stratagus_Translate00
static int tolua_stratagus_Translate00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  std::string str = ((std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   std::string tolua_ret = (std::string)  Translate(str);
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Translate'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: LoadPO */
#ifndef TOLUA_DISABLE_tolua_stratagus_LoadPO00
static int tolua_stratagus_LoadPO00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  std::string file = ((std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   LoadPO(file);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'LoadPO'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetTranslationsFiles */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetTranslationsFiles00
static int tolua_stratagus_SetTranslationsFiles00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  std::string stratagusfile = ((std::string)  tolua_tocppstring(tolua_S,1,0));
  std::string gamefile = ((std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   SetTranslationsFiles(stratagusfile,gamefile);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetTranslationsFiles'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetNumOpponents */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetNumOpponents00
static int tolua_stratagus_GetNumOpponents00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int player = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   int tolua_ret = (int)  GetNumOpponents(player);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetNumOpponents'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetTimer */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetTimer00
static int tolua_stratagus_GetTimer00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetTimer();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetTimer'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ActionVictory */
#ifndef TOLUA_DISABLE_tolua_stratagus_ActionVictory00
static int tolua_stratagus_ActionVictory00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ActionVictory();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ActionVictory'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ActionDefeat */
#ifndef TOLUA_DISABLE_tolua_stratagus_ActionDefeat00
static int tolua_stratagus_ActionDefeat00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ActionDefeat();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ActionDefeat'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ActionDraw */
#ifndef TOLUA_DISABLE_tolua_stratagus_ActionDraw00
static int tolua_stratagus_ActionDraw00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ActionDraw();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ActionDraw'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ActionSetTimer */
#ifndef TOLUA_DISABLE_tolua_stratagus_ActionSetTimer00
static int tolua_stratagus_ActionSetTimer00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isboolean(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int cycles = ((int)  tolua_tonumber(tolua_S,1,0));
  bool increasing = ((bool)  tolua_toboolean(tolua_S,2,0));
  {
   ActionSetTimer(cycles,increasing);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ActionSetTimer'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ActionStartTimer */
#ifndef TOLUA_DISABLE_tolua_stratagus_ActionStartTimer00
static int tolua_stratagus_ActionStartTimer00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ActionStartTimer();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ActionStartTimer'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ActionStopTimer */
#ifndef TOLUA_DISABLE_tolua_stratagus_ActionStopTimer00
static int tolua_stratagus_ActionStopTimer00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ActionStopTimer();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ActionStopTimer'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetTrigger */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetTrigger00
static int tolua_stratagus_SetTrigger00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int trigger = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   SetTrigger(trigger);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetTrigger'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUIButton_new00
static int tolua_stratagus_CUIButton_new00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CUIButton",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CUIButton* tolua_ret = (CUIButton*)  Mtolua_new((CUIButton)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUIButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new_local of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUIButton_new00_local
static int tolua_stratagus_CUIButton_new00_local(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CUIButton",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CUIButton* tolua_ret = (CUIButton*)  Mtolua_new((CUIButton)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUIButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_get_CUIButton_X
static int tolua_get_CUIButton_X(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_set_CUIButton_X
static int tolua_set_CUIButton_X(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_get_CUIButton_Y
static int tolua_get_CUIButton_Y(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_set_CUIButton_Y
static int tolua_set_CUIButton_Y(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Text of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_get_CUIButton_Text
static int tolua_get_CUIButton_Text(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Text'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Text);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Text of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_set_CUIButton_Text
static int tolua_set_CUIButton_Text(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Text'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Text = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Style of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_get_CUIButton_Style_ptr
static int tolua_get_CUIButton_Style_ptr(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Style'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Style,"ButtonStyle");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Style of class  CUIButton */
#ifndef TOLUA_DISABLE_tolua_set_CUIButton_Style_ptr
static int tolua_set_CUIButton_Style_ptr(lua_State* tolua_S)
{
  CUIButton* self = (CUIButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Style'",NULL);
  if (!tolua_isusertype(tolua_S,2,"ButtonStyle",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Style = ((ButtonStyle*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_X
static int tolua_get_CMapArea_X(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_X
static int tolua_set_CMapArea_X(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_Y
static int tolua_get_CMapArea_Y(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_Y
static int tolua_set_CMapArea_Y(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: EndX of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_EndX
static int tolua_get_CMapArea_EndX(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EndX'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->EndX);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: EndX of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_EndX
static int tolua_set_CMapArea_EndX(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EndX'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->EndX = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: EndY of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_EndY
static int tolua_get_CMapArea_EndY(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EndY'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->EndY);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: EndY of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_EndY
static int tolua_set_CMapArea_EndY(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EndY'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->EndY = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ScrollPaddingLeft of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_ScrollPaddingLeft
static int tolua_get_CMapArea_ScrollPaddingLeft(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingLeft'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ScrollPaddingLeft);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ScrollPaddingLeft of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_ScrollPaddingLeft
static int tolua_set_CMapArea_ScrollPaddingLeft(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingLeft'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ScrollPaddingLeft = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ScrollPaddingRight of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_ScrollPaddingRight
static int tolua_get_CMapArea_ScrollPaddingRight(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingRight'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ScrollPaddingRight);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ScrollPaddingRight of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_ScrollPaddingRight
static int tolua_set_CMapArea_ScrollPaddingRight(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingRight'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ScrollPaddingRight = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ScrollPaddingTop of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_ScrollPaddingTop
static int tolua_get_CMapArea_ScrollPaddingTop(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingTop'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ScrollPaddingTop);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ScrollPaddingTop of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_ScrollPaddingTop
static int tolua_set_CMapArea_ScrollPaddingTop(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingTop'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ScrollPaddingTop = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ScrollPaddingBottom of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_get_CMapArea_ScrollPaddingBottom
static int tolua_get_CMapArea_ScrollPaddingBottom(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingBottom'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ScrollPaddingBottom);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ScrollPaddingBottom of class  CMapArea */
#ifndef TOLUA_DISABLE_tolua_set_CMapArea_ScrollPaddingBottom
static int tolua_set_CMapArea_ScrollPaddingBottom(lua_State* tolua_S)
{
  CMapArea* self = (CMapArea*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ScrollPaddingBottom'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ScrollPaddingBottom = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: new of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFiller_new00
static int tolua_stratagus_CFiller_new00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CFiller",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CFiller* tolua_ret = (CFiller*)  Mtolua_new((CFiller)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CFiller");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new_local of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_stratagus_CFiller_new00_local
static int tolua_stratagus_CFiller_new00_local(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CFiller",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CFiller* tolua_ret = (CFiller*)  Mtolua_new((CFiller)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CFiller");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: G of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_get_CFiller_G
static int tolua_get_CFiller_G(lua_State* tolua_S)
{
  CFiller* self = (CFiller*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->G,"CGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: G of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_set_CFiller_G
static int tolua_set_CFiller_G(lua_State* tolua_S)
{
  CFiller* self = (CFiller*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->G = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_get_CFiller_X
static int tolua_get_CFiller_X(lua_State* tolua_S)
{
  CFiller* self = (CFiller*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_set_CFiller_X
static int tolua_set_CFiller_X(lua_State* tolua_S)
{
  CFiller* self = (CFiller*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_get_CFiller_Y
static int tolua_get_CFiller_Y(lua_State* tolua_S)
{
  CFiller* self = (CFiller*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CFiller */
#ifndef TOLUA_DISABLE_tolua_set_CFiller_Y
static int tolua_set_CFiller_Y(lua_State* tolua_S)
{
  CFiller* self = (CFiller*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller___geti00
static int tolua_stratagus_vector_CFiller___geti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CFiller>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CFiller>* self = (const vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   const CFiller tolua_ret = (const CFiller)  self->operator[](index);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CFiller)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"const CFiller");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(const CFiller));
     tolua_pushusertype(tolua_S,tolua_obj,"const CFiller");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.geti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator&[] of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller___seti00
static int tolua_stratagus_vector_CFiller___seti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"CFiller",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
  CFiller tolua_value = *((CFiller*)  tolua_tousertype(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator&[]'", NULL);
#endif
  self->operator[](index) =  tolua_value;
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.seti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller___geti01
static int tolua_stratagus_vector_CFiller___geti01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   CFiller tolua_ret = (CFiller)  self->operator[](index);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CFiller)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CFiller");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CFiller));
     tolua_pushusertype(tolua_S,tolua_obj,"CFiller");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CFiller___geti00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__at00
static int tolua_stratagus_vector_CFiller__at00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CFiller>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CFiller>* self = (const vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   const CFiller& tolua_ret = (const CFiller&)  self->at(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CFiller");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'at'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__at01
static int tolua_stratagus_vector_CFiller__at01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   CFiller& tolua_ret = (CFiller&)  self->at(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CFiller");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CFiller__at00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__front00
static int tolua_stratagus_vector_CFiller__front00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CFiller>* self = (const vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   const CFiller& tolua_ret = (const CFiller&)  self->front();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CFiller");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'front'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__front01
static int tolua_stratagus_vector_CFiller__front01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   CFiller& tolua_ret = (CFiller&)  self->front();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CFiller");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CFiller__front00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__back00
static int tolua_stratagus_vector_CFiller__back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CFiller>* self = (const vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   const CFiller& tolua_ret = (const CFiller&)  self->back();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CFiller");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__back01
static int tolua_stratagus_vector_CFiller__back01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   CFiller& tolua_ret = (CFiller&)  self->back();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CFiller");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CFiller__back00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: push_back of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__push_back00
static int tolua_stratagus_vector_CFiller__push_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CFiller",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  CFiller val = *((CFiller*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'push_back'", NULL);
#endif
  {
   self->push_back(val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'push_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: pop_back of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__pop_back00
static int tolua_stratagus_vector_CFiller__pop_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'pop_back'", NULL);
#endif
  {
   self->pop_back();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'pop_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: assign of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__assign00
static int tolua_stratagus_vector_CFiller__assign00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"const CFiller",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
  int num = ((int)  tolua_tonumber(tolua_S,2,0));
  const CFiller* val = ((const CFiller*)  tolua_tousertype(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'assign'", NULL);
#endif
  {
   self->assign(num,*val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'assign'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: clear of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__clear00
static int tolua_stratagus_vector_CFiller__clear00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CFiller>* self = (vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'clear'", NULL);
#endif
  {
   self->clear();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'clear'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: empty of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__empty00
static int tolua_stratagus_vector_CFiller__empty00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CFiller>* self = (const vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'empty'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->empty();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'empty'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: size of class  vector<CFiller> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CFiller__size00
static int tolua_stratagus_vector_CFiller__size00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CFiller>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CFiller>* self = (const vector<CFiller>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'size'", NULL);
#endif
  {
   int tolua_ret = (int)  self->size();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'size'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton___geti00
static int tolua_stratagus_vector_CUIButton___geti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIButton>* self = (const vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   const CUIButton tolua_ret = (const CUIButton)  self->operator[](index);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CUIButton)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"const CUIButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(const CUIButton));
     tolua_pushusertype(tolua_S,tolua_obj,"const CUIButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.geti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator&[] of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton___seti00
static int tolua_stratagus_vector_CUIButton___seti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"CUIButton",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
  CUIButton tolua_value = *((CUIButton*)  tolua_tousertype(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator&[]'", NULL);
#endif
  self->operator[](index) =  tolua_value;
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.seti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton___geti01
static int tolua_stratagus_vector_CUIButton___geti01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   CUIButton tolua_ret = (CUIButton)  self->operator[](index);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CUIButton)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CUIButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CUIButton));
     tolua_pushusertype(tolua_S,tolua_obj,"CUIButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIButton___geti00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__at00
static int tolua_stratagus_vector_CUIButton__at00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIButton>* self = (const vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   const CUIButton& tolua_ret = (const CUIButton&)  self->at(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CUIButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'at'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__at01
static int tolua_stratagus_vector_CUIButton__at01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   CUIButton& tolua_ret = (CUIButton&)  self->at(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUIButton");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIButton__at00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__front00
static int tolua_stratagus_vector_CUIButton__front00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIButton>* self = (const vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   const CUIButton& tolua_ret = (const CUIButton&)  self->front();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CUIButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'front'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__front01
static int tolua_stratagus_vector_CUIButton__front01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   CUIButton& tolua_ret = (CUIButton&)  self->front();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUIButton");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIButton__front00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__back00
static int tolua_stratagus_vector_CUIButton__back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIButton>* self = (const vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   const CUIButton& tolua_ret = (const CUIButton&)  self->back();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CUIButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__back01
static int tolua_stratagus_vector_CUIButton__back01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   CUIButton& tolua_ret = (CUIButton&)  self->back();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUIButton");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIButton__back00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: push_back of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__push_back00
static int tolua_stratagus_vector_CUIButton__push_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  CUIButton val = *((CUIButton*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'push_back'", NULL);
#endif
  {
   self->push_back(val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'push_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: pop_back of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__pop_back00
static int tolua_stratagus_vector_CUIButton__pop_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'pop_back'", NULL);
#endif
  {
   self->pop_back();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'pop_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: assign of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__assign00
static int tolua_stratagus_vector_CUIButton__assign00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"const CUIButton",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
  int num = ((int)  tolua_tonumber(tolua_S,2,0));
  const CUIButton* val = ((const CUIButton*)  tolua_tousertype(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'assign'", NULL);
#endif
  {
   self->assign(num,*val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'assign'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: clear of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__clear00
static int tolua_stratagus_vector_CUIButton__clear00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIButton>* self = (vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'clear'", NULL);
#endif
  {
   self->clear();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'clear'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: empty of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__empty00
static int tolua_stratagus_vector_CUIButton__empty00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIButton>* self = (const vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'empty'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->empty();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'empty'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: size of class  vector<CUIButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIButton__size00
static int tolua_stratagus_vector_CUIButton__size00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIButton>* self = (const vector<CUIButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'size'", NULL);
#endif
  {
   int tolua_ret = (int)  self->size();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'size'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton___geti00
static int tolua_stratagus_vector_CUIUserButton___geti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIUserButton>* self = (const vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   const CUIUserButton tolua_ret = (const CUIUserButton)  self->operator[](index);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CUIUserButton)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"const CUIUserButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(const CUIUserButton));
     tolua_pushusertype(tolua_S,tolua_obj,"const CUIUserButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.geti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator&[] of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton___seti00
static int tolua_stratagus_vector_CUIUserButton___seti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"CUIUserButton",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
  CUIUserButton tolua_value = *((CUIUserButton*)  tolua_tousertype(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator&[]'", NULL);
#endif
  self->operator[](index) =  tolua_value;
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.seti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton___geti01
static int tolua_stratagus_vector_CUIUserButton___geti01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   CUIUserButton tolua_ret = (CUIUserButton)  self->operator[](index);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CUIUserButton)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CUIUserButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CUIUserButton));
     tolua_pushusertype(tolua_S,tolua_obj,"CUIUserButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIUserButton___geti00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__at00
static int tolua_stratagus_vector_CUIUserButton__at00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIUserButton>* self = (const vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   const CUIUserButton& tolua_ret = (const CUIUserButton&)  self->at(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CUIUserButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'at'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__at01
static int tolua_stratagus_vector_CUIUserButton__at01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   CUIUserButton& tolua_ret = (CUIUserButton&)  self->at(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUIUserButton");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIUserButton__at00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__front00
static int tolua_stratagus_vector_CUIUserButton__front00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIUserButton>* self = (const vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   const CUIUserButton& tolua_ret = (const CUIUserButton&)  self->front();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CUIUserButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'front'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__front01
static int tolua_stratagus_vector_CUIUserButton__front01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   CUIUserButton& tolua_ret = (CUIUserButton&)  self->front();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUIUserButton");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIUserButton__front00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__back00
static int tolua_stratagus_vector_CUIUserButton__back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIUserButton>* self = (const vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   const CUIUserButton& tolua_ret = (const CUIUserButton&)  self->back();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"const CUIUserButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__back01
static int tolua_stratagus_vector_CUIUserButton__back01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   CUIUserButton& tolua_ret = (CUIUserButton&)  self->back();
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUIUserButton");
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_CUIUserButton__back00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: push_back of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__push_back00
static int tolua_stratagus_vector_CUIUserButton__push_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUIUserButton",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  CUIUserButton val = *((CUIUserButton*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'push_back'", NULL);
#endif
  {
   self->push_back(val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'push_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: pop_back of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__pop_back00
static int tolua_stratagus_vector_CUIUserButton__pop_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'pop_back'", NULL);
#endif
  {
   self->pop_back();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'pop_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: assign of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__assign00
static int tolua_stratagus_vector_CUIUserButton__assign00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,3,&tolua_err) || !tolua_isusertype(tolua_S,3,"const CUIUserButton",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
  int num = ((int)  tolua_tonumber(tolua_S,2,0));
  const CUIUserButton* val = ((const CUIUserButton*)  tolua_tousertype(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'assign'", NULL);
#endif
  {
   self->assign(num,*val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'assign'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: clear of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__clear00
static int tolua_stratagus_vector_CUIUserButton__clear00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<CUIUserButton>* self = (vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'clear'", NULL);
#endif
  {
   self->clear();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'clear'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: empty of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__empty00
static int tolua_stratagus_vector_CUIUserButton__empty00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIUserButton>* self = (const vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'empty'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->empty();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'empty'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: size of class  vector<CUIUserButton> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_CUIUserButton__size00
static int tolua_stratagus_vector_CUIUserButton__size00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<CUIUserButton>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<CUIUserButton>* self = (const vector<CUIUserButton>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'size'", NULL);
#endif
  {
   int tolua_ret = (int)  self->size();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'size'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string___geti00
static int tolua_stratagus_vector_string___geti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<string>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<string>* self = (const vector<string>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   const string tolua_ret = (const string)  self->operator[](index);
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.geti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator&[] of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string___seti00
static int tolua_stratagus_vector_string___seti00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
  string tolua_value = ((string)  tolua_tocppstring(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator&[]'", NULL);
#endif
  self->operator[](index) =  tolua_value;
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '.seti'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: operator[] of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string___geti01
static int tolua_stratagus_vector_string___geti01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'operator[]'", NULL);
#endif
  {
   string tolua_ret = (string)  self->operator[](index);
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_string___geti00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__at00
static int tolua_stratagus_vector_string__at00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<string>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<string>* self = (const vector<string>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   const string tolua_ret = (const string)  self->at(index);
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'at'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: at of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__at01
static int tolua_stratagus_vector_string__at01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'at'", NULL);
#endif
  {
   string tolua_ret = (string)  self->at(index);
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_string__at00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__front00
static int tolua_stratagus_vector_string__front00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<string>* self = (const vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   const string tolua_ret = (const string)  self->front();
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'front'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: front of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__front01
static int tolua_stratagus_vector_string__front01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'front'", NULL);
#endif
  {
   string tolua_ret = (string)  self->front();
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_string__front00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__back00
static int tolua_stratagus_vector_string__back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<string>* self = (const vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   const string tolua_ret = (const string)  self->back();
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: back of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__back01
static int tolua_stratagus_vector_string__back01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'back'", NULL);
#endif
  {
   string tolua_ret = (string)  self->back();
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_vector_string__back00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* method: push_back of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__push_back00
static int tolua_stratagus_vector_string__push_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
  string val = ((string)  tolua_tocppstring(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'push_back'", NULL);
#endif
  {
   self->push_back(val);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'push_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: pop_back of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__pop_back00
static int tolua_stratagus_vector_string__pop_back00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'pop_back'", NULL);
#endif
  {
   self->pop_back();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'pop_back'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: assign of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__assign00
static int tolua_stratagus_vector_string__assign00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
  int num = ((int)  tolua_tonumber(tolua_S,2,0));
  const string val = ((const string)  tolua_tocppstring(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'assign'", NULL);
#endif
  {
   self->assign(num,val);
   tolua_pushcppstring(tolua_S,(const char*)val);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'assign'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: clear of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__clear00
static int tolua_stratagus_vector_string__clear00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  vector<string>* self = (vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'clear'", NULL);
#endif
  {
   self->clear();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'clear'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: empty of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__empty00
static int tolua_stratagus_vector_string__empty00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<string>* self = (const vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'empty'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->empty();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'empty'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: size of class  vector<string> */
#ifndef TOLUA_DISABLE_tolua_stratagus_vector_string__size00
static int tolua_stratagus_vector_string__size00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const vector<string>",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const vector<string>* self = (const vector<string>*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'size'", NULL);
#endif
  {
   int tolua_ret = (int)  self->size();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'size'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_get_CButtonPanel_X
static int tolua_get_CButtonPanel_X(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_set_CButtonPanel_X
static int tolua_set_CButtonPanel_X(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_get_CButtonPanel_Y
static int tolua_get_CButtonPanel_Y(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_set_CButtonPanel_Y
static int tolua_set_CButtonPanel_Y(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Buttons of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_get_CButtonPanel_Buttons
static int tolua_get_CButtonPanel_Buttons(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Buttons'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Buttons,"vector<CUIButton>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Buttons of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_set_CButtonPanel_Buttons
static int tolua_set_CButtonPanel_Buttons(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Buttons'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<CUIButton>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Buttons = *((vector<CUIButton>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AutoCastBorderColorRGB of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_get_CButtonPanel_AutoCastBorderColorRGB
static int tolua_get_CButtonPanel_AutoCastBorderColorRGB(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AutoCastBorderColorRGB'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->AutoCastBorderColorRGB,"CColor");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AutoCastBorderColorRGB of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_set_CButtonPanel_AutoCastBorderColorRGB
static int tolua_set_CButtonPanel_AutoCastBorderColorRGB(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AutoCastBorderColorRGB'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CColor",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->AutoCastBorderColorRGB = *((CColor*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowCommandKey of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_get_CButtonPanel_ShowCommandKey
static int tolua_get_CButtonPanel_ShowCommandKey(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowCommandKey'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowCommandKey);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowCommandKey of class  CButtonPanel */
#ifndef TOLUA_DISABLE_tolua_set_CButtonPanel_ShowCommandKey
static int tolua_set_CButtonPanel_ShowCommandKey(lua_State* tolua_S)
{
  CButtonPanel* self = (CButtonPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowCommandKey'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowCommandKey = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: G of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_get_CPieMenu_G
static int tolua_get_CPieMenu_G(lua_State* tolua_S)
{
  CPieMenu* self = (CPieMenu*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->G,"CGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: G of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_set_CPieMenu_G
static int tolua_set_CPieMenu_G(lua_State* tolua_S)
{
  CPieMenu* self = (CPieMenu*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->G = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MouseButton of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_get_CPieMenu_MouseButton
static int tolua_get_CPieMenu_MouseButton(lua_State* tolua_S)
{
  CPieMenu* self = (CPieMenu*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MouseButton'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MouseButton);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MouseButton of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_set_CPieMenu_MouseButton
static int tolua_set_CPieMenu_MouseButton(lua_State* tolua_S)
{
  CPieMenu* self = (CPieMenu*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MouseButton'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MouseButton = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPieMenu_X
static int tolua_get_stratagus_CPieMenu_X(lua_State* tolua_S)
{
 int tolua_index;
  CPieMenu* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPieMenu*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=8)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->X[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPieMenu_X
static int tolua_set_stratagus_CPieMenu_X(lua_State* tolua_S)
{
 int tolua_index;
  CPieMenu* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPieMenu*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=8)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->X[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CPieMenu_Y
static int tolua_get_stratagus_CPieMenu_Y(lua_State* tolua_S)
{
 int tolua_index;
  CPieMenu* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPieMenu*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=8)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->Y[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CPieMenu_Y
static int tolua_set_stratagus_CPieMenu_Y(lua_State* tolua_S)
{
 int tolua_index;
  CPieMenu* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CPieMenu*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=8)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Y[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: SetRadius of class  CPieMenu */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPieMenu_SetRadius00
static int tolua_stratagus_CPieMenu_SetRadius00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CPieMenu",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CPieMenu* self = (CPieMenu*)  tolua_tousertype(tolua_S,1,0);
  int radius = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'SetRadius'", NULL);
#endif
  {
   self->SetRadius(radius);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetRadius'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: G of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_G
static int tolua_get_CResourceInfo_G(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->G,"CGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: G of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_G
static int tolua_set_CResourceInfo_G(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->G = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IconFrame of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_IconFrame
static int tolua_get_CResourceInfo_IconFrame(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconFrame'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->IconFrame);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IconFrame of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_IconFrame
static int tolua_set_CResourceInfo_IconFrame(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconFrame'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->IconFrame = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IconX of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_IconX
static int tolua_get_CResourceInfo_IconX(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconX'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->IconX);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IconX of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_IconX
static int tolua_set_CResourceInfo_IconX(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconX'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->IconX = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IconY of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_IconY
static int tolua_get_CResourceInfo_IconY(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconY'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->IconY);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IconY of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_IconY
static int tolua_set_CResourceInfo_IconY(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconY'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->IconY = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IconWidth of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_IconWidth
static int tolua_get_CResourceInfo_IconWidth(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconWidth'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->IconWidth);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IconWidth of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_IconWidth
static int tolua_set_CResourceInfo_IconWidth(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconWidth'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->IconWidth = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TextX of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_TextX
static int tolua_get_CResourceInfo_TextX(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextX'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TextX);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TextX of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_TextX
static int tolua_set_CResourceInfo_TextX(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextX'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TextX = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TextY of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_get_CResourceInfo_TextY
static int tolua_get_CResourceInfo_TextY(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextY'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TextY);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TextY of class  CResourceInfo */
#ifndef TOLUA_DISABLE_tolua_set_CResourceInfo_TextY
static int tolua_set_CResourceInfo_TextY(lua_State* tolua_S)
{
  CResourceInfo* self = (CResourceInfo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextY'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TextY = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: G of class  CInfoPanel */
#ifndef TOLUA_DISABLE_tolua_get_CInfoPanel_G
static int tolua_get_CInfoPanel_G(lua_State* tolua_S)
{
  CInfoPanel* self = (CInfoPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->G,"CGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: G of class  CInfoPanel */
#ifndef TOLUA_DISABLE_tolua_set_CInfoPanel_G
static int tolua_set_CInfoPanel_G(lua_State* tolua_S)
{
  CInfoPanel* self = (CInfoPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->G = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CInfoPanel */
#ifndef TOLUA_DISABLE_tolua_get_CInfoPanel_X
static int tolua_get_CInfoPanel_X(lua_State* tolua_S)
{
  CInfoPanel* self = (CInfoPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CInfoPanel */
#ifndef TOLUA_DISABLE_tolua_set_CInfoPanel_X
static int tolua_set_CInfoPanel_X(lua_State* tolua_S)
{
  CInfoPanel* self = (CInfoPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CInfoPanel */
#ifndef TOLUA_DISABLE_tolua_get_CInfoPanel_Y
static int tolua_get_CInfoPanel_Y(lua_State* tolua_S)
{
  CInfoPanel* self = (CInfoPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CInfoPanel */
#ifndef TOLUA_DISABLE_tolua_set_CInfoPanel_Y
static int tolua_set_CInfoPanel_Y(lua_State* tolua_S)
{
  CInfoPanel* self = (CInfoPanel*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: new of class  CUIUserButton */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUIUserButton_new00
static int tolua_stratagus_CUIUserButton_new00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CUIUserButton",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CUIUserButton* tolua_ret = (CUIUserButton*)  Mtolua_new((CUIUserButton)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUIUserButton");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new_local of class  CUIUserButton */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUIUserButton_new00_local
static int tolua_stratagus_CUIUserButton_new00_local(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CUIUserButton",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CUIUserButton* tolua_ret = (CUIUserButton*)  Mtolua_new((CUIUserButton)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUIUserButton");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Clicked of class  CUIUserButton */
#ifndef TOLUA_DISABLE_tolua_get_CUIUserButton_Clicked
static int tolua_get_CUIUserButton_Clicked(lua_State* tolua_S)
{
  CUIUserButton* self = (CUIUserButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Clicked'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->Clicked);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Clicked of class  CUIUserButton */
#ifndef TOLUA_DISABLE_tolua_set_CUIUserButton_Clicked
static int tolua_set_CUIUserButton_Clicked(lua_State* tolua_S)
{
  CUIUserButton* self = (CUIUserButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Clicked'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Clicked = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Button of class  CUIUserButton */
#ifndef TOLUA_DISABLE_tolua_get_CUIUserButton_Button
static int tolua_get_CUIUserButton_Button(lua_State* tolua_S)
{
  CUIUserButton* self = (CUIUserButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Button'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Button,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Button of class  CUIUserButton */
#ifndef TOLUA_DISABLE_tolua_set_CUIUserButton_Button
static int tolua_set_CUIUserButton_Button(lua_State* tolua_S)
{
  CUIUserButton* self = (CUIUserButton*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Button'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Button = *((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: Set of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_stratagus_CStatusLine_Set00
static int tolua_stratagus_CStatusLine_Set00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CStatusLine",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
  const std::string status = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Set'", NULL);
#endif
  {
   self->Set(status);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Set'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_stratagus_CStatusLine_Get00
static int tolua_stratagus_CStatusLine_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CStatusLine",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Get'", NULL);
#endif
  {
   const std::string tolua_ret = (const std::string)  self->Get();
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Clear of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_stratagus_CStatusLine_Clear00
static int tolua_stratagus_CStatusLine_Clear00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CStatusLine",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Clear'", NULL);
#endif
  {
   self->Clear();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Clear'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Width of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_get_CStatusLine_Width
static int tolua_get_CStatusLine_Width(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Width'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Width);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Width of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_set_CStatusLine_Width
static int tolua_set_CStatusLine_Width(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Width'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Width = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TextX of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_get_CStatusLine_TextX
static int tolua_get_CStatusLine_TextX(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextX'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TextX);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TextX of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_set_CStatusLine_TextX
static int tolua_set_CStatusLine_TextX(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextX'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TextX = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TextY of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_get_CStatusLine_TextY
static int tolua_get_CStatusLine_TextY(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextY'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TextY);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TextY of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_set_CStatusLine_TextY
static int tolua_set_CStatusLine_TextY(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TextY'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TextY = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Font of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_get_CStatusLine_Font_ptr
static int tolua_get_CStatusLine_Font_ptr(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Font'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Font,"CFont");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Font of class  CStatusLine */
#ifndef TOLUA_DISABLE_tolua_set_CStatusLine_Font_ptr
static int tolua_set_CStatusLine_Font_ptr(lua_State* tolua_S)
{
  CStatusLine* self = (CStatusLine*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Font'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CFont",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Font = ((CFont*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: X of class  CUITimer */
#ifndef TOLUA_DISABLE_tolua_get_CUITimer_X
static int tolua_get_CUITimer_X(lua_State* tolua_S)
{
  CUITimer* self = (CUITimer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->X);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: X of class  CUITimer */
#ifndef TOLUA_DISABLE_tolua_set_CUITimer_X
static int tolua_set_CUITimer_X(lua_State* tolua_S)
{
  CUITimer* self = (CUITimer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'X'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->X = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Y of class  CUITimer */
#ifndef TOLUA_DISABLE_tolua_get_CUITimer_Y
static int tolua_get_CUITimer_Y(lua_State* tolua_S)
{
  CUITimer* self = (CUITimer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Y of class  CUITimer */
#ifndef TOLUA_DISABLE_tolua_set_CUITimer_Y
static int tolua_set_CUITimer_Y(lua_State* tolua_S)
{
  CUITimer* self = (CUITimer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Y = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Font of class  CUITimer */
#ifndef TOLUA_DISABLE_tolua_get_CUITimer_Font_ptr
static int tolua_get_CUITimer_Font_ptr(lua_State* tolua_S)
{
  CUITimer* self = (CUITimer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Font'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Font,"CFont");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Font of class  CUITimer */
#ifndef TOLUA_DISABLE_tolua_set_CUITimer_Font_ptr
static int tolua_set_CUITimer_Font_ptr(lua_State* tolua_S)
{
  CUITimer* self = (CUITimer*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Font'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CFont",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Font = ((CFont*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NormalFontColor of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_NormalFontColor
static int tolua_get_CUserInterface_NormalFontColor(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NormalFontColor'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->NormalFontColor);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NormalFontColor of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_NormalFontColor
static int tolua_set_CUserInterface_NormalFontColor(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NormalFontColor'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NormalFontColor = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ReverseFontColor of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_ReverseFontColor
static int tolua_get_CUserInterface_ReverseFontColor(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ReverseFontColor'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->ReverseFontColor);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ReverseFontColor of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_ReverseFontColor
static int tolua_set_CUserInterface_ReverseFontColor(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ReverseFontColor'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ReverseFontColor = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Fillers of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_Fillers
static int tolua_get_CUserInterface_Fillers(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Fillers'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Fillers,"vector<CFiller>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Fillers of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_Fillers
static int tolua_set_CUserInterface_Fillers(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Fillers'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<CFiller>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Fillers = *((vector<CFiller>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Resources of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CUserInterface_Resources
static int tolua_get_stratagus_CUserInterface_Resources(lua_State* tolua_S)
{
 int tolua_index;
  CUserInterface* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CUserInterface*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxResourceInfo)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  tolua_pushusertype(tolua_S,(void*)&self->Resources[tolua_index],"CResourceInfo");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Resources of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CUserInterface_Resources
static int tolua_set_stratagus_CUserInterface_Resources(lua_State* tolua_S)
{
 int tolua_index;
  CUserInterface* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CUserInterface*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxResourceInfo)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Resources[tolua_index] = *((CResourceInfo*)  tolua_tousertype(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: InfoPanel of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_InfoPanel
static int tolua_get_CUserInterface_InfoPanel(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'InfoPanel'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->InfoPanel,"CInfoPanel");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: InfoPanel of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_InfoPanel
static int tolua_set_CUserInterface_InfoPanel(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'InfoPanel'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CInfoPanel",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->InfoPanel = *((CInfoPanel*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: DefaultUnitPortrait of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_DefaultUnitPortrait
static int tolua_get_CUserInterface_DefaultUnitPortrait(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'DefaultUnitPortrait'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->DefaultUnitPortrait);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: DefaultUnitPortrait of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_DefaultUnitPortrait
static int tolua_set_CUserInterface_DefaultUnitPortrait(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'DefaultUnitPortrait'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->DefaultUnitPortrait = ((string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SingleSelectedButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_SingleSelectedButton_ptr
static int tolua_get_CUserInterface_SingleSelectedButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SingleSelectedButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->SingleSelectedButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SingleSelectedButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_SingleSelectedButton_ptr
static int tolua_set_CUserInterface_SingleSelectedButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SingleSelectedButton'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SingleSelectedButton = ((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SelectedButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_SelectedButtons
static int tolua_get_CUserInterface_SelectedButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SelectedButtons'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->SelectedButtons,"vector<CUIButton>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SelectedButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_SelectedButtons
static int tolua_set_CUserInterface_SelectedButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SelectedButtons'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<CUIButton>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SelectedButtons = *((vector<CUIButton>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MaxSelectedFont of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MaxSelectedFont_ptr
static int tolua_get_CUserInterface_MaxSelectedFont_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MaxSelectedFont'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->MaxSelectedFont,"CFont");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MaxSelectedFont of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MaxSelectedFont_ptr
static int tolua_set_CUserInterface_MaxSelectedFont_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MaxSelectedFont'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CFont",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MaxSelectedFont = ((CFont*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MaxSelectedTextX of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MaxSelectedTextX
static int tolua_get_CUserInterface_MaxSelectedTextX(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MaxSelectedTextX'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MaxSelectedTextX);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MaxSelectedTextX of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MaxSelectedTextX
static int tolua_set_CUserInterface_MaxSelectedTextX(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MaxSelectedTextX'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MaxSelectedTextX = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MaxSelectedTextY of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MaxSelectedTextY
static int tolua_get_CUserInterface_MaxSelectedTextY(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MaxSelectedTextY'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MaxSelectedTextY);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MaxSelectedTextY of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MaxSelectedTextY
static int tolua_set_CUserInterface_MaxSelectedTextY(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MaxSelectedTextY'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MaxSelectedTextY = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SingleTrainingButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_SingleTrainingButton_ptr
static int tolua_get_CUserInterface_SingleTrainingButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SingleTrainingButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->SingleTrainingButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SingleTrainingButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_SingleTrainingButton_ptr
static int tolua_set_CUserInterface_SingleTrainingButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SingleTrainingButton'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SingleTrainingButton = ((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TrainingButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_TrainingButtons
static int tolua_get_CUserInterface_TrainingButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TrainingButtons'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->TrainingButtons,"vector<CUIButton>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TrainingButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_TrainingButtons
static int tolua_set_CUserInterface_TrainingButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TrainingButtons'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<CUIButton>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TrainingButtons = *((vector<CUIButton>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UpgradingButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_UpgradingButton_ptr
static int tolua_get_CUserInterface_UpgradingButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UpgradingButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->UpgradingButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: UpgradingButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_UpgradingButton_ptr
static int tolua_set_CUserInterface_UpgradingButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UpgradingButton'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->UpgradingButton = ((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ResearchingButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_ResearchingButton_ptr
static int tolua_get_CUserInterface_ResearchingButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ResearchingButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->ResearchingButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ResearchingButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_ResearchingButton_ptr
static int tolua_set_CUserInterface_ResearchingButton_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ResearchingButton'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ResearchingButton = ((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TransportingButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_TransportingButtons
static int tolua_get_CUserInterface_TransportingButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TransportingButtons'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->TransportingButtons,"vector<CUIButton>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TransportingButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_TransportingButtons
static int tolua_set_CUserInterface_TransportingButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TransportingButtons'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<CUIButton>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TransportingButtons = *((vector<CUIButton>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: LifeBarColorNames of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_LifeBarColorNames
static int tolua_get_CUserInterface_LifeBarColorNames(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarColorNames'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->LifeBarColorNames,"vector<string>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: LifeBarColorNames of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_LifeBarColorNames
static int tolua_set_CUserInterface_LifeBarColorNames(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarColorNames'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<string>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->LifeBarColorNames = *((vector<string>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: LifeBarPercents of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_LifeBarPercents
static int tolua_get_CUserInterface_LifeBarPercents(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarPercents'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->LifeBarPercents,"vector<int>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: LifeBarPercents of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_LifeBarPercents
static int tolua_set_CUserInterface_LifeBarPercents(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarPercents'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<int>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->LifeBarPercents = *((vector<int>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: LifeBarYOffset of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_LifeBarYOffset
static int tolua_get_CUserInterface_LifeBarYOffset(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarYOffset'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->LifeBarYOffset);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: LifeBarYOffset of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_LifeBarYOffset
static int tolua_set_CUserInterface_LifeBarYOffset(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarYOffset'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->LifeBarYOffset = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: LifeBarPadding of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_LifeBarPadding
static int tolua_get_CUserInterface_LifeBarPadding(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarPadding'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->LifeBarPadding);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: LifeBarPadding of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_LifeBarPadding
static int tolua_set_CUserInterface_LifeBarPadding(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarPadding'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->LifeBarPadding = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: LifeBarBorder of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_LifeBarBorder
static int tolua_get_CUserInterface_LifeBarBorder(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarBorder'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->LifeBarBorder);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: LifeBarBorder of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_LifeBarBorder
static int tolua_set_CUserInterface_LifeBarBorder(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'LifeBarBorder'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->LifeBarBorder = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: CompletedBarColorRGB of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_CompletedBarColorRGB
static int tolua_get_CUserInterface_CompletedBarColorRGB(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'CompletedBarColorRGB'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->CompletedBarColorRGB,"CColor");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: CompletedBarColorRGB of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_CompletedBarColorRGB
static int tolua_set_CUserInterface_CompletedBarColorRGB(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'CompletedBarColorRGB'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CColor",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->CompletedBarColorRGB = *((CColor*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: CompletedBarShadow of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_CompletedBarShadow
static int tolua_get_CUserInterface_CompletedBarShadow(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'CompletedBarShadow'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->CompletedBarShadow);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: CompletedBarShadow of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_CompletedBarShadow
static int tolua_set_CUserInterface_CompletedBarShadow(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'CompletedBarShadow'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->CompletedBarShadow = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ButtonPanel of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_ButtonPanel
static int tolua_get_CUserInterface_ButtonPanel(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ButtonPanel'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->ButtonPanel,"CButtonPanel");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ButtonPanel of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_ButtonPanel
static int tolua_set_CUserInterface_ButtonPanel(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ButtonPanel'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CButtonPanel",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ButtonPanel = *((CButtonPanel*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PieMenu of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_PieMenu
static int tolua_get_CUserInterface_PieMenu(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PieMenu'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->PieMenu,"CPieMenu");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PieMenu of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_PieMenu
static int tolua_set_CUserInterface_PieMenu(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PieMenu'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CPieMenu",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->PieMenu = *((CPieMenu*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MouseViewport of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MouseViewport_ptr
static int tolua_get_CUserInterface_MouseViewport_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MouseViewport'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->MouseViewport,"CViewport");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MouseViewport of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MouseViewport_ptr
static int tolua_set_CUserInterface_MouseViewport_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MouseViewport'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CViewport",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MouseViewport = ((CViewport*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MapArea of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MapArea
static int tolua_get_CUserInterface_MapArea(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MapArea'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->MapArea,"CMapArea");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MapArea of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MapArea
static int tolua_set_CUserInterface_MapArea(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MapArea'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CMapArea",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MapArea = *((CMapArea*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MessageFont of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MessageFont_ptr
static int tolua_get_CUserInterface_MessageFont_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MessageFont'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->MessageFont,"CFont");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MessageFont of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MessageFont_ptr
static int tolua_set_CUserInterface_MessageFont_ptr(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MessageFont'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CFont",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MessageFont = ((CFont*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MessageScrollSpeed of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MessageScrollSpeed
static int tolua_get_CUserInterface_MessageScrollSpeed(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MessageScrollSpeed'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MessageScrollSpeed);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MessageScrollSpeed of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MessageScrollSpeed
static int tolua_set_CUserInterface_MessageScrollSpeed(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MessageScrollSpeed'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MessageScrollSpeed = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MenuButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_MenuButton
static int tolua_get_CUserInterface_MenuButton(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MenuButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->MenuButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MenuButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_MenuButton
static int tolua_set_CUserInterface_MenuButton(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MenuButton'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MenuButton = *((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetworkMenuButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_NetworkMenuButton
static int tolua_get_CUserInterface_NetworkMenuButton(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NetworkMenuButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->NetworkMenuButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NetworkMenuButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_NetworkMenuButton
static int tolua_set_CUserInterface_NetworkMenuButton(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NetworkMenuButton'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NetworkMenuButton = *((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NetworkDiplomacyButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_NetworkDiplomacyButton
static int tolua_get_CUserInterface_NetworkDiplomacyButton(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NetworkDiplomacyButton'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->NetworkDiplomacyButton,"CUIButton");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NetworkDiplomacyButton of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_NetworkDiplomacyButton
static int tolua_set_CUserInterface_NetworkDiplomacyButton(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NetworkDiplomacyButton'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUIButton",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NetworkDiplomacyButton = *((CUIButton*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UserButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_UserButtons
static int tolua_get_CUserInterface_UserButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UserButtons'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->UserButtons,"vector<CUIUserButton>");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: UserButtons of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_UserButtons
static int tolua_set_CUserInterface_UserButtons(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'UserButtons'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"vector<CUIUserButton>",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->UserButtons = *((vector<CUIUserButton>*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Minimap of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_Minimap
static int tolua_get_CUserInterface_Minimap(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Minimap'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Minimap,"CMinimap");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Minimap of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_Minimap
static int tolua_set_CUserInterface_Minimap(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Minimap'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CMinimap",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Minimap = *((CMinimap*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: StatusLine of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_StatusLine
static int tolua_get_CUserInterface_StatusLine(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'StatusLine'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->StatusLine,"CStatusLine");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: StatusLine of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_StatusLine
static int tolua_set_CUserInterface_StatusLine(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'StatusLine'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CStatusLine",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->StatusLine = *((CStatusLine*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Timer of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_Timer
static int tolua_get_CUserInterface_Timer(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Timer'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->Timer,"CUITimer");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Timer of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_Timer
static int tolua_set_CUserInterface_Timer(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Timer'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CUITimer",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Timer = *((CUITimer*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: EditorSettingsAreaTopLeft of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_EditorSettingsAreaTopLeft
static int tolua_get_CUserInterface_EditorSettingsAreaTopLeft(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorSettingsAreaTopLeft'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->EditorSettingsAreaTopLeft,"Vec2i");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: EditorSettingsAreaTopLeft of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_EditorSettingsAreaTopLeft
static int tolua_set_CUserInterface_EditorSettingsAreaTopLeft(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorSettingsAreaTopLeft'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Vec2i",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->EditorSettingsAreaTopLeft = *((Vec2i*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: EditorSettingsAreaBottomRight of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_EditorSettingsAreaBottomRight
static int tolua_get_CUserInterface_EditorSettingsAreaBottomRight(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorSettingsAreaBottomRight'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->EditorSettingsAreaBottomRight,"Vec2i");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: EditorSettingsAreaBottomRight of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_EditorSettingsAreaBottomRight
static int tolua_set_CUserInterface_EditorSettingsAreaBottomRight(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorSettingsAreaBottomRight'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Vec2i",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->EditorSettingsAreaBottomRight = *((Vec2i*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: EditorButtonAreaTopLeft of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_EditorButtonAreaTopLeft
static int tolua_get_CUserInterface_EditorButtonAreaTopLeft(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorButtonAreaTopLeft'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->EditorButtonAreaTopLeft,"Vec2i");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: EditorButtonAreaTopLeft of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_EditorButtonAreaTopLeft
static int tolua_set_CUserInterface_EditorButtonAreaTopLeft(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorButtonAreaTopLeft'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Vec2i",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->EditorButtonAreaTopLeft = *((Vec2i*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: EditorButtonAreaBottomRight of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_get_CUserInterface_EditorButtonAreaBottomRight
static int tolua_get_CUserInterface_EditorButtonAreaBottomRight(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorButtonAreaBottomRight'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->EditorButtonAreaBottomRight,"Vec2i");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: EditorButtonAreaBottomRight of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_set_CUserInterface_EditorButtonAreaBottomRight
static int tolua_set_CUserInterface_EditorButtonAreaBottomRight(lua_State* tolua_S)
{
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'EditorButtonAreaBottomRight'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"Vec2i",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->EditorButtonAreaBottomRight = *((Vec2i*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: Load of class  CUserInterface */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUserInterface_Load00
static int tolua_stratagus_CUserInterface_Load00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CUserInterface",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CUserInterface* self = (CUserInterface*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Load'", NULL);
#endif
  {
   self->Load();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Load'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UI */
#ifndef TOLUA_DISABLE_tolua_get_UI
static int tolua_get_UI(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&UI,"CUserInterface");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_stratagus_CIcon_New00
static int tolua_stratagus_CIcon_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CIcon",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CIcon* tolua_ret = (CIcon*)  CIcon::New(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CIcon");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_stratagus_CIcon_Get00
static int tolua_stratagus_CIcon_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CIcon",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CIcon* tolua_ret = (CIcon*)  CIcon::Get(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CIcon");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Ident of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_get_CIcon_Ident
static int tolua_get_CIcon_Ident(lua_State* tolua_S)
{
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Ident'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->GetIdent());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: G of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_get_CIcon_G
static int tolua_get_CIcon_G(lua_State* tolua_S)
{
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->G,"CPlayerColorGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: G of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_set_CIcon_G
static int tolua_set_CIcon_G(lua_State* tolua_S)
{
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CPlayerColorGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->G = *((CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Frame of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_get_CIcon_Frame
static int tolua_get_CIcon_Frame(lua_State* tolua_S)
{
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Frame'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Frame);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Frame of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_set_CIcon_Frame
static int tolua_set_CIcon_Frame(lua_State* tolua_S)
{
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Frame'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Frame = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: ClearExtraGraphics of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_stratagus_CIcon_ClearExtraGraphics00
static int tolua_stratagus_CIcon_ClearExtraGraphics00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CIcon",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'ClearExtraGraphics'", NULL);
#endif
  {
   self->ClearExtraGraphics();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ClearExtraGraphics'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: AddSingleSelectionGraphic of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_stratagus_CIcon_AddSingleSelectionGraphic00
static int tolua_stratagus_CIcon_AddSingleSelectionGraphic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CIcon",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CPlayerColorGraphicPtr",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
  CPlayerColorGraphicPtr g = *((CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'AddSingleSelectionGraphic'", NULL);
#endif
  {
   self->AddSingleSelectionGraphic(g);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'AddSingleSelectionGraphic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: AddGroupSelectionGraphic of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_stratagus_CIcon_AddGroupSelectionGraphic00
static int tolua_stratagus_CIcon_AddGroupSelectionGraphic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CIcon",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CPlayerColorGraphicPtr",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
  CPlayerColorGraphicPtr g = *((CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'AddGroupSelectionGraphic'", NULL);
#endif
  {
   self->AddGroupSelectionGraphic(g);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'AddGroupSelectionGraphic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: AddContainedGraphic of class  CIcon */
#ifndef TOLUA_DISABLE_tolua_stratagus_CIcon_AddContainedGraphic00
static int tolua_stratagus_CIcon_AddContainedGraphic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CIcon",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CPlayerColorGraphicPtr",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CIcon* self = (CIcon*)  tolua_tousertype(tolua_S,1,0);
  CPlayerColorGraphicPtr g = *((CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'AddContainedGraphic'", NULL);
#endif
  {
   self->AddContainedGraphic(g);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'AddContainedGraphic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: FindButtonStyle */
#ifndef TOLUA_DISABLE_tolua_stratagus_FindButtonStyle00
static int tolua_stratagus_FindButtonStyle00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string style = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   ButtonStyle* tolua_ret = (ButtonStyle*)  FindButtonStyle(style);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"ButtonStyle");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'FindButtonStyle'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetMouseScroll */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetMouseScroll00
static int tolua_stratagus_GetMouseScroll00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  GetMouseScroll();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetMouseScroll'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetMouseScroll */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetMouseScroll00
static int tolua_stratagus_SetMouseScroll00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool enabled = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetMouseScroll(enabled);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetMouseScroll'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetKeyScroll */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetKeyScroll00
static int tolua_stratagus_GetKeyScroll00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  GetKeyScroll();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetKeyScroll'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetKeyScroll */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetKeyScroll00
static int tolua_stratagus_SetKeyScroll00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool enabled = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetKeyScroll(enabled);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetKeyScroll'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetGrabMouse */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetGrabMouse00
static int tolua_stratagus_GetGrabMouse00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  GetGrabMouse();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetGrabMouse'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetGrabMouse */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetGrabMouse00
static int tolua_stratagus_SetGrabMouse00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool enabled = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetGrabMouse(enabled);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetGrabMouse'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetLeaveStops */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetLeaveStops00
static int tolua_stratagus_GetLeaveStops00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   bool tolua_ret = (bool)  GetLeaveStops();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetLeaveStops'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetLeaveStops */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetLeaveStops00
static int tolua_stratagus_SetLeaveStops00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool enabled = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetLeaveStops(enabled);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetLeaveStops'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetDoubleClickDelay */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetDoubleClickDelay00
static int tolua_stratagus_GetDoubleClickDelay00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetDoubleClickDelay();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetDoubleClickDelay'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetDoubleClickDelay */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetDoubleClickDelay00
static int tolua_stratagus_SetDoubleClickDelay00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int delay = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   SetDoubleClickDelay(delay);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetDoubleClickDelay'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetHoldClickDelay */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetHoldClickDelay00
static int tolua_stratagus_GetHoldClickDelay00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  GetHoldClickDelay();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetHoldClickDelay'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetHoldClickDelay */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetHoldClickDelay00
static int tolua_stratagus_SetHoldClickDelay00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int delay = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   SetHoldClickDelay(delay);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetHoldClickDelay'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetScrollMargins */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetScrollMargins00
static int tolua_stratagus_SetScrollMargins00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned int top = ((unsigned int)  tolua_tonumber(tolua_S,1,0));
  unsigned int right = ((unsigned int)  tolua_tonumber(tolua_S,2,0));
  unsigned int bottom = ((unsigned int)  tolua_tonumber(tolua_S,3,0));
  unsigned int left = ((unsigned int)  tolua_tonumber(tolua_S,4,0));
  {
   SetScrollMargins(top,right,bottom,left);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetScrollMargins'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: CursorScreenPos */
#ifndef TOLUA_DISABLE_tolua_get_CursorScreenPos
static int tolua_get_CursorScreenPos(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&CursorScreenPos,"PixelPos");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: CursorScreenPos */
#ifndef TOLUA_DISABLE_tolua_set_CursorScreenPos
static int tolua_set_CursorScreenPos(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"PixelPos",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  CursorScreenPos = *((PixelPos*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: x of class  Vec2i */
#ifndef TOLUA_DISABLE_tolua_get_Vec2i_x
static int tolua_get_Vec2i_x(lua_State* tolua_S)
{
  Vec2i* self = (Vec2i*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'x'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->x);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: x of class  Vec2i */
#ifndef TOLUA_DISABLE_tolua_set_Vec2i_x
static int tolua_set_Vec2i_x(lua_State* tolua_S)
{
  Vec2i* self = (Vec2i*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'x'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->x = ((short int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: y of class  Vec2i */
#ifndef TOLUA_DISABLE_tolua_get_Vec2i_y
static int tolua_get_Vec2i_y(lua_State* tolua_S)
{
  Vec2i* self = (Vec2i*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'y'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->y);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: y of class  Vec2i */
#ifndef TOLUA_DISABLE_tolua_set_Vec2i_y
static int tolua_set_Vec2i_y(lua_State* tolua_S)
{
  Vec2i* self = (Vec2i*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'y'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->y = ((short int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: tilePos of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_get_CUnit_tilePos
static int tolua_get_CUnit_tilePos(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'tilePos'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->tilePos,"Vec2i");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Type of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_get_CUnit_Type_ptr
static int tolua_get_CUnit_Type_ptr(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Type'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Type,"CUnitType");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Player of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_get_CUnit_Player_ptr
static int tolua_get_CUnit_Player_ptr(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Player'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Player,"CPlayer");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Goal of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_get_CUnit_Goal_ptr
static int tolua_get_CUnit_Goal_ptr(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Goal'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Goal,"CUnit");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Goal of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_set_CUnit_Goal_ptr
static int tolua_set_CUnit_Goal_ptr(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Goal'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CUnit",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Goal = ((CUnit*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Active of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_get_CUnit_Active
static int tolua_get_CUnit_Active(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Active'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->Active);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Active of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_set_CUnit_Active
static int tolua_set_CUnit_Active(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Active'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Active = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ResourcesHeld of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_get_CUnit_ResourcesHeld
static int tolua_get_CUnit_ResourcesHeld(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ResourcesHeld'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ResourcesHeld);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ResourcesHeld of class  CUnit */
#ifndef TOLUA_DISABLE_tolua_set_CUnit_ResourcesHeld
static int tolua_set_CUnit_ResourcesHeld(lua_State* tolua_S)
{
  CUnit* self = (CUnit*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ResourcesHeld'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ResourcesHeld = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowSightRange of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_ShowSightRange
static int tolua_get_CPreference_ShowSightRange(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowSightRange'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowSightRange);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowSightRange of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_ShowSightRange
static int tolua_set_CPreference_ShowSightRange(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowSightRange'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowSightRange = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowReactionRange of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_ShowReactionRange
static int tolua_get_CPreference_ShowReactionRange(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowReactionRange'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowReactionRange);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowReactionRange of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_ShowReactionRange
static int tolua_set_CPreference_ShowReactionRange(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowReactionRange'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowReactionRange = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowAttackRange of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_ShowAttackRange
static int tolua_get_CPreference_ShowAttackRange(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowAttackRange'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowAttackRange);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowAttackRange of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_ShowAttackRange
static int tolua_set_CPreference_ShowAttackRange(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowAttackRange'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowAttackRange = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowMessages of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_ShowMessages
static int tolua_get_CPreference_ShowMessages(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowMessages'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowMessages);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowMessages of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_ShowMessages
static int tolua_set_CPreference_ShowMessages(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowMessages'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowMessages = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowNoSelectionStats of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_ShowNoSelectionStats
static int tolua_get_CPreference_ShowNoSelectionStats(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowNoSelectionStats'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->ShowNoSelectionStats);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowNoSelectionStats of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_ShowNoSelectionStats
static int tolua_set_CPreference_ShowNoSelectionStats(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowNoSelectionStats'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowNoSelectionStats = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: BigScreen of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_BigScreen
static int tolua_get_CPreference_BigScreen(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'BigScreen'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->BigScreen);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: BigScreen of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_BigScreen
static int tolua_set_CPreference_BigScreen(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'BigScreen'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->BigScreen = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PauseOnLeave of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_PauseOnLeave
static int tolua_get_CPreference_PauseOnLeave(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PauseOnLeave'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->PauseOnLeave);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PauseOnLeave of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_PauseOnLeave
static int tolua_set_CPreference_PauseOnLeave(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PauseOnLeave'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->PauseOnLeave = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AiExplores of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_AiExplores
static int tolua_get_CPreference_AiExplores(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiExplores'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->get_AiExplores());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AiExplores of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_AiExplores
static int tolua_set_CPreference_AiExplores(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiExplores'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_AiExplores(((bool)  tolua_toboolean(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GrayscaleIcons of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_GrayscaleIcons
static int tolua_get_CPreference_GrayscaleIcons(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GrayscaleIcons'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->GrayscaleIcons);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GrayscaleIcons of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_GrayscaleIcons
static int tolua_set_CPreference_GrayscaleIcons(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GrayscaleIcons'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->GrayscaleIcons = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IconsShift of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_IconsShift
static int tolua_get_CPreference_IconsShift(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconsShift'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->IconsShift);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IconsShift of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_IconsShift
static int tolua_set_CPreference_IconsShift(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconsShift'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->IconsShift = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: StereoSound of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_StereoSound
static int tolua_get_CPreference_StereoSound(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'StereoSound'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->StereoSound);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: StereoSound of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_StereoSound
static int tolua_set_CPreference_StereoSound(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'StereoSound'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->StereoSound = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MineNotifications of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_MineNotifications
static int tolua_get_CPreference_MineNotifications(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MineNotifications'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->MineNotifications);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MineNotifications of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_MineNotifications
static int tolua_set_CPreference_MineNotifications(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MineNotifications'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MineNotifications = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: DeselectInMine of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_DeselectInMine
static int tolua_get_CPreference_DeselectInMine(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'DeselectInMine'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->DeselectInMine);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: DeselectInMine of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_DeselectInMine
static int tolua_set_CPreference_DeselectInMine(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'DeselectInMine'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->DeselectInMine = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: NoStatusLineTooltips of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_NoStatusLineTooltips
static int tolua_get_CPreference_NoStatusLineTooltips(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NoStatusLineTooltips'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->NoStatusLineTooltips);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: NoStatusLineTooltips of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_NoStatusLineTooltips
static int tolua_set_CPreference_NoStatusLineTooltips(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'NoStatusLineTooltips'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->NoStatusLineTooltips = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SimplifiedAutoTargeting of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_SimplifiedAutoTargeting
static int tolua_get_CPreference_SimplifiedAutoTargeting(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SimplifiedAutoTargeting'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->get_SimplifiedAutoTargeting());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SimplifiedAutoTargeting of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_SimplifiedAutoTargeting
static int tolua_set_CPreference_SimplifiedAutoTargeting(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SimplifiedAutoTargeting'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_SimplifiedAutoTargeting(((bool)  tolua_toboolean(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AiChecksDependencies of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_AiChecksDependencies
static int tolua_get_CPreference_AiChecksDependencies(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiChecksDependencies'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->get_AiChecksDependencies());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AiChecksDependencies of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_AiChecksDependencies
static int tolua_set_CPreference_AiChecksDependencies(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AiChecksDependencies'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_AiChecksDependencies(((bool)  tolua_toboolean(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AllyDepositsAllowed of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_AllyDepositsAllowed
static int tolua_get_CPreference_AllyDepositsAllowed(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AllyDepositsAllowed'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->get_AllyDepositsAllowed());
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AllyDepositsAllowed of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_AllyDepositsAllowed
static int tolua_set_CPreference_AllyDepositsAllowed(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AllyDepositsAllowed'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->set_AllyDepositsAllowed(((bool)  tolua_toboolean(tolua_S,2,0))
)
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: HardwareCursor of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_HardwareCursor
static int tolua_get_CPreference_HardwareCursor(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'HardwareCursor'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->HardwareCursor);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: HardwareCursor of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_HardwareCursor
static int tolua_set_CPreference_HardwareCursor(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'HardwareCursor'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->HardwareCursor = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: SelectionRectangleIndicatesDamage of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_SelectionRectangleIndicatesDamage
static int tolua_get_CPreference_SelectionRectangleIndicatesDamage(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SelectionRectangleIndicatesDamage'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->SelectionRectangleIndicatesDamage);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: SelectionRectangleIndicatesDamage of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_SelectionRectangleIndicatesDamage
static int tolua_set_CPreference_SelectionRectangleIndicatesDamage(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'SelectionRectangleIndicatesDamage'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->SelectionRectangleIndicatesDamage = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: FormationMovement of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_FormationMovement
static int tolua_get_CPreference_FormationMovement(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FormationMovement'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->FormationMovement);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: FormationMovement of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_FormationMovement
static int tolua_set_CPreference_FormationMovement(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FormationMovement'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->FormationMovement = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: FrameSkip of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_unsigned_FrameSkip
static int tolua_get_CPreference_unsigned_FrameSkip(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FrameSkip'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->FrameSkip);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: FrameSkip of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_unsigned_FrameSkip
static int tolua_set_CPreference_unsigned_FrameSkip(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FrameSkip'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->FrameSkip = ((unsigned int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowOrders of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_unsigned_ShowOrders
static int tolua_get_CPreference_unsigned_ShowOrders(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowOrders'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ShowOrders);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowOrders of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_unsigned_ShowOrders
static int tolua_set_CPreference_unsigned_ShowOrders(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowOrders'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowOrders = ((unsigned int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowNameDelay of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_unsigned_ShowNameDelay
static int tolua_get_CPreference_unsigned_ShowNameDelay(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowNameDelay'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ShowNameDelay);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowNameDelay of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_unsigned_ShowNameDelay
static int tolua_set_CPreference_unsigned_ShowNameDelay(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowNameDelay'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowNameDelay = ((unsigned int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ShowNameTime of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_unsigned_ShowNameTime
static int tolua_get_CPreference_unsigned_ShowNameTime(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowNameTime'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ShowNameTime);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ShowNameTime of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_unsigned_ShowNameTime
static int tolua_set_CPreference_unsigned_ShowNameTime(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ShowNameTime'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ShowNameTime = ((unsigned int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: AutosaveMinutes of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_unsigned_AutosaveMinutes
static int tolua_get_CPreference_unsigned_AutosaveMinutes(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AutosaveMinutes'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->AutosaveMinutes);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: AutosaveMinutes of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_unsigned_AutosaveMinutes
static int tolua_set_CPreference_unsigned_AutosaveMinutes(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'AutosaveMinutes'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->AutosaveMinutes = ((unsigned int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IconFrameG of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_IconFrameG
static int tolua_get_CPreference_IconFrameG(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconFrameG'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->IconFrameG,"CGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IconFrameG of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_IconFrameG
static int tolua_set_CPreference_IconFrameG(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'IconFrameG'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->IconFrameG = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: PressedIconFrameG of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_get_CPreference_PressedIconFrameG
static int tolua_get_CPreference_PressedIconFrameG(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PressedIconFrameG'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)&self->PressedIconFrameG,"CGraphicPtr");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: PressedIconFrameG of class  CPreference */
#ifndef TOLUA_DISABLE_tolua_set_CPreference_PressedIconFrameG
static int tolua_set_CPreference_PressedIconFrameG(lua_State* tolua_S)
{
  CPreference* self = (CPreference*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'PressedIconFrameG'",NULL);
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->PressedIconFrameG = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: GetSlotUnit of class  CUnitManager */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUnitManager_GetSlotUnit00
static int tolua_stratagus_CUnitManager_GetSlotUnit00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"const CUnitManager",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const CUnitManager* self = (const CUnitManager*)  tolua_tousertype(tolua_S,1,0);
  int index = ((int)  tolua_tonumber(tolua_S,2,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'GetSlotUnit'", NULL);
#endif
  {
   CUnit& tolua_ret = (CUnit&)  self->GetSlotUnit(index);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUnit");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetSlotUnit'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitManager */
#ifndef TOLUA_DISABLE_tolua_get_UnitManager_ptr
static int tolua_get_UnitManager_ptr(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)UnitManager,"CUnitManager");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Preference */
#ifndef TOLUA_DISABLE_tolua_get_Preference
static int tolua_get_Preference(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&Preference,"CPreference");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Preference */
#ifndef TOLUA_DISABLE_tolua_set_Preference
static int tolua_set_Preference(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if ((tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CPreference",0,&tolua_err)))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  Preference = *((CPreference*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: GetUnitUnderCursor */
#ifndef TOLUA_DISABLE_tolua_stratagus_GetUnitUnderCursor00
static int tolua_stratagus_GetUnitUnderCursor00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   CUnit* tolua_ret = (CUnit*)  GetUnitUnderCursor();
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUnit");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'GetUnitUnderCursor'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: UnitNumber */
#ifndef TOLUA_DISABLE_tolua_stratagus_UnitNumber00
static int tolua_stratagus_UnitNumber00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     (tolua_isvaluenil(tolua_S,1,&tolua_err) || !tolua_isusertype(tolua_S,1,"CUnit",0,&tolua_err)) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CUnit* unit = ((CUnit*)  tolua_tousertype(tolua_S,1,0));
  {
   int tolua_ret = (int)  UnitNumber(*unit);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'UnitNumber'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Ident of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_Ident
static int tolua_get_CUnitType_Ident(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Ident'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Ident);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Ident of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_Ident
static int tolua_set_CUnitType_Ident(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Ident'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Ident = ((std::string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Name of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_Name
static int tolua_get_CUnitType_Name(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Name);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Name of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_Name
static int tolua_set_CUnitType_Name(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Name = ((std::string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Slot of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_Slot
static int tolua_get_CUnitType_Slot(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Slot'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Slot);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MinAttackRange of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_MinAttackRange
static int tolua_get_CUnitType_MinAttackRange(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MinAttackRange'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->MinAttackRange);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MinAttackRange of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_MinAttackRange
static int tolua_set_CUnitType_MinAttackRange(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'MinAttackRange'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->MinAttackRange = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: ClicksToExplode of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_ClicksToExplode
static int tolua_get_CUnitType_ClicksToExplode(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ClicksToExplode'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->ClicksToExplode);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: ClicksToExplode of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_ClicksToExplode
static int tolua_set_CUnitType_ClicksToExplode(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'ClicksToExplode'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->ClicksToExplode = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: GivesResource of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_GivesResource
static int tolua_get_CUnitType_GivesResource(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GivesResource'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->GivesResource);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: GivesResource of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_GivesResource
static int tolua_set_CUnitType_GivesResource(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'GivesResource'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->GivesResource = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TileWidth of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_TileWidth
static int tolua_get_CUnitType_TileWidth(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TileWidth'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TileWidth);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TileWidth of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_TileWidth
static int tolua_set_CUnitType_TileWidth(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TileWidth'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TileWidth = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: TileHeight of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_get_CUnitType_TileHeight
static int tolua_get_CUnitType_TileHeight(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TileHeight'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->TileHeight);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: TileHeight of class  CUnitType */
#ifndef TOLUA_DISABLE_tolua_set_CUnitType_TileHeight
static int tolua_set_CUnitType_TileHeight(lua_State* tolua_S)
{
  CUnitType* self = (CUnitType*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'TileHeight'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->TileHeight = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: UnitTypeByIdent */
#ifndef TOLUA_DISABLE_tolua_stratagus_UnitTypeByIdent00
static int tolua_stratagus_UnitTypeByIdent00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  std::string ident = ((std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   CUnitType& tolua_ret = (CUnitType&)  UnitTypeByIdent(ident);
    tolua_pushusertype(tolua_S,(void*)&tolua_ret,"CUnitType");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'UnitTypeByIdent'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitTypeHumanWall */
#ifndef TOLUA_DISABLE_tolua_get_UnitTypeHumanWall_ptr
static int tolua_get_UnitTypeHumanWall_ptr(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)UnitTypeHumanWall,"CUnitType");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: UnitTypeHumanWall */
#ifndef TOLUA_DISABLE_tolua_set_UnitTypeHumanWall_ptr
static int tolua_set_UnitTypeHumanWall_ptr(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isusertype(tolua_S,2,"CUnitType",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  UnitTypeHumanWall = ((CUnitType*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: UnitTypeOrcWall */
#ifndef TOLUA_DISABLE_tolua_get_UnitTypeOrcWall_ptr
static int tolua_get_UnitTypeOrcWall_ptr(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)UnitTypeOrcWall,"CUnitType");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: UnitTypeOrcWall */
#ifndef TOLUA_DISABLE_tolua_set_UnitTypeOrcWall_ptr
static int tolua_set_UnitTypeOrcWall_ptr(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isusertype(tolua_S,2,"CUnitType",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  UnitTypeOrcWall = ((CUnitType*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetMapStat */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetMapStat00
static int tolua_stratagus_SetMapStat00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,4,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  std::string ident = ((std::string)  tolua_tocppstring(tolua_S,1,0));
  std::string variable_key = ((std::string)  tolua_tocppstring(tolua_S,2,0));
  int value = ((int)  tolua_tonumber(tolua_S,3,0));
  std::string variable_type = ((std::string)  tolua_tocppstring(tolua_S,4,0));
  {
   SetMapStat(ident,variable_key,value,variable_type);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetMapStat'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetMapSound */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetMapSound00
static int tolua_stratagus_SetMapSound00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,3,0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,4,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  std::string ident = ((std::string)  tolua_tocppstring(tolua_S,1,0));
  std::string sound = ((std::string)  tolua_tocppstring(tolua_S,2,0));
  std::string sound_type = ((std::string)  tolua_tocppstring(tolua_S,3,0));
  std::string sound_subtype = ((std::string)  tolua_tocppstring(tolua_S,4,""));
  {
   SetMapSound(ident,sound,sound_type,sound_subtype);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetMapSound'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUpgrade_New00
static int tolua_stratagus_CUpgrade_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CUpgrade",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CUpgrade* tolua_ret = (CUpgrade*)  CUpgrade::New(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUpgrade");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_stratagus_CUpgrade_Get00
static int tolua_stratagus_CUpgrade_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CUpgrade",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string ident = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CUpgrade* tolua_ret = (CUpgrade*)  CUpgrade::Get(ident);
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CUpgrade");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Name of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_get_CUpgrade_Name
static int tolua_get_CUpgrade_Name(lua_State* tolua_S)
{
  CUpgrade* self = (CUpgrade*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
#endif
  tolua_pushcppstring(tolua_S,(const char*)self->Name);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Name of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_set_CUpgrade_Name
static int tolua_set_CUpgrade_Name(lua_State* tolua_S)
{
  CUpgrade* self = (CUpgrade*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Name'",NULL);
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Name = ((std::string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Costs of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_get_stratagus_CUpgrade_Costs
static int tolua_get_stratagus_CUpgrade_Costs(lua_State* tolua_S)
{
 int tolua_index;
  CUpgrade* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CUpgrade*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
 tolua_pushnumber(tolua_S,(lua_Number)self->Costs[tolua_index]);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Costs of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_set_stratagus_CUpgrade_Costs
static int tolua_set_stratagus_CUpgrade_Costs(lua_State* tolua_S)
{
 int tolua_index;
  CUpgrade* self;
 lua_pushstring(tolua_S,".self");
 lua_rawget(tolua_S,1);
 self = (CUpgrade*)  lua_touserdata(tolua_S,-1);
#ifndef TOLUA_RELEASE
 {
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in array indexing.",&tolua_err);
 }
#endif
 tolua_index = (int)tolua_tonumber(tolua_S,2,0);
#ifndef TOLUA_RELEASE
 if (tolua_index<0 || tolua_index>=MaxCosts)
  tolua_error(tolua_S,"array indexing out of range.",NULL);
#endif
  self->Costs[tolua_index] = ((int)  tolua_tonumber(tolua_S,3,0));
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Icon of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_get_CUpgrade_Icon_ptr
static int tolua_get_CUpgrade_Icon_ptr(lua_State* tolua_S)
{
  CUpgrade* self = (CUpgrade*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Icon'",NULL);
#endif
   tolua_pushusertype(tolua_S,(void*)self->Icon,"CIcon");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Icon of class  CUpgrade */
#ifndef TOLUA_DISABLE_tolua_set_CUpgrade_Icon_ptr
static int tolua_set_CUpgrade_Icon_ptr(lua_State* tolua_S)
{
  CUpgrade* self = (CUpgrade*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Icon'",NULL);
  if (!tolua_isusertype(tolua_S,2,"CIcon",0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Icon = ((CIcon*)  tolua_tousertype(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: InitVideo */
#ifndef TOLUA_DISABLE_tolua_stratagus_InitVideo00
static int tolua_stratagus_InitVideo00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   InitVideo();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'InitVideo'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: PlayMovie */
#ifndef TOLUA_DISABLE_tolua_stratagus_PlayMovie00
static int tolua_stratagus_PlayMovie00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string name = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   int tolua_ret = (int)  PlayMovie(name);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'PlayMovie'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ShowFullImage */
#ifndef TOLUA_DISABLE_tolua_stratagus_ShowFullImage00
static int tolua_stratagus_ShowFullImage00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string name = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  unsigned int timeOutInSecond = ((unsigned int)  tolua_tonumber(tolua_S,2,0));
  {
   ShowFullImage(name,timeOutInSecond);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ShowFullImage'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SaveMapPNG */
#ifndef TOLUA_DISABLE_tolua_stratagus_SaveMapPNG00
static int tolua_stratagus_SaveMapPNG00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const char* name = ((const char*)  tolua_tostring(tolua_S,1,0));
  {
   SaveMapPNG(name);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SaveMapPNG'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Width of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_get_CVideo_Width
static int tolua_get_CVideo_Width(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Width'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Width);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Width of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_set_CVideo_Width
static int tolua_set_CVideo_Width(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Width'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Width = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Height of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_get_CVideo_Height
static int tolua_get_CVideo_Height(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Height'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Height);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Height of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_set_CVideo_Height
static int tolua_set_CVideo_Height(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Height'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Height = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Depth of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_get_CVideo_Depth
static int tolua_get_CVideo_Depth(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Depth'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->Depth);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: Depth of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_set_CVideo_Depth
static int tolua_set_CVideo_Depth(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'Depth'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->Depth = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: FullScreen of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_get_CVideo_FullScreen
static int tolua_get_CVideo_FullScreen(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FullScreen'",NULL);
#endif
  tolua_pushboolean(tolua_S,(bool)self->FullScreen);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: FullScreen of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_set_CVideo_FullScreen
static int tolua_set_CVideo_FullScreen(lua_State* tolua_S)
{
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'FullScreen'",NULL);
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->FullScreen = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: ResizeScreen of class  CVideo */
#ifndef TOLUA_DISABLE_tolua_stratagus_CVideo_ResizeScreen00
static int tolua_stratagus_CVideo_ResizeScreen00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CVideo",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CVideo* self = (CVideo*)  tolua_tousertype(tolua_S,1,0);
  int width = ((int)  tolua_tonumber(tolua_S,2,0));
  int height = ((int)  tolua_tonumber(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'ResizeScreen'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->ResizeScreen(width,height);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ResizeScreen'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: Video */
#ifndef TOLUA_DISABLE_tolua_get_Video
static int tolua_get_Video(lua_State* tolua_S)
{
   tolua_pushusertype(tolua_S,(void*)&Video,"CVideo");
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* function: ToggleFullScreen */
#ifndef TOLUA_DISABLE_tolua_stratagus_ToggleFullScreen00
static int tolua_stratagus_ToggleFullScreen00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ToggleFullScreen();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ToggleFullScreen'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Load of class  CGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphicPtr_Load00
static int tolua_stratagus_CGraphicPtr_Load00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CGraphicPtr",0,&tolua_err) ||
     !tolua_isboolean(tolua_S,2,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CGraphicPtr* self = (CGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  bool grayscale = ((bool)  tolua_toboolean(tolua_S,2,false));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Load'", NULL);
#endif
  {
   Load(self,grayscale);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Load'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Resize of class  CGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphicPtr_Resize00
static int tolua_stratagus_CGraphicPtr_Resize00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CGraphicPtr",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CGraphicPtr* self = (CGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  int w = ((int)  tolua_tonumber(tolua_S,2,0));
  int h = ((int)  tolua_tonumber(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Resize'", NULL);
#endif
  {
   Resize(self,w,h);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Resize'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: ResizeKeepRatio of class  CGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphicPtr_ResizeKeepRatio00
static int tolua_stratagus_CGraphicPtr_ResizeKeepRatio00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CGraphicPtr",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CGraphicPtr* self = (CGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  int w = ((int)  tolua_tonumber(tolua_S,2,0));
  int h = ((int)  tolua_tonumber(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'ResizeKeepRatio'", NULL);
#endif
  {
   ResizeKeepRatio(self,w,h);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ResizeKeepRatio'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: SetPaletteColor of class  CGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphicPtr_SetPaletteColor00
static int tolua_stratagus_CGraphicPtr_SetPaletteColor00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CGraphicPtr",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,5,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,6,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CGraphicPtr* self = (CGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  int idx = ((int)  tolua_tonumber(tolua_S,2,0));
  int r = ((int)  tolua_tonumber(tolua_S,3,0));
  int g = ((int)  tolua_tonumber(tolua_S,4,0));
  int b = ((int)  tolua_tonumber(tolua_S,5,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'SetPaletteColor'", NULL);
#endif
  {
   SetPaletteColor(self,idx,r,g,b);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetPaletteColor'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: OverlayGraphic of class  CGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphicPtr_OverlayGraphic00
static int tolua_stratagus_CGraphicPtr_OverlayGraphic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CGraphicPtr",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)) ||
     !tolua_isboolean(tolua_S,3,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CGraphicPtr* self = (CGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  CGraphicPtr g = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0));
  bool mask = ((bool)  tolua_toboolean(tolua_S,3,false));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'OverlayGraphic'", NULL);
#endif
  {
   OverlayGraphic(self,g,mask);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'OverlayGraphic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Load of class  CPlayerColorGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphicPtr_Load00
static int tolua_stratagus_CPlayerColorGraphicPtr_Load00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CPlayerColorGraphicPtr",0,&tolua_err) ||
     !tolua_isboolean(tolua_S,2,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CPlayerColorGraphicPtr* self = (CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  bool grayscale = ((bool)  tolua_toboolean(tolua_S,2,false));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Load'", NULL);
#endif
  {
   Load(self,grayscale);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Load'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Resize of class  CPlayerColorGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphicPtr_Resize00
static int tolua_stratagus_CPlayerColorGraphicPtr_Resize00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CPlayerColorGraphicPtr",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CPlayerColorGraphicPtr* self = (CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  int w = ((int)  tolua_tonumber(tolua_S,2,0));
  int h = ((int)  tolua_tonumber(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Resize'", NULL);
#endif
  {
   Resize(self,w,h);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Resize'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: SetPaletteColor of class  CPlayerColorGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphicPtr_SetPaletteColor00
static int tolua_stratagus_CPlayerColorGraphicPtr_SetPaletteColor00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CPlayerColorGraphicPtr",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,5,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,6,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CPlayerColorGraphicPtr* self = (CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  int idx = ((int)  tolua_tonumber(tolua_S,2,0));
  int r = ((int)  tolua_tonumber(tolua_S,3,0));
  int g = ((int)  tolua_tonumber(tolua_S,4,0));
  int b = ((int)  tolua_tonumber(tolua_S,5,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'SetPaletteColor'", NULL);
#endif
  {
   SetPaletteColor(self,idx,r,g,b);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetPaletteColor'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: OverlayGraphic of class  CPlayerColorGraphicPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphicPtr_OverlayGraphic00
static int tolua_stratagus_CPlayerColorGraphicPtr_OverlayGraphic00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"CPlayerColorGraphicPtr",0,&tolua_err) ||
     (tolua_isvaluenil(tolua_S,2,&tolua_err) || !tolua_isusertype(tolua_S,2,"CGraphicPtr",0,&tolua_err)) ||
     !tolua_isboolean(tolua_S,3,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  CPlayerColorGraphicPtr* self = (CPlayerColorGraphicPtr*)  tolua_tousertype(tolua_S,1,0);
  CGraphicPtr g = *((CGraphicPtr*)  tolua_tousertype(tolua_S,2,0));
  bool mask = ((bool)  tolua_toboolean(tolua_S,3,false));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'OverlayGraphic'", NULL);
#endif
  {
   OverlayGraphic(self,g,mask);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'OverlayGraphic'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  CGraphic */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphic_New00
static int tolua_stratagus_CGraphic_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CGraphic",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string file = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  int w = ((int)  tolua_tonumber(tolua_S,3,0));
  int h = ((int)  tolua_tonumber(tolua_S,4,0));
  {
   CGraphicPtr tolua_ret = (CGraphicPtr)  CGraphic::New(file,w,h);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CGraphicPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CGraphicPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"CGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: ForceNew of class  CGraphic */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphic_ForceNew00
static int tolua_stratagus_CGraphic_ForceNew00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CGraphic",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string file = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  int w = ((int)  tolua_tonumber(tolua_S,3,0));
  int h = ((int)  tolua_tonumber(tolua_S,4,0));
  {
   CGraphicPtr tolua_ret = (CGraphicPtr)  CGraphic::ForceNew(file,w,h);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CGraphicPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CGraphicPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"CGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ForceNew'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CGraphic */
#ifndef TOLUA_DISABLE_tolua_stratagus_CGraphic_Get00
static int tolua_stratagus_CGraphic_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CGraphic",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string file = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CGraphicPtr tolua_ret = (CGraphicPtr)  CGraphic::Get(file);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CGraphicPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CGraphicPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"CGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  CPlayerColorGraphic */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphic_New00
static int tolua_stratagus_CPlayerColorGraphic_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CPlayerColorGraphic",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string file = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  int w = ((int)  tolua_tonumber(tolua_S,3,0));
  int h = ((int)  tolua_tonumber(tolua_S,4,0));
  {
   CPlayerColorGraphicPtr tolua_ret = (CPlayerColorGraphicPtr)  CPlayerColorGraphic::New(file,w,h);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CPlayerColorGraphicPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CPlayerColorGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CPlayerColorGraphicPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"CPlayerColorGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: ForceNew of class  CPlayerColorGraphic */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphic_ForceNew00
static int tolua_stratagus_CPlayerColorGraphic_ForceNew00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CPlayerColorGraphic",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string file = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  int w = ((int)  tolua_tonumber(tolua_S,3,0));
  int h = ((int)  tolua_tonumber(tolua_S,4,0));
  {
   CPlayerColorGraphicPtr tolua_ret = (CPlayerColorGraphicPtr)  CPlayerColorGraphic::ForceNew(file,w,h);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CPlayerColorGraphicPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CPlayerColorGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CPlayerColorGraphicPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"CPlayerColorGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ForceNew'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Get of class  CPlayerColorGraphic */
#ifndef TOLUA_DISABLE_tolua_stratagus_CPlayerColorGraphic_Get00
static int tolua_stratagus_CPlayerColorGraphic_Get00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CPlayerColorGraphic",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string file = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   CPlayerColorGraphicPtr tolua_ret = (CPlayerColorGraphicPtr)  CPlayerColorGraphic::Get(file);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((CPlayerColorGraphicPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"CPlayerColorGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(CPlayerColorGraphicPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"CPlayerColorGraphicPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Get'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new of class  CColor */
#ifndef TOLUA_DISABLE_tolua_stratagus_CColor_new00
static int tolua_stratagus_CColor_new00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CColor",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,5,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,6,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned char r = ((unsigned char)  tolua_tonumber(tolua_S,2,0));
  unsigned char g = ((unsigned char)  tolua_tonumber(tolua_S,3,0));
  unsigned char b = ((unsigned char)  tolua_tonumber(tolua_S,4,0));
  unsigned char a = ((unsigned char)  tolua_tonumber(tolua_S,5,255));
  {
   CColor* tolua_ret = (CColor*)  Mtolua_new((CColor)(r,g,b,a));
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CColor");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new_local of class  CColor */
#ifndef TOLUA_DISABLE_tolua_stratagus_CColor_new00_local
static int tolua_stratagus_CColor_new00_local(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"CColor",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,1,&tolua_err) ||
     !tolua_isnumber(tolua_S,5,1,&tolua_err) ||
     !tolua_isnoobj(tolua_S,6,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned char r = ((unsigned char)  tolua_tonumber(tolua_S,2,0));
  unsigned char g = ((unsigned char)  tolua_tonumber(tolua_S,3,0));
  unsigned char b = ((unsigned char)  tolua_tonumber(tolua_S,4,0));
  unsigned char a = ((unsigned char)  tolua_tonumber(tolua_S,5,255));
  {
   CColor* tolua_ret = (CColor*)  Mtolua_new((CColor)(r,g,b,a));
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"CColor");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: R of class  CColor */
#ifndef TOLUA_DISABLE_tolua_get_CColor_unsigned_R
static int tolua_get_CColor_unsigned_R(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'R'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->R);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: R of class  CColor */
#ifndef TOLUA_DISABLE_tolua_set_CColor_unsigned_R
static int tolua_set_CColor_unsigned_R(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'R'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->R = ((unsigned char)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: G of class  CColor */
#ifndef TOLUA_DISABLE_tolua_get_CColor_unsigned_G
static int tolua_get_CColor_unsigned_G(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->G);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: G of class  CColor */
#ifndef TOLUA_DISABLE_tolua_set_CColor_unsigned_G
static int tolua_set_CColor_unsigned_G(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'G'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->G = ((unsigned char)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: B of class  CColor */
#ifndef TOLUA_DISABLE_tolua_get_CColor_unsigned_B
static int tolua_get_CColor_unsigned_B(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'B'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->B);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: B of class  CColor */
#ifndef TOLUA_DISABLE_tolua_set_CColor_unsigned_B
static int tolua_set_CColor_unsigned_B(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'B'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->B = ((unsigned char)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: A of class  CColor */
#ifndef TOLUA_DISABLE_tolua_get_CColor_unsigned_A
static int tolua_get_CColor_unsigned_A(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'A'",NULL);
#endif
  tolua_pushnumber(tolua_S,(lua_Number)self->A);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: A of class  CColor */
#ifndef TOLUA_DISABLE_tolua_set_CColor_unsigned_A
static int tolua_set_CColor_unsigned_A(lua_State* tolua_S)
{
  CColor* self = (CColor*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!self) tolua_error(tolua_S,"invalid 'self' in accessing variable 'A'",NULL);
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  self->A = ((unsigned char)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetColorCycleAll */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetColorCycleAll00
static int tolua_stratagus_SetColorCycleAll00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isboolean(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  bool value = ((bool)  tolua_toboolean(tolua_S,1,0));
  {
   SetColorCycleAll(value);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetColorCycleAll'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: ClearAllColorCyclingRange */
#ifndef TOLUA_DISABLE_tolua_stratagus_ClearAllColorCyclingRange00
static int tolua_stratagus_ClearAllColorCyclingRange00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   ClearAllColorCyclingRange();
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'ClearAllColorCyclingRange'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: AddColorCyclingRange */
#ifndef TOLUA_DISABLE_tolua_stratagus_AddColorCyclingRange00
static int tolua_stratagus_AddColorCyclingRange00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned int startColorIndex = ((unsigned int)  tolua_tonumber(tolua_S,1,0));
  unsigned int endColorIndex = ((unsigned int)  tolua_tonumber(tolua_S,2,0));
  {
   AddColorCyclingRange(startColorIndex,endColorIndex);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'AddColorCyclingRange'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SetColorCycleSpeed */
#ifndef TOLUA_DISABLE_tolua_stratagus_SetColorCycleSpeed00
static int tolua_stratagus_SetColorCycleSpeed00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  unsigned int speed = ((unsigned int)  tolua_tonumber(tolua_S,1,0));
  {
   unsigned int tolua_ret = (unsigned int)  SetColorCycleSpeed(speed);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SetColorCycleSpeed'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Load of class  MngPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_MngPtr_Load00
static int tolua_stratagus_MngPtr_Load00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"MngPtr",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  MngPtr* self = (MngPtr*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Load'", NULL);
#endif
  {
   Load(self);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Load'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Draw of class  MngPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_MngPtr_Draw00
static int tolua_stratagus_MngPtr_Draw00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"MngPtr",0,&tolua_err) ||
     !tolua_isnumber(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,4,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  MngPtr* self = (MngPtr*)  tolua_tousertype(tolua_S,1,0);
  int w = ((int)  tolua_tonumber(tolua_S,2,0));
  int h = ((int)  tolua_tonumber(tolua_S,3,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Draw'", NULL);
#endif
  {
   Draw(self,w,h);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Draw'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Reset of class  MngPtr */
#ifndef TOLUA_DISABLE_tolua_stratagus_MngPtr_Reset00
static int tolua_stratagus_MngPtr_Reset00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"MngPtr",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  MngPtr* self = (MngPtr*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Reset'", NULL);
#endif
  {
   Reset(self);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Reset'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: New of class  Mng */
#ifndef TOLUA_DISABLE_tolua_stratagus_Mng_New00
static int tolua_stratagus_Mng_New00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"Mng",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,3,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string name = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  {
   MngPtr tolua_ret = (MngPtr)  Mng::New(name);
   {
#ifdef __cplusplus
    void* tolua_obj = Mtolua_new((MngPtr)(tolua_ret));
     tolua_pushusertype(tolua_S,tolua_obj,"MngPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#else
    void* tolua_obj = tolua_copy(tolua_S,(void*)&tolua_ret,sizeof(MngPtr));
     tolua_pushusertype(tolua_S,tolua_obj,"MngPtr");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
#endif
   }
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'New'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: MaxFPS of class  Mng */
#ifndef TOLUA_DISABLE_tolua_get_Mng_MaxFPS
static int tolua_get_Mng_MaxFPS(lua_State* tolua_S)
{
  tolua_pushnumber(tolua_S,(lua_Number)Mng::MaxFPS);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: MaxFPS of class  Mng */
#ifndef TOLUA_DISABLE_tolua_set_Mng_MaxFPS
static int tolua_set_Mng_MaxFPS(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isnumber(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  Mng::MaxFPS = ((int)  tolua_tonumber(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* method: new of class  Movie */
#ifndef TOLUA_DISABLE_tolua_stratagus_Movie_new00
static int tolua_stratagus_Movie_new00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"Movie",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   Movie* tolua_ret = (Movie*)  Mtolua_new((Movie)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"Movie");
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: new_local of class  Movie */
#ifndef TOLUA_DISABLE_tolua_stratagus_Movie_new00_local
static int tolua_stratagus_Movie_new00_local(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertable(tolua_S,1,"Movie",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   Movie* tolua_ret = (Movie*)  Mtolua_new((Movie)());
    tolua_pushusertype(tolua_S,(void*)tolua_ret,"Movie");
    tolua_register_gc(tolua_S,lua_gettop(tolua_S));
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'new'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: Load of class  Movie */
#ifndef TOLUA_DISABLE_tolua_stratagus_Movie_Load00
static int tolua_stratagus_Movie_Load00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"Movie",0,&tolua_err) ||
     !tolua_iscppstring(tolua_S,2,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,3,0,&tolua_err) ||
     !tolua_isnumber(tolua_S,4,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,5,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  Movie* self = (Movie*)  tolua_tousertype(tolua_S,1,0);
  const std::string name = ((const std::string)  tolua_tocppstring(tolua_S,2,0));
  int w = ((int)  tolua_tonumber(tolua_S,3,0));
  int h = ((int)  tolua_tonumber(tolua_S,4,0));
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'Load'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->Load(name,w,h);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Load'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* method: IsPlaying of class  Movie */
#ifndef TOLUA_DISABLE_tolua_stratagus_Movie_IsPlaying00
static int tolua_stratagus_Movie_IsPlaying00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isusertype(tolua_S,1,"Movie",0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  Movie* self = (Movie*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
  if (!self) tolua_error(tolua_S,"invalid 'self' in function 'IsPlaying'", NULL);
#endif
  {
   bool tolua_ret = (bool)  self->IsPlaying();
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'IsPlaying'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SaveGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_SaveGame00
static int tolua_stratagus_SaveGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string filename = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   int tolua_ret = (int)  SaveGame(filename);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SaveGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: DeleteSaveGame */
#ifndef TOLUA_DISABLE_tolua_stratagus_DeleteSaveGame00
static int tolua_stratagus_DeleteSaveGame00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string filename = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   DeleteSaveGame(filename);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'DeleteSaveGame'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: Translate */
#ifndef TOLUA_DISABLE_tolua_stratagus__00
static int tolua_stratagus__00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_iscppstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const std::string str = ((const std::string)  tolua_tocppstring(tolua_S,1,0));
  {
   std::string tolua_ret = (std::string)  Translate(str);
   tolua_pushcppstring(tolua_S,(const char*)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function '_'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SyncRand */
#ifndef TOLUA_DISABLE_tolua_stratagus_SyncRand00
static int tolua_stratagus_SyncRand00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnoobj(tolua_S,1,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  {
   int tolua_ret = (int)  SyncRand();
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'SyncRand'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: SyncRand */
#ifndef TOLUA_DISABLE_tolua_stratagus_SyncRand01
static int tolua_stratagus_SyncRand01(lua_State* tolua_S)
{
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
 {
  int max = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   int tolua_ret = (int)  SyncRand(max);
   tolua_pushnumber(tolua_S,(lua_Number)tolua_ret);
  }
 }
 return 1;
tolua_lerror:
 return tolua_stratagus_SyncRand00(tolua_S);
}
#endif //#ifndef TOLUA_DISABLE

/* function: CanAccessFile */
#ifndef TOLUA_DISABLE_tolua_stratagus_CanAccessFile00
static int tolua_stratagus_CanAccessFile00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isstring(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  const char* filename = ((const char*)  tolua_tostring(tolua_S,1,0));
  {
   bool tolua_ret = (bool)  CanAccessFile(filename);
   tolua_pushboolean(tolua_S,(bool)tolua_ret);
  }
 }
 return 1;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'CanAccessFile'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* function: Exit */
#ifndef TOLUA_DISABLE_tolua_stratagus_Exit00
static int tolua_stratagus_Exit00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
 tolua_Error tolua_err;
 if (
     !tolua_isnumber(tolua_S,1,0,&tolua_err) ||
     !tolua_isnoobj(tolua_S,2,&tolua_err)
 )
  goto tolua_lerror;
 else
#endif
 {
  int err = ((int)  tolua_tonumber(tolua_S,1,0));
  {
   Exit(err);
  }
 }
 return 0;
#ifndef TOLUA_RELEASE
 tolua_lerror:
 tolua_error(tolua_S,"#ferror in function 'Exit'.",&tolua_err);
 return 0;
#endif
}
#endif //#ifndef TOLUA_DISABLE

/* get function: CliMapName */
#ifndef TOLUA_DISABLE_tolua_get_CliMapName
static int tolua_get_CliMapName(lua_State* tolua_S)
{
  tolua_pushcppstring(tolua_S,(const char*)CliMapName);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: CliMapName */
#ifndef TOLUA_DISABLE_tolua_set_CliMapName
static int tolua_set_CliMapName(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  CliMapName = ((std::string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: StratagusLibPath */
#ifndef TOLUA_DISABLE_tolua_get_StratagusLibPath
static int tolua_get_StratagusLibPath(lua_State* tolua_S)
{
  tolua_pushcppstring(tolua_S,(const char*)StratagusLibPath);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: StratagusLibPath */
#ifndef TOLUA_DISABLE_tolua_set_StratagusLibPath
static int tolua_set_StratagusLibPath(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_iscppstring(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  StratagusLibPath = ((std::string)  tolua_tocppstring(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* get function: IsDebugEnabled */
#ifndef TOLUA_DISABLE_tolua_get_IsDebugEnabled
static int tolua_get_IsDebugEnabled(lua_State* tolua_S)
{
  tolua_pushboolean(tolua_S,(bool)IsDebugEnabled);
 return 1;
}
#endif //#ifndef TOLUA_DISABLE

/* set function: IsDebugEnabled */
#ifndef TOLUA_DISABLE_tolua_set_IsDebugEnabled
static int tolua_set_IsDebugEnabled(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
  tolua_Error tolua_err;
  if (!tolua_isboolean(tolua_S,2,0,&tolua_err))
   tolua_error(tolua_S,"#vinvalid type in variable assignment.",&tolua_err);
#endif
  IsDebugEnabled = ((bool)  tolua_toboolean(tolua_S,2,0))
;
 return 0;
}
#endif //#ifndef TOLUA_DISABLE

/* Open function */
TOLUA_API int tolua_stratagus_open (lua_State* tolua_S)
{
 tolua_open(tolua_S);
 tolua_reg_types(tolua_S);
 tolua_module(tolua_S,NULL,1);
 tolua_beginmodule(tolua_S,NULL);

  { /* begin embedded lua code */
   int top = lua_gettop(tolua_S);
   static const unsigned char B[] = {
    10,109,116, 32, 61, 32,123, 32, 95, 95,105,110,100,101,120,
     32, 61, 32,102,117,110, 99,116,105,111,110, 40,116, 44, 32,
    107,101,121, 41, 32,114,101,116,117,114,110, 32, 67, 73, 99,
    111,110, 58, 71,101,116, 40,107,101,121, 41, 32,101,110,100,
     32,125, 10, 73, 99,111,110,115, 32, 61, 32,123,125, 10,115,
    101,116,109,101,116, 97,116, 97, 98,108,101, 40, 73, 99,111,
    110,115, 44, 32,109,116, 41, 10,109,116, 32, 61, 32,123, 32,
     95, 95,105,110,100,101,120, 32, 61, 32,102,117,110, 99,116,
    105,111,110, 40,116, 44, 32,107,101,121, 41, 32,114,101,116,
    117,114,110, 32, 67, 85,112,103,114, 97,100,101, 58, 71,101,
    116, 40,107,101,121, 41, 32,101,110,100, 32,125, 10, 85,112,
    103,114, 97,100,101,115, 32, 61, 32,123,125, 10,115,101,116,
    109,101,116, 97,116, 97, 98,108,101, 40, 85,112,103,114, 97,
    100,101,115, 44, 32,109,116, 41, 10,109,116, 32, 61, 32,123,
     32, 95, 95,105,110,100,101,120, 32, 61, 32,102,117,110, 99,
    116,105,111,110, 40,116, 44, 32,107,101,121, 41, 32,114,101,
    116,117,114,110, 32, 67, 70,111,110,116, 58, 71,101,116, 40,
    107,101,121, 41, 32,101,110,100, 32,125, 10, 70,111,110,116,
    115, 32, 61, 32,123,125, 10,115,101,116,109,101,116, 97,116,
     97, 98,108,101, 40, 70,111,110,116,115, 44, 32,109,116, 41,
     10,109,116, 32, 61, 32,123, 32, 95, 95,105,110,100,101,120,
     32, 61, 32,102,117,110, 99,116,105,111,110, 40,116, 44, 32,
    107,101,121, 41, 32,114,101,116,117,114,110, 32, 67, 70,111,
    110,116, 67,111,108,111,114, 58, 71,101,116, 40,107,101,121,
     41, 32,101,110,100, 32,125, 10, 70,111,110,116, 67,111,108,
    111,114,115, 32, 61, 32,123,125, 10,115,101,116,109,101,116,
     97,116, 97, 98,108,101, 40, 70,111,110,116, 67,111,108,111,
    114,115, 44, 32,109,116, 41, 10,109,116, 32, 61, 32,123, 32,
     95, 95,105,110,100,101,120, 32, 61, 32,102,117,110, 99,116,
    105,111,110, 40,116, 44, 32,107,101,121, 41, 32,114,101,116,
    117,114,110, 32, 85,110,105,116, 84,121,112,101, 66,121, 73,
    100,101,110,116, 40,107,101,121, 41, 32,101,110,100, 32,125,
     10, 85,110,105,116, 84,121,112,101,115, 32, 61, 32,123,125,
     10,115,101,116,109,101,116, 97,116, 97, 98,108,101, 40, 85,
    110,105,116, 84,121,112,101,115, 44, 32,109,116, 41, 10,102,
    117,110, 99,116,105,111,110, 32, 71, 97,109,101, 83,116, 97,
    114,116,105,110,103, 40, 41, 10,101,110,100, 45, 45, 45, 45,
     45, 45, 45, 45, 45,32
   };
   tolua_dobuffer(tolua_S,(char*)B,sizeof(B),"tolua: embedded Lua code 1");
   lua_settop(tolua_S, top);
  } /* end of embedded lua code */

  tolua_constant(tolua_S,"MaxCosts",MaxCosts);
  tolua_constant(tolua_S,"FoodCost",FoodCost);
  tolua_constant(tolua_S,"ScoreCost",ScoreCost);
  tolua_constant(tolua_S,"ManaResCost",ManaResCost);
  tolua_constant(tolua_S,"FreeWorkersCount",FreeWorkersCount);
  tolua_constant(tolua_S,"MaxResourceInfo",MaxResourceInfo);
  tolua_constant(tolua_S,"PlayerMax",PlayerMax);
  tolua_constant(tolua_S,"PlayerNumNeutral",PlayerNumNeutral);
  tolua_constant(tolua_S,"InfiniteRepairRange",InfiniteRepairRange);
  tolua_constant(tolua_S,"NoButton",NoButton);
  tolua_constant(tolua_S,"LeftButton",LeftButton);
  tolua_constant(tolua_S,"MiddleButton",MiddleButton);
  tolua_constant(tolua_S,"RightButton",RightButton);
  tolua_constant(tolua_S,"UpButton",UpButton);
  tolua_constant(tolua_S,"DownButton",DownButton);
  tolua_function(tolua_S,"AiAttackWithForceAt",tolua_stratagus_AiAttackWithForceAt00);
  tolua_function(tolua_S,"AiAttackWithForce",tolua_stratagus_AiAttackWithForce00);
  tolua_cclass(tolua_S,"CFont","CFont","",NULL);
  tolua_beginmodule(tolua_S,"CFont");
   tolua_function(tolua_S,"New",tolua_stratagus_CFont_New00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CFont_Get00);
   tolua_function(tolua_S,"Height",tolua_stratagus_CFont_Height00);
   tolua_function(tolua_S,"Width",tolua_stratagus_CFont_Width00);
  tolua_endmodule(tolua_S);
  tolua_constant(tolua_S,"MaxFontColors",MaxFontColors);
  tolua_cclass(tolua_S,"CFontColor","CFontColor","",NULL);
  tolua_beginmodule(tolua_S,"CFontColor");
   tolua_function(tolua_S,"New",tolua_stratagus_CFontColor_New00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CFontColor_Get00);
   tolua_array(tolua_S,"Colors",tolua_get_stratagus_CFontColor_Colors,tolua_set_stratagus_CFontColor_Colors);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"IsReplayGame",tolua_stratagus_IsReplayGame00);
  tolua_function(tolua_S,"StartMap",tolua_stratagus_StartMap00);
  tolua_function(tolua_S,"StartReplay",tolua_stratagus_StartReplay00);
  tolua_function(tolua_S,"StartSavedGame",tolua_stratagus_StartSavedGame00);
  tolua_function(tolua_S,"SaveReplay",tolua_stratagus_SaveReplay00);
  tolua_constant(tolua_S,"GameNoResult",GameNoResult);
  tolua_constant(tolua_S,"GameVictory",GameVictory);
  tolua_constant(tolua_S,"GameDefeat",GameDefeat);
  tolua_constant(tolua_S,"GameDraw",GameDraw);
  tolua_constant(tolua_S,"GameQuitToMenu",GameQuitToMenu);
  tolua_constant(tolua_S,"GameRestart",GameRestart);
  tolua_variable(tolua_S,"GameResult",tolua_get_GameResult,tolua_set_GameResult);
  tolua_function(tolua_S,"StopGame",tolua_stratagus_StopGame00);
  tolua_variable(tolua_S,"GameRunning",tolua_get_GameRunning,tolua_set_GameRunning);
  tolua_function(tolua_S,"SetGamePaused",tolua_stratagus_SetGamePaused00);
  tolua_function(tolua_S,"GetGamePaused",tolua_stratagus_GetGamePaused00);
  tolua_variable(tolua_S,"GamePaused",tolua_get_GamePaused,tolua_set_GamePaused);
  tolua_function(tolua_S,"SetGameSpeed",tolua_stratagus_SetGameSpeed00);
  tolua_function(tolua_S,"GetGameSpeed",tolua_stratagus_GetGameSpeed00);
  tolua_variable(tolua_S,"GameSpeed",tolua_get_GameSpeed,tolua_set_GameSpeed);
  tolua_variable(tolua_S,"GameObserve",tolua_get_GameObserve,tolua_set_GameObserve);
  tolua_variable(tolua_S,"GameEstablishing",tolua_get_GameEstablishing,tolua_set_GameEstablishing);
  tolua_variable(tolua_S,"GameCycle",tolua_get_unsigned_GameCycle,tolua_set_unsigned_GameCycle);
  tolua_variable(tolua_S,"FastForwardCycle",tolua_get_unsigned_FastForwardCycle,tolua_set_unsigned_FastForwardCycle);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"SettingsPresets","SettingsPresets","",tolua_collect_SettingsPresets);
  #else
  tolua_cclass(tolua_S,"SettingsPresets","SettingsPresets","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"SettingsPresets");
   tolua_variable(tolua_S,"PlayerColor",tolua_get_SettingsPresets_PlayerColor,tolua_set_SettingsPresets_PlayerColor);
   tolua_variable(tolua_S,"AIScript",tolua_get_SettingsPresets_AIScript,tolua_set_SettingsPresets_AIScript);
   tolua_variable(tolua_S,"Race",tolua_get_SettingsPresets_Race,tolua_set_SettingsPresets_Race);
   tolua_variable(tolua_S,"Team",tolua_get_SettingsPresets_Team,tolua_set_SettingsPresets_Team);
   tolua_variable(tolua_S,"Type",tolua_get_SettingsPresets_Type,tolua_set_SettingsPresets_Type);
  tolua_endmodule(tolua_S);
  tolua_constant(tolua_S,"cNoRevelation",cNoRevelation);
  tolua_constant(tolua_S,"cAllUnits",cAllUnits);
  tolua_constant(tolua_S,"cBuildingsOnly",cBuildingsOnly);
  tolua_constant(tolua_S,"SettingsSinglePlayerGame",SettingsSinglePlayerGame);
  tolua_constant(tolua_S,"SettingsMultiPlayerGame",SettingsMultiPlayerGame);
  tolua_constant(tolua_S,"Unset",Unset);
  tolua_cclass(tolua_S,"Settings","Settings","",NULL);
  tolua_beginmodule(tolua_S,"Settings");
   tolua_variable(tolua_S,"NetGameType",tolua_get_Settings_NetGameType,tolua_set_Settings_NetGameType);
   tolua_array(tolua_S,"Presets",tolua_get_stratagus_Settings_Presets,tolua_set_stratagus_Settings_Presets);
   tolua_variable(tolua_S,"Resources",tolua_get_Settings_Resources,tolua_set_Settings_Resources);
   tolua_variable(tolua_S,"NumUnits",tolua_get_Settings_NumUnits,tolua_set_Settings_NumUnits);
   tolua_variable(tolua_S,"Opponents",tolua_get_Settings_Opponents,tolua_set_Settings_Opponents);
   tolua_variable(tolua_S,"Difficulty",tolua_get_Settings_Difficulty,tolua_set_Settings_Difficulty);
   tolua_variable(tolua_S,"GameType",tolua_get_Settings_GameType,tolua_set_Settings_GameType);
   tolua_variable(tolua_S,"FoV",tolua_get_Settings_FoV,tolua_set_Settings_FoV);
   tolua_variable(tolua_S,"RevealMap",tolua_get_Settings_RevealMap,tolua_set_Settings_RevealMap);
   tolua_variable(tolua_S,"DefeatReveal",tolua_get_Settings_DefeatReveal,tolua_set_Settings_DefeatReveal);
   tolua_variable(tolua_S,"NoFogOfWar",tolua_get_Settings_NoFogOfWar,tolua_set_Settings_NoFogOfWar);
   tolua_variable(tolua_S,"Inside",tolua_get_Settings_Inside,tolua_set_Settings_Inside);
   tolua_variable(tolua_S,"AiExplores",tolua_get_Settings_AiExplores,tolua_set_Settings_AiExplores);
   tolua_variable(tolua_S,"SimplifiedAutoTargeting",tolua_get_Settings_SimplifiedAutoTargeting,tolua_set_Settings_SimplifiedAutoTargeting);
   tolua_function(tolua_S,"GetUserGameSetting",tolua_stratagus_Settings_GetUserGameSetting00);
   tolua_function(tolua_S,"SetUserGameSetting",tolua_stratagus_Settings_SetUserGameSetting00);
  tolua_endmodule(tolua_S);

  { /* begin embedded lua code */
   int top = lua_gettop(tolua_S);
   static const unsigned char B[] = {
    10, 83,101,116,116,105,110,103,115, 46, 77, 97,112, 82,105,
     99,104,110,101,115,115, 32, 61, 32, 48, 45, 45, 45, 45, 45,
     45, 45,32
   };
   tolua_dobuffer(tolua_S,(char*)B,sizeof(B),"tolua: embedded Lua code 2");
   lua_settop(tolua_S, top);
  } /* end of embedded lua code */

  tolua_variable(tolua_S,"GameSettings",tolua_get_GameSettings,tolua_set_GameSettings);
  tolua_constant(tolua_S,"SettingsPresetMapDefault",SettingsPresetMapDefault);
  tolua_constant(tolua_S,"cHidden",cHidden);
  tolua_constant(tolua_S,"cKnown",cKnown);
  tolua_constant(tolua_S,"cExplored",cExplored);
  tolua_constant(tolua_S,"cNumOfModes",cNumOfModes);
  tolua_constant(tolua_S,"cShadowCasting",cShadowCasting);
  tolua_constant(tolua_S,"cSimpleRadial",cSimpleRadial);
  tolua_constant(tolua_S,"NumOfTypes",NumOfTypes);
  tolua_constant(tolua_S,"SettingsGameTypeMapDefault",SettingsGameTypeMapDefault);
  tolua_constant(tolua_S,"SettingsGameTypeMelee",SettingsGameTypeMelee);
  tolua_constant(tolua_S,"SettingsGameTypeFreeForAll",SettingsGameTypeFreeForAll);
  tolua_constant(tolua_S,"SettingsGameTypeTopVsBottom",SettingsGameTypeTopVsBottom);
  tolua_constant(tolua_S,"SettingsGameTypeLeftVsRight",SettingsGameTypeLeftVsRight);
  tolua_constant(tolua_S,"SettingsGameTypeManVsMachine",SettingsGameTypeManVsMachine);
  tolua_constant(tolua_S,"SettingsGameTypeManTeamVsMachine",SettingsGameTypeManTeamVsMachine);
  tolua_constant(tolua_S,"SettingsGameTypeMachineVsMachine",SettingsGameTypeMachineVsMachine);
  tolua_constant(tolua_S,"SettingsGameTypeMachineVsMachineTraining",SettingsGameTypeMachineVsMachineTraining);
  tolua_cclass(tolua_S,"CMapInfo","CMapInfo","",NULL);
  tolua_beginmodule(tolua_S,"CMapInfo");
   tolua_variable(tolua_S,"Description",tolua_get_CMapInfo_Description,tolua_set_CMapInfo_Description);
   tolua_variable(tolua_S,"Filename",tolua_get_CMapInfo_Filename,tolua_set_CMapInfo_Filename);
   tolua_variable(tolua_S,"Preamble",tolua_get_CMapInfo_Preamble,tolua_set_CMapInfo_Preamble);
   tolua_variable(tolua_S,"Postamble",tolua_get_CMapInfo_Postamble,tolua_set_CMapInfo_Postamble);
   tolua_variable(tolua_S,"MapWidth",tolua_get_CMapInfo_MapWidth,tolua_set_CMapInfo_MapWidth);
   tolua_variable(tolua_S,"MapHeight",tolua_get_CMapInfo_MapHeight,tolua_set_CMapInfo_MapHeight);
   tolua_array(tolua_S,"PlayerType",tolua_get_stratagus_CMapInfo_PlayerType,tolua_set_stratagus_CMapInfo_PlayerType);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CTileset","CTileset","",NULL);
  tolua_beginmodule(tolua_S,"CTileset");
   tolua_variable(tolua_S,"Name",tolua_get_CTileset_Name,tolua_set_CTileset_Name);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CMap","CMap","",NULL);
  tolua_beginmodule(tolua_S,"CMap");
   tolua_variable(tolua_S,"Info",tolua_get_CMap_Info,NULL);
   tolua_variable(tolua_S,"Tileset",tolua_get_CMap_Tileset,NULL);
  tolua_endmodule(tolua_S);
  tolua_variable(tolua_S,"Map",tolua_get_Map,tolua_set_Map);
  tolua_function(tolua_S,"SetTile",tolua_stratagus_SetTile00);
  tolua_cclass(tolua_S,"CMinimap","CMinimap","",NULL);
  tolua_beginmodule(tolua_S,"CMinimap");
   tolua_variable(tolua_S,"X",tolua_get_CMinimap_X,tolua_set_CMinimap_X);
   tolua_variable(tolua_S,"Y",tolua_get_CMinimap_Y,tolua_set_CMinimap_Y);
   tolua_variable(tolua_S,"W",tolua_get_CMinimap_W,tolua_set_CMinimap_W);
   tolua_variable(tolua_S,"H",tolua_get_CMinimap_H,tolua_set_CMinimap_H);
   tolua_variable(tolua_S,"WithTerrain",tolua_get_CMinimap_WithTerrain,tolua_set_CMinimap_WithTerrain);
   tolua_variable(tolua_S,"ShowSelected",tolua_get_CMinimap_ShowSelected,tolua_set_CMinimap_ShowSelected);
   tolua_variable(tolua_S,"Transparent",tolua_get_CMinimap_Transparent,tolua_set_CMinimap_Transparent);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"InitNetwork1",tolua_stratagus_InitNetwork100);
  tolua_function(tolua_S,"ExitNetwork1",tolua_stratagus_ExitNetwork100);
  tolua_function(tolua_S,"IsNetworkGame",tolua_stratagus_IsNetworkGame00);
  tolua_function(tolua_S,"NetworkSetupServerAddress",tolua_stratagus_NetworkSetupServerAddress00);
  tolua_function(tolua_S,"NetworkInitClientConnect",tolua_stratagus_NetworkInitClientConnect00);
  tolua_function(tolua_S,"NetworkInitServerConnect",tolua_stratagus_NetworkInitServerConnect00);
  tolua_function(tolua_S,"NetworkServerStartGame",tolua_stratagus_NetworkServerStartGame00);
  tolua_function(tolua_S,"NetworkProcessClientRequest",tolua_stratagus_NetworkProcessClientRequest00);
  tolua_function(tolua_S,"GetNetworkState",tolua_stratagus_GetNetworkState00);
  tolua_function(tolua_S,"NetworkServerResyncClients",tolua_stratagus_NetworkServerResyncClients00);
  tolua_function(tolua_S,"NetworkDetachFromServer",tolua_stratagus_NetworkDetachFromServer00);
  tolua_cclass(tolua_S,"ServerSetupStateRacesArray","ServerSetupStateRacesArray","",NULL);
  tolua_beginmodule(tolua_S,"ServerSetupStateRacesArray");
   tolua_function(tolua_S,".seti",tolua_stratagus_ServerSetupStateRacesArray__seti00);
   tolua_function(tolua_S,".geti",tolua_stratagus_ServerSetupStateRacesArray__geti00);
   tolua_function(tolua_S,".geti",tolua_stratagus_ServerSetupStateRacesArray__geti01);
  tolua_endmodule(tolua_S);
  tolua_constant(tolua_S,"SlotAvailable",Available);
  tolua_constant(tolua_S,"SlotComputer",Computer);
  tolua_constant(tolua_S,"SlotClosed",Closed);
  tolua_cclass(tolua_S,"CServerSetup","CServerSetup","",NULL);
  tolua_beginmodule(tolua_S,"CServerSetup");
   tolua_variable(tolua_S,"ServerGameSettings",tolua_get_CServerSetup_ServerGameSettings,tolua_set_CServerSetup_ServerGameSettings);
   tolua_array(tolua_S,"CompOpt",tolua_get_stratagus_CServerSetup_CompOpt,tolua_set_stratagus_CServerSetup_CompOpt);
   tolua_array(tolua_S,"Ready",tolua_get_stratagus_CServerSetup_Ready,tolua_set_stratagus_CServerSetup_Ready);
   tolua_variable(tolua_S,"ResourcesOption",tolua_get_CServerSetup_unsigned_ResourcesOption,tolua_set_CServerSetup_unsigned_ResourcesOption);
   tolua_variable(tolua_S,"UnitsOption",tolua_get_CServerSetup_unsigned_UnitsOption,tolua_set_CServerSetup_unsigned_UnitsOption);
   tolua_variable(tolua_S,"FogOfWar",tolua_get_CServerSetup_unsigned_FogOfWar,tolua_set_CServerSetup_unsigned_FogOfWar);
   tolua_variable(tolua_S,"Inside",tolua_get_CServerSetup_unsigned_Inside,tolua_set_CServerSetup_unsigned_Inside);
   tolua_variable(tolua_S,"RevealMap",tolua_get_CServerSetup_unsigned_RevealMap,tolua_set_CServerSetup_unsigned_RevealMap);
   tolua_variable(tolua_S,"GameTypeOption",tolua_get_CServerSetup_unsigned_GameTypeOption,tolua_set_CServerSetup_unsigned_GameTypeOption);
   tolua_variable(tolua_S,"Difficulty",tolua_get_CServerSetup_unsigned_Difficulty,tolua_set_CServerSetup_unsigned_Difficulty);
   tolua_variable(tolua_S,"Opponents",tolua_get_CServerSetup_unsigned_Opponents,tolua_set_CServerSetup_unsigned_Opponents);
   tolua_variable(tolua_S,"Race",tolua_get_CServerSetup_Race_ptr,NULL);
  tolua_endmodule(tolua_S);

  { /* begin embedded lua code */
   int top = lua_gettop(tolua_S);
   static const unsigned char B[] = {
    10, 67, 83,101,114,118,101,114, 83,101,116,117,112, 46, 77,
     97,112, 82,105, 99,104,110,101,115,115, 32, 61, 32, 48, 45,
     45, 45,32
   };
   tolua_dobuffer(tolua_S,(char*)B,sizeof(B),"tolua: embedded Lua code 3");
   lua_settop(tolua_S, top);
  } /* end of embedded lua code */

  tolua_variable(tolua_S,"LocalSetupState",tolua_get_LocalSetupState,tolua_set_LocalSetupState);
  tolua_variable(tolua_S,"ServerSetupState",tolua_get_ServerSetupState,tolua_set_ServerSetupState);
  tolua_variable(tolua_S,"NetLocalHostsSlot",tolua_get_NetLocalHostsSlot,tolua_set_NetLocalHostsSlot);
  tolua_variable(tolua_S,"NetPlayerNameSize",tolua_get_NetPlayerNameSize,NULL);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CNetworkHost","CNetworkHost","",tolua_collect_CNetworkHost);
  #else
  tolua_cclass(tolua_S,"CNetworkHost","CNetworkHost","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CNetworkHost");
   tolua_variable(tolua_S,"Host",tolua_get_CNetworkHost_unsigned_Host,tolua_set_CNetworkHost_unsigned_Host);
   tolua_variable(tolua_S,"Port",tolua_get_CNetworkHost_unsigned_Port,tolua_set_CNetworkHost_unsigned_Port);
   tolua_variable(tolua_S,"PlyNr",tolua_get_CNetworkHost_unsigned_PlyNr,tolua_set_CNetworkHost_unsigned_PlyNr);
   tolua_variable(tolua_S,"PlyName",tolua_get_CNetworkHost_PlyName,tolua_set_CNetworkHost_PlyName);
  tolua_endmodule(tolua_S);
  tolua_array(tolua_S,"Hosts",tolua_get_stratagus_Hosts,tolua_set_stratagus_Hosts);
  tolua_variable(tolua_S,"NetworkMapName",tolua_get_NetworkMapName,tolua_set_NetworkMapName);
  tolua_variable(tolua_S,"NetworkMapFragmentName",tolua_get_NetworkMapFragmentName,tolua_set_NetworkMapFragmentName);
  tolua_function(tolua_S,"NetworkGamePrepareGameSettings",tolua_stratagus_NetworkGamePrepareGameSettings00);
  tolua_variable(tolua_S,"AStarFixedUnitCrossingCost",tolua_get_AStarFixedUnitCrossingCost,tolua_set_AStarFixedUnitCrossingCost);
  tolua_variable(tolua_S,"AStarMovingUnitCrossingCost",tolua_get_AStarMovingUnitCrossingCost,tolua_set_AStarMovingUnitCrossingCost);
  tolua_variable(tolua_S,"AStarKnowUnseenTerrain",tolua_get_AStarKnowUnseenTerrain,tolua_set_AStarKnowUnseenTerrain);
  tolua_variable(tolua_S,"AStarUnknownTerrainCost",tolua_get_AStarUnknownTerrainCost,tolua_set_AStarUnknownTerrainCost);
  tolua_constant(tolua_S,"PlayerNeutral",PlayerNeutral);
  tolua_constant(tolua_S,"PlayerNobody",PlayerNobody);
  tolua_constant(tolua_S,"PlayerComputer",PlayerComputer);
  tolua_constant(tolua_S,"PlayerPerson",PlayerPerson);
  tolua_constant(tolua_S,"PlayerRescuePassive",PlayerRescuePassive);
  tolua_constant(tolua_S,"PlayerRescueActive",PlayerRescueActive);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CPlayer","CPlayer","",tolua_collect_CPlayer);
  #else
  tolua_cclass(tolua_S,"CPlayer","CPlayer","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CPlayer");
   tolua_variable(tolua_S,"Index",tolua_get_CPlayer_Index,tolua_set_CPlayer_Index);
   tolua_variable(tolua_S,"Name",tolua_get_CPlayer_Name,tolua_set_CPlayer_Name);
   tolua_variable(tolua_S,"Type",tolua_get_CPlayer_Type,tolua_set_CPlayer_Type);
   tolua_variable(tolua_S,"Race",tolua_get_CPlayer_Race,tolua_set_CPlayer_Race);
   tolua_variable(tolua_S,"AiName",tolua_get_CPlayer_AiName,tolua_set_CPlayer_AiName);
   tolua_variable(tolua_S,"StartPos",tolua_get_CPlayer_StartPos,tolua_set_CPlayer_StartPos);
   tolua_function(tolua_S,"SetStartView",tolua_stratagus_CPlayer_SetStartView00);
   tolua_array(tolua_S,"Resources",tolua_get_stratagus_CPlayer_Resources,tolua_set_stratagus_CPlayer_Resources);
   tolua_array(tolua_S,"StoredResources",tolua_get_stratagus_CPlayer_StoredResources,tolua_set_stratagus_CPlayer_StoredResources);
   tolua_array(tolua_S,"Incomes",tolua_get_stratagus_CPlayer_Incomes,tolua_set_stratagus_CPlayer_Incomes);
   tolua_array(tolua_S,"Revenue",tolua_get_stratagus_CPlayer_Revenue,NULL);
   tolua_array(tolua_S,"UnitTypesCount",tolua_get_stratagus_CPlayer_UnitTypesCount,NULL);
   tolua_array(tolua_S,"UnitTypesAiActiveCount",tolua_get_stratagus_CPlayer_UnitTypesAiActiveCount,NULL);
   tolua_variable(tolua_S,"AiEnabled",tolua_get_CPlayer_AiEnabled,tolua_set_CPlayer_AiEnabled);
   tolua_variable(tolua_S,"NumBuildings",tolua_get_CPlayer_NumBuildings,tolua_set_CPlayer_NumBuildings);
   tolua_variable(tolua_S,"Supply",tolua_get_CPlayer_Supply,tolua_set_CPlayer_Supply);
   tolua_variable(tolua_S,"Demand",tolua_get_CPlayer_Demand,tolua_set_CPlayer_Demand);
   tolua_variable(tolua_S,"UnitLimit",tolua_get_CPlayer_UnitLimit,tolua_set_CPlayer_UnitLimit);
   tolua_variable(tolua_S,"BuildingLimit",tolua_get_CPlayer_BuildingLimit,tolua_set_CPlayer_BuildingLimit);
   tolua_variable(tolua_S,"TotalUnitLimit",tolua_get_CPlayer_TotalUnitLimit,tolua_set_CPlayer_TotalUnitLimit);
   tolua_variable(tolua_S,"Score",tolua_get_CPlayer_Score,tolua_set_CPlayer_Score);
   tolua_variable(tolua_S,"TotalUnits",tolua_get_CPlayer_TotalUnits,tolua_set_CPlayer_TotalUnits);
   tolua_variable(tolua_S,"TotalBuildings",tolua_get_CPlayer_TotalBuildings,tolua_set_CPlayer_TotalBuildings);
   tolua_array(tolua_S,"TotalResources",tolua_get_stratagus_CPlayer_TotalResources,tolua_set_stratagus_CPlayer_TotalResources);
   tolua_variable(tolua_S,"TotalRazings",tolua_get_CPlayer_TotalRazings,tolua_set_CPlayer_TotalRazings);
   tolua_variable(tolua_S,"TotalKills",tolua_get_CPlayer_TotalKills,tolua_set_CPlayer_TotalKills);
   tolua_array(tolua_S,"SpeedResourcesHarvest",tolua_get_stratagus_CPlayer_SpeedResourcesHarvest,tolua_set_stratagus_CPlayer_SpeedResourcesHarvest);
   tolua_array(tolua_S,"SpeedResourcesReturn",tolua_get_stratagus_CPlayer_SpeedResourcesReturn,tolua_set_stratagus_CPlayer_SpeedResourcesReturn);
   tolua_variable(tolua_S,"SpeedBuild",tolua_get_CPlayer_SpeedBuild,tolua_set_CPlayer_SpeedBuild);
   tolua_variable(tolua_S,"SpeedTrain",tolua_get_CPlayer_SpeedTrain,tolua_set_CPlayer_SpeedTrain);
   tolua_variable(tolua_S,"SpeedUpgrade",tolua_get_CPlayer_SpeedUpgrade,tolua_set_CPlayer_SpeedUpgrade);
   tolua_variable(tolua_S,"SpeedResearch",tolua_get_CPlayer_SpeedResearch,tolua_set_CPlayer_SpeedResearch);
   tolua_function(tolua_S,"GetUnit",tolua_stratagus_CPlayer_GetUnit00);
   tolua_function(tolua_S,"GetUnitCount",tolua_stratagus_CPlayer_GetUnitCount00);
   tolua_function(tolua_S,"IsEnemy",tolua_stratagus_CPlayer_IsEnemy00);
   tolua_function(tolua_S,"IsEnemy",tolua_stratagus_CPlayer_IsEnemy01);
   tolua_function(tolua_S,"IsAllied",tolua_stratagus_CPlayer_IsAllied00);
   tolua_function(tolua_S,"IsAllied",tolua_stratagus_CPlayer_IsAllied01);
   tolua_function(tolua_S,"HasSharedVisionWith",tolua_stratagus_CPlayer_HasSharedVisionWith00);
   tolua_function(tolua_S,"IsTeamed",tolua_stratagus_CPlayer_IsTeamed00);
   tolua_function(tolua_S,"IsTeamed",tolua_stratagus_CPlayer_IsTeamed01);
  tolua_endmodule(tolua_S);
  tolua_array(tolua_S,"Players",tolua_get_stratagus_Players,NULL);
  tolua_variable(tolua_S,"ThisPlayer",tolua_get_ThisPlayer_ptr,NULL);
  tolua_function(tolua_S,"GetEffectsVolume",tolua_stratagus_GetEffectsVolume00);
  tolua_function(tolua_S,"SetEffectsVolume",tolua_stratagus_SetEffectsVolume00);
  tolua_function(tolua_S,"GetMusicVolume",tolua_stratagus_GetMusicVolume00);
  tolua_function(tolua_S,"SetMusicVolume",tolua_stratagus_SetMusicVolume00);
  tolua_function(tolua_S,"SetEffectsEnabled",tolua_stratagus_SetEffectsEnabled00);
  tolua_function(tolua_S,"IsEffectsEnabled",tolua_stratagus_IsEffectsEnabled00);
  tolua_function(tolua_S,"SetMusicEnabled",tolua_stratagus_SetMusicEnabled00);
  tolua_function(tolua_S,"IsMusicEnabled",tolua_stratagus_IsMusicEnabled00);
  tolua_function(tolua_S,"PlayFile",tolua_stratagus_PlayFile00);

  { /* begin embedded lua code */
   int top = lua_gettop(tolua_S);
   static const unsigned char B[] = {
    10,102,117,110, 99,116,105,111,110, 32, 80,108, 97,121, 83,
    111,117,110,100, 70,105,108,101, 40,102,105,108,101, 44, 32,
     99, 97,108,108, 98, 97, 99,107, 41, 10,114,101,116,117,114,
    110, 32, 80,108, 97,121, 70,105,108,101, 40,102,105,108,101,
     44, 32, 76,117, 97, 65, 99,116,105,111,110, 76,105,115,116,
    101,110,101,114, 58,110,101,119, 40, 99, 97,108,108, 98, 97,
     99,107, 41, 41, 10,101,110,100, 45, 45, 45, 45, 45, 45, 45,
     45, 45, 45, 45, 45, 45, 45,32
   };
   tolua_dobuffer(tolua_S,(char*)B,sizeof(B),"tolua: embedded Lua code 4");
   lua_settop(tolua_S, top);
  } /* end of embedded lua code */

  tolua_function(tolua_S,"PlayMusic",tolua_stratagus_PlayMusic00);
  tolua_function(tolua_S,"StopMusic",tolua_stratagus_StopMusic00);
  tolua_function(tolua_S,"IsMusicPlaying",tolua_stratagus_IsMusicPlaying00);
  tolua_function(tolua_S,"SetChannelVolume",tolua_stratagus_SetChannelVolume00);
  tolua_function(tolua_S,"SetChannelStereo",tolua_stratagus_SetChannelStereo00);
  tolua_function(tolua_S,"StopChannel",tolua_stratagus_StopChannel00);
  tolua_function(tolua_S,"StopAllChannels",tolua_stratagus_StopAllChannels00);
  tolua_function(tolua_S,"Translate",tolua_stratagus_Translate00);
  tolua_function(tolua_S,"LoadPO",tolua_stratagus_LoadPO00);
  tolua_function(tolua_S,"SetTranslationsFiles",tolua_stratagus_SetTranslationsFiles00);
  tolua_function(tolua_S,"GetNumOpponents",tolua_stratagus_GetNumOpponents00);
  tolua_function(tolua_S,"GetTimer",tolua_stratagus_GetTimer00);
  tolua_function(tolua_S,"ActionVictory",tolua_stratagus_ActionVictory00);
  tolua_function(tolua_S,"ActionDefeat",tolua_stratagus_ActionDefeat00);
  tolua_function(tolua_S,"ActionDraw",tolua_stratagus_ActionDraw00);
  tolua_function(tolua_S,"ActionSetTimer",tolua_stratagus_ActionSetTimer00);
  tolua_function(tolua_S,"ActionStartTimer",tolua_stratagus_ActionStartTimer00);
  tolua_function(tolua_S,"ActionStopTimer",tolua_stratagus_ActionStopTimer00);
  tolua_function(tolua_S,"SetTrigger",tolua_stratagus_SetTrigger00);
  tolua_constant(tolua_S,"VIEWPORT_SINGLE",VIEWPORT_SINGLE);
  tolua_constant(tolua_S,"VIEWPORT_SPLIT_HORIZ",VIEWPORT_SPLIT_HORIZ);
  tolua_constant(tolua_S,"VIEWPORT_SPLIT_HORIZ3",VIEWPORT_SPLIT_HORIZ3);
  tolua_constant(tolua_S,"VIEWPORT_SPLIT_VERT",VIEWPORT_SPLIT_VERT);
  tolua_constant(tolua_S,"VIEWPORT_QUAD",VIEWPORT_QUAD);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CUIButton","CUIButton","",tolua_collect_CUIButton);
  #else
  tolua_cclass(tolua_S,"CUIButton","CUIButton","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CUIButton");
   tolua_function(tolua_S,"new",tolua_stratagus_CUIButton_new00);
   tolua_function(tolua_S,"new_local",tolua_stratagus_CUIButton_new00_local);
   tolua_function(tolua_S,".call",tolua_stratagus_CUIButton_new00_local);
   tolua_variable(tolua_S,"X",tolua_get_CUIButton_X,tolua_set_CUIButton_X);
   tolua_variable(tolua_S,"Y",tolua_get_CUIButton_Y,tolua_set_CUIButton_Y);
   tolua_variable(tolua_S,"Text",tolua_get_CUIButton_Text,tolua_set_CUIButton_Text);
   tolua_variable(tolua_S,"Style",tolua_get_CUIButton_Style_ptr,tolua_set_CUIButton_Style_ptr);
  tolua_endmodule(tolua_S);

  { /* begin embedded lua code */
   int top = lua_gettop(tolua_S);
   static const unsigned char B[] = {
    10, 67, 85, 73, 66,117,116,116,111,110, 46, 83,101,116, 67,
     97,108,108, 98, 97, 99,107, 32, 61, 32,102,117,110, 99,116,
    105,111,110, 40,119, 44, 32,102, 41, 32,101,110,100, 32, 45,
     45, 45, 45,32
   };
   tolua_dobuffer(tolua_S,(char*)B,sizeof(B),"tolua: embedded Lua code 5");
   lua_settop(tolua_S, top);
  } /* end of embedded lua code */

  tolua_cclass(tolua_S,"CMapArea","CMapArea","",NULL);
  tolua_beginmodule(tolua_S,"CMapArea");
   tolua_variable(tolua_S,"X",tolua_get_CMapArea_X,tolua_set_CMapArea_X);
   tolua_variable(tolua_S,"Y",tolua_get_CMapArea_Y,tolua_set_CMapArea_Y);
   tolua_variable(tolua_S,"EndX",tolua_get_CMapArea_EndX,tolua_set_CMapArea_EndX);
   tolua_variable(tolua_S,"EndY",tolua_get_CMapArea_EndY,tolua_set_CMapArea_EndY);
   tolua_variable(tolua_S,"ScrollPaddingLeft",tolua_get_CMapArea_ScrollPaddingLeft,tolua_set_CMapArea_ScrollPaddingLeft);
   tolua_variable(tolua_S,"ScrollPaddingRight",tolua_get_CMapArea_ScrollPaddingRight,tolua_set_CMapArea_ScrollPaddingRight);
   tolua_variable(tolua_S,"ScrollPaddingTop",tolua_get_CMapArea_ScrollPaddingTop,tolua_set_CMapArea_ScrollPaddingTop);
   tolua_variable(tolua_S,"ScrollPaddingBottom",tolua_get_CMapArea_ScrollPaddingBottom,tolua_set_CMapArea_ScrollPaddingBottom);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CViewport","CViewport","",NULL);
  tolua_beginmodule(tolua_S,"CViewport");
  tolua_endmodule(tolua_S);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CFiller","CFiller","",tolua_collect_CFiller);
  #else
  tolua_cclass(tolua_S,"CFiller","CFiller","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CFiller");
   tolua_function(tolua_S,"new",tolua_stratagus_CFiller_new00);
   tolua_function(tolua_S,"new_local",tolua_stratagus_CFiller_new00_local);
   tolua_function(tolua_S,".call",tolua_stratagus_CFiller_new00_local);
   tolua_variable(tolua_S,"G",tolua_get_CFiller_G,tolua_set_CFiller_G);
   tolua_variable(tolua_S,"X",tolua_get_CFiller_X,tolua_set_CFiller_X);
   tolua_variable(tolua_S,"Y",tolua_get_CFiller_Y,tolua_set_CFiller_Y);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"vector_CFiller_","vector<CFiller>","",NULL);
  tolua_beginmodule(tolua_S,"vector_CFiller_");
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_CFiller___geti00);
   tolua_function(tolua_S,".seti",tolua_stratagus_vector_CFiller___seti00);
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_CFiller___geti01);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_CFiller__at00);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_CFiller__at01);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_CFiller__front00);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_CFiller__front01);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_CFiller__back00);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_CFiller__back01);
   tolua_function(tolua_S,"push_back",tolua_stratagus_vector_CFiller__push_back00);
   tolua_function(tolua_S,"pop_back",tolua_stratagus_vector_CFiller__pop_back00);
   tolua_function(tolua_S,"assign",tolua_stratagus_vector_CFiller__assign00);
   tolua_function(tolua_S,"clear",tolua_stratagus_vector_CFiller__clear00);
   tolua_function(tolua_S,"empty",tolua_stratagus_vector_CFiller__empty00);
   tolua_function(tolua_S,"size",tolua_stratagus_vector_CFiller__size00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"vector_CUIButton_","vector<CUIButton>","",NULL);
  tolua_beginmodule(tolua_S,"vector_CUIButton_");
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_CUIButton___geti00);
   tolua_function(tolua_S,".seti",tolua_stratagus_vector_CUIButton___seti00);
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_CUIButton___geti01);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_CUIButton__at00);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_CUIButton__at01);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_CUIButton__front00);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_CUIButton__front01);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_CUIButton__back00);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_CUIButton__back01);
   tolua_function(tolua_S,"push_back",tolua_stratagus_vector_CUIButton__push_back00);
   tolua_function(tolua_S,"pop_back",tolua_stratagus_vector_CUIButton__pop_back00);
   tolua_function(tolua_S,"assign",tolua_stratagus_vector_CUIButton__assign00);
   tolua_function(tolua_S,"clear",tolua_stratagus_vector_CUIButton__clear00);
   tolua_function(tolua_S,"empty",tolua_stratagus_vector_CUIButton__empty00);
   tolua_function(tolua_S,"size",tolua_stratagus_vector_CUIButton__size00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"vector_CUIUserButton_","vector<CUIUserButton>","",NULL);
  tolua_beginmodule(tolua_S,"vector_CUIUserButton_");
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_CUIUserButton___geti00);
   tolua_function(tolua_S,".seti",tolua_stratagus_vector_CUIUserButton___seti00);
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_CUIUserButton___geti01);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_CUIUserButton__at00);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_CUIUserButton__at01);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_CUIUserButton__front00);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_CUIUserButton__front01);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_CUIUserButton__back00);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_CUIUserButton__back01);
   tolua_function(tolua_S,"push_back",tolua_stratagus_vector_CUIUserButton__push_back00);
   tolua_function(tolua_S,"pop_back",tolua_stratagus_vector_CUIUserButton__pop_back00);
   tolua_function(tolua_S,"assign",tolua_stratagus_vector_CUIUserButton__assign00);
   tolua_function(tolua_S,"clear",tolua_stratagus_vector_CUIUserButton__clear00);
   tolua_function(tolua_S,"empty",tolua_stratagus_vector_CUIUserButton__empty00);
   tolua_function(tolua_S,"size",tolua_stratagus_vector_CUIUserButton__size00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"vector_string_","vector<string>","",NULL);
  tolua_beginmodule(tolua_S,"vector_string_");
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_string___geti00);
   tolua_function(tolua_S,".seti",tolua_stratagus_vector_string___seti00);
   tolua_function(tolua_S,".geti",tolua_stratagus_vector_string___geti01);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_string__at00);
   tolua_function(tolua_S,"at",tolua_stratagus_vector_string__at01);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_string__front00);
   tolua_function(tolua_S,"front",tolua_stratagus_vector_string__front01);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_string__back00);
   tolua_function(tolua_S,"back",tolua_stratagus_vector_string__back01);
   tolua_function(tolua_S,"push_back",tolua_stratagus_vector_string__push_back00);
   tolua_function(tolua_S,"pop_back",tolua_stratagus_vector_string__pop_back00);
   tolua_function(tolua_S,"assign",tolua_stratagus_vector_string__assign00);
   tolua_function(tolua_S,"clear",tolua_stratagus_vector_string__clear00);
   tolua_function(tolua_S,"empty",tolua_stratagus_vector_string__empty00);
   tolua_function(tolua_S,"size",tolua_stratagus_vector_string__size00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CButtonPanel","CButtonPanel","",NULL);
  tolua_beginmodule(tolua_S,"CButtonPanel");
   tolua_variable(tolua_S,"X",tolua_get_CButtonPanel_X,tolua_set_CButtonPanel_X);
   tolua_variable(tolua_S,"Y",tolua_get_CButtonPanel_Y,tolua_set_CButtonPanel_Y);
   tolua_variable(tolua_S,"Buttons",tolua_get_CButtonPanel_Buttons,tolua_set_CButtonPanel_Buttons);
   tolua_variable(tolua_S,"AutoCastBorderColorRGB",tolua_get_CButtonPanel_AutoCastBorderColorRGB,tolua_set_CButtonPanel_AutoCastBorderColorRGB);
   tolua_variable(tolua_S,"ShowCommandKey",tolua_get_CButtonPanel_ShowCommandKey,tolua_set_CButtonPanel_ShowCommandKey);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CPieMenu","CPieMenu","",NULL);
  tolua_beginmodule(tolua_S,"CPieMenu");
   tolua_variable(tolua_S,"G",tolua_get_CPieMenu_G,tolua_set_CPieMenu_G);
   tolua_variable(tolua_S,"MouseButton",tolua_get_CPieMenu_MouseButton,tolua_set_CPieMenu_MouseButton);
   tolua_array(tolua_S,"X",tolua_get_stratagus_CPieMenu_X,tolua_set_stratagus_CPieMenu_X);
   tolua_array(tolua_S,"Y",tolua_get_stratagus_CPieMenu_Y,tolua_set_stratagus_CPieMenu_Y);
   tolua_function(tolua_S,"SetRadius",tolua_stratagus_CPieMenu_SetRadius00);
  tolua_endmodule(tolua_S);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CResourceInfo","CResourceInfo","",tolua_collect_CResourceInfo);
  #else
  tolua_cclass(tolua_S,"CResourceInfo","CResourceInfo","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CResourceInfo");
   tolua_variable(tolua_S,"G",tolua_get_CResourceInfo_G,tolua_set_CResourceInfo_G);
   tolua_variable(tolua_S,"IconFrame",tolua_get_CResourceInfo_IconFrame,tolua_set_CResourceInfo_IconFrame);
   tolua_variable(tolua_S,"IconX",tolua_get_CResourceInfo_IconX,tolua_set_CResourceInfo_IconX);
   tolua_variable(tolua_S,"IconY",tolua_get_CResourceInfo_IconY,tolua_set_CResourceInfo_IconY);
   tolua_variable(tolua_S,"IconWidth",tolua_get_CResourceInfo_IconWidth,tolua_set_CResourceInfo_IconWidth);
   tolua_variable(tolua_S,"TextX",tolua_get_CResourceInfo_TextX,tolua_set_CResourceInfo_TextX);
   tolua_variable(tolua_S,"TextY",tolua_get_CResourceInfo_TextY,tolua_set_CResourceInfo_TextY);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CInfoPanel","CInfoPanel","",NULL);
  tolua_beginmodule(tolua_S,"CInfoPanel");
   tolua_variable(tolua_S,"G",tolua_get_CInfoPanel_G,tolua_set_CInfoPanel_G);
   tolua_variable(tolua_S,"X",tolua_get_CInfoPanel_X,tolua_set_CInfoPanel_X);
   tolua_variable(tolua_S,"Y",tolua_get_CInfoPanel_Y,tolua_set_CInfoPanel_Y);
  tolua_endmodule(tolua_S);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CUIUserButton","CUIUserButton","",tolua_collect_CUIUserButton);
  #else
  tolua_cclass(tolua_S,"CUIUserButton","CUIUserButton","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CUIUserButton");
   tolua_function(tolua_S,"new",tolua_stratagus_CUIUserButton_new00);
   tolua_function(tolua_S,"new_local",tolua_stratagus_CUIUserButton_new00_local);
   tolua_function(tolua_S,".call",tolua_stratagus_CUIUserButton_new00_local);
   tolua_variable(tolua_S,"Clicked",tolua_get_CUIUserButton_Clicked,tolua_set_CUIUserButton_Clicked);
   tolua_variable(tolua_S,"Button",tolua_get_CUIUserButton_Button,tolua_set_CUIUserButton_Button);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CStatusLine","CStatusLine","",NULL);
  tolua_beginmodule(tolua_S,"CStatusLine");
   tolua_function(tolua_S,"Set",tolua_stratagus_CStatusLine_Set00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CStatusLine_Get00);
   tolua_function(tolua_S,"Clear",tolua_stratagus_CStatusLine_Clear00);
   tolua_variable(tolua_S,"Width",tolua_get_CStatusLine_Width,tolua_set_CStatusLine_Width);
   tolua_variable(tolua_S,"TextX",tolua_get_CStatusLine_TextX,tolua_set_CStatusLine_TextX);
   tolua_variable(tolua_S,"TextY",tolua_get_CStatusLine_TextY,tolua_set_CStatusLine_TextY);
   tolua_variable(tolua_S,"Font",tolua_get_CStatusLine_Font_ptr,tolua_set_CStatusLine_Font_ptr);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CUITimer","CUITimer","",NULL);
  tolua_beginmodule(tolua_S,"CUITimer");
   tolua_variable(tolua_S,"X",tolua_get_CUITimer_X,tolua_set_CUITimer_X);
   tolua_variable(tolua_S,"Y",tolua_get_CUITimer_Y,tolua_set_CUITimer_Y);
   tolua_variable(tolua_S,"Font",tolua_get_CUITimer_Font_ptr,tolua_set_CUITimer_Font_ptr);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CUserInterface","CUserInterface","",NULL);
  tolua_beginmodule(tolua_S,"CUserInterface");
   tolua_variable(tolua_S,"NormalFontColor",tolua_get_CUserInterface_NormalFontColor,tolua_set_CUserInterface_NormalFontColor);
   tolua_variable(tolua_S,"ReverseFontColor",tolua_get_CUserInterface_ReverseFontColor,tolua_set_CUserInterface_ReverseFontColor);
   tolua_variable(tolua_S,"Fillers",tolua_get_CUserInterface_Fillers,tolua_set_CUserInterface_Fillers);
   tolua_array(tolua_S,"Resources",tolua_get_stratagus_CUserInterface_Resources,tolua_set_stratagus_CUserInterface_Resources);
   tolua_variable(tolua_S,"InfoPanel",tolua_get_CUserInterface_InfoPanel,tolua_set_CUserInterface_InfoPanel);
   tolua_variable(tolua_S,"DefaultUnitPortrait",tolua_get_CUserInterface_DefaultUnitPortrait,tolua_set_CUserInterface_DefaultUnitPortrait);
   tolua_variable(tolua_S,"SingleSelectedButton",tolua_get_CUserInterface_SingleSelectedButton_ptr,tolua_set_CUserInterface_SingleSelectedButton_ptr);
   tolua_variable(tolua_S,"SelectedButtons",tolua_get_CUserInterface_SelectedButtons,tolua_set_CUserInterface_SelectedButtons);
   tolua_variable(tolua_S,"MaxSelectedFont",tolua_get_CUserInterface_MaxSelectedFont_ptr,tolua_set_CUserInterface_MaxSelectedFont_ptr);
   tolua_variable(tolua_S,"MaxSelectedTextX",tolua_get_CUserInterface_MaxSelectedTextX,tolua_set_CUserInterface_MaxSelectedTextX);
   tolua_variable(tolua_S,"MaxSelectedTextY",tolua_get_CUserInterface_MaxSelectedTextY,tolua_set_CUserInterface_MaxSelectedTextY);
   tolua_variable(tolua_S,"SingleTrainingButton",tolua_get_CUserInterface_SingleTrainingButton_ptr,tolua_set_CUserInterface_SingleTrainingButton_ptr);
   tolua_variable(tolua_S,"TrainingButtons",tolua_get_CUserInterface_TrainingButtons,tolua_set_CUserInterface_TrainingButtons);
   tolua_variable(tolua_S,"UpgradingButton",tolua_get_CUserInterface_UpgradingButton_ptr,tolua_set_CUserInterface_UpgradingButton_ptr);
   tolua_variable(tolua_S,"ResearchingButton",tolua_get_CUserInterface_ResearchingButton_ptr,tolua_set_CUserInterface_ResearchingButton_ptr);
   tolua_variable(tolua_S,"TransportingButtons",tolua_get_CUserInterface_TransportingButtons,tolua_set_CUserInterface_TransportingButtons);
   tolua_variable(tolua_S,"LifeBarColorNames",tolua_get_CUserInterface_LifeBarColorNames,tolua_set_CUserInterface_LifeBarColorNames);
   tolua_variable(tolua_S,"LifeBarPercents",tolua_get_CUserInterface_LifeBarPercents,tolua_set_CUserInterface_LifeBarPercents);
   tolua_variable(tolua_S,"LifeBarYOffset",tolua_get_CUserInterface_LifeBarYOffset,tolua_set_CUserInterface_LifeBarYOffset);
   tolua_variable(tolua_S,"LifeBarPadding",tolua_get_CUserInterface_LifeBarPadding,tolua_set_CUserInterface_LifeBarPadding);
   tolua_variable(tolua_S,"LifeBarBorder",tolua_get_CUserInterface_LifeBarBorder,tolua_set_CUserInterface_LifeBarBorder);
   tolua_variable(tolua_S,"CompletedBarColorRGB",tolua_get_CUserInterface_CompletedBarColorRGB,tolua_set_CUserInterface_CompletedBarColorRGB);
   tolua_variable(tolua_S,"CompletedBarShadow",tolua_get_CUserInterface_CompletedBarShadow,tolua_set_CUserInterface_CompletedBarShadow);
   tolua_variable(tolua_S,"ButtonPanel",tolua_get_CUserInterface_ButtonPanel,tolua_set_CUserInterface_ButtonPanel);
   tolua_variable(tolua_S,"PieMenu",tolua_get_CUserInterface_PieMenu,tolua_set_CUserInterface_PieMenu);
   tolua_variable(tolua_S,"MouseViewport",tolua_get_CUserInterface_MouseViewport_ptr,tolua_set_CUserInterface_MouseViewport_ptr);
   tolua_variable(tolua_S,"MapArea",tolua_get_CUserInterface_MapArea,tolua_set_CUserInterface_MapArea);
   tolua_variable(tolua_S,"MessageFont",tolua_get_CUserInterface_MessageFont_ptr,tolua_set_CUserInterface_MessageFont_ptr);
   tolua_variable(tolua_S,"MessageScrollSpeed",tolua_get_CUserInterface_MessageScrollSpeed,tolua_set_CUserInterface_MessageScrollSpeed);
   tolua_variable(tolua_S,"MenuButton",tolua_get_CUserInterface_MenuButton,tolua_set_CUserInterface_MenuButton);
   tolua_variable(tolua_S,"NetworkMenuButton",tolua_get_CUserInterface_NetworkMenuButton,tolua_set_CUserInterface_NetworkMenuButton);
   tolua_variable(tolua_S,"NetworkDiplomacyButton",tolua_get_CUserInterface_NetworkDiplomacyButton,tolua_set_CUserInterface_NetworkDiplomacyButton);
   tolua_variable(tolua_S,"UserButtons",tolua_get_CUserInterface_UserButtons,tolua_set_CUserInterface_UserButtons);
   tolua_variable(tolua_S,"Minimap",tolua_get_CUserInterface_Minimap,tolua_set_CUserInterface_Minimap);
   tolua_variable(tolua_S,"StatusLine",tolua_get_CUserInterface_StatusLine,tolua_set_CUserInterface_StatusLine);
   tolua_variable(tolua_S,"Timer",tolua_get_CUserInterface_Timer,tolua_set_CUserInterface_Timer);
   tolua_variable(tolua_S,"EditorSettingsAreaTopLeft",tolua_get_CUserInterface_EditorSettingsAreaTopLeft,tolua_set_CUserInterface_EditorSettingsAreaTopLeft);
   tolua_variable(tolua_S,"EditorSettingsAreaBottomRight",tolua_get_CUserInterface_EditorSettingsAreaBottomRight,tolua_set_CUserInterface_EditorSettingsAreaBottomRight);
   tolua_variable(tolua_S,"EditorButtonAreaTopLeft",tolua_get_CUserInterface_EditorButtonAreaTopLeft,tolua_set_CUserInterface_EditorButtonAreaTopLeft);
   tolua_variable(tolua_S,"EditorButtonAreaBottomRight",tolua_get_CUserInterface_EditorButtonAreaBottomRight,tolua_set_CUserInterface_EditorButtonAreaBottomRight);
   tolua_function(tolua_S,"Load",tolua_stratagus_CUserInterface_Load00);
  tolua_endmodule(tolua_S);
  tolua_variable(tolua_S,"UI",tolua_get_UI,NULL);
  tolua_cclass(tolua_S,"CIcon","CIcon","",NULL);
  tolua_beginmodule(tolua_S,"CIcon");
   tolua_function(tolua_S,"New",tolua_stratagus_CIcon_New00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CIcon_Get00);
   tolua_variable(tolua_S,"Ident",tolua_get_CIcon_Ident,NULL);
   tolua_variable(tolua_S,"G",tolua_get_CIcon_G,tolua_set_CIcon_G);
   tolua_variable(tolua_S,"Frame",tolua_get_CIcon_Frame,tolua_set_CIcon_Frame);
   tolua_function(tolua_S,"ClearExtraGraphics",tolua_stratagus_CIcon_ClearExtraGraphics00);
   tolua_function(tolua_S,"AddSingleSelectionGraphic",tolua_stratagus_CIcon_AddSingleSelectionGraphic00);
   tolua_function(tolua_S,"AddGroupSelectionGraphic",tolua_stratagus_CIcon_AddGroupSelectionGraphic00);
   tolua_function(tolua_S,"AddContainedGraphic",tolua_stratagus_CIcon_AddContainedGraphic00);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"FindButtonStyle",tolua_stratagus_FindButtonStyle00);
  tolua_function(tolua_S,"GetMouseScroll",tolua_stratagus_GetMouseScroll00);
  tolua_function(tolua_S,"SetMouseScroll",tolua_stratagus_SetMouseScroll00);
  tolua_function(tolua_S,"GetKeyScroll",tolua_stratagus_GetKeyScroll00);
  tolua_function(tolua_S,"SetKeyScroll",tolua_stratagus_SetKeyScroll00);
  tolua_function(tolua_S,"GetGrabMouse",tolua_stratagus_GetGrabMouse00);
  tolua_function(tolua_S,"SetGrabMouse",tolua_stratagus_SetGrabMouse00);
  tolua_function(tolua_S,"GetLeaveStops",tolua_stratagus_GetLeaveStops00);
  tolua_function(tolua_S,"SetLeaveStops",tolua_stratagus_SetLeaveStops00);
  tolua_function(tolua_S,"GetDoubleClickDelay",tolua_stratagus_GetDoubleClickDelay00);
  tolua_function(tolua_S,"SetDoubleClickDelay",tolua_stratagus_SetDoubleClickDelay00);
  tolua_function(tolua_S,"GetHoldClickDelay",tolua_stratagus_GetHoldClickDelay00);
  tolua_function(tolua_S,"SetHoldClickDelay",tolua_stratagus_SetHoldClickDelay00);
  tolua_function(tolua_S,"SetScrollMargins",tolua_stratagus_SetScrollMargins00);
  tolua_variable(tolua_S,"CursorScreenPos",tolua_get_CursorScreenPos,tolua_set_CursorScreenPos);
  tolua_cclass(tolua_S,"Vec2i","Vec2i","",NULL);
  tolua_beginmodule(tolua_S,"Vec2i");
   tolua_variable(tolua_S,"x",tolua_get_Vec2i_x,tolua_set_Vec2i_x);
   tolua_variable(tolua_S,"y",tolua_get_Vec2i_y,tolua_set_Vec2i_y);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CUnit","CUnit","",NULL);
  tolua_beginmodule(tolua_S,"CUnit");
   tolua_variable(tolua_S,"tilePos",tolua_get_CUnit_tilePos,NULL);
   tolua_variable(tolua_S,"Type",tolua_get_CUnit_Type_ptr,NULL);
   tolua_variable(tolua_S,"Player",tolua_get_CUnit_Player_ptr,NULL);
   tolua_variable(tolua_S,"Goal",tolua_get_CUnit_Goal_ptr,tolua_set_CUnit_Goal_ptr);
   tolua_variable(tolua_S,"Active",tolua_get_CUnit_Active,tolua_set_CUnit_Active);
   tolua_variable(tolua_S,"ResourcesHeld",tolua_get_CUnit_ResourcesHeld,tolua_set_CUnit_ResourcesHeld);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CPreference","CPreference","",NULL);
  tolua_beginmodule(tolua_S,"CPreference");
   tolua_variable(tolua_S,"ShowSightRange",tolua_get_CPreference_ShowSightRange,tolua_set_CPreference_ShowSightRange);
   tolua_variable(tolua_S,"ShowReactionRange",tolua_get_CPreference_ShowReactionRange,tolua_set_CPreference_ShowReactionRange);
   tolua_variable(tolua_S,"ShowAttackRange",tolua_get_CPreference_ShowAttackRange,tolua_set_CPreference_ShowAttackRange);
   tolua_variable(tolua_S,"ShowMessages",tolua_get_CPreference_ShowMessages,tolua_set_CPreference_ShowMessages);
   tolua_variable(tolua_S,"ShowNoSelectionStats",tolua_get_CPreference_ShowNoSelectionStats,tolua_set_CPreference_ShowNoSelectionStats);
   tolua_variable(tolua_S,"BigScreen",tolua_get_CPreference_BigScreen,tolua_set_CPreference_BigScreen);
   tolua_variable(tolua_S,"PauseOnLeave",tolua_get_CPreference_PauseOnLeave,tolua_set_CPreference_PauseOnLeave);
   tolua_variable(tolua_S,"AiExplores",tolua_get_CPreference_AiExplores,tolua_set_CPreference_AiExplores);
   tolua_variable(tolua_S,"GrayscaleIcons",tolua_get_CPreference_GrayscaleIcons,tolua_set_CPreference_GrayscaleIcons);
   tolua_variable(tolua_S,"IconsShift",tolua_get_CPreference_IconsShift,tolua_set_CPreference_IconsShift);
   tolua_variable(tolua_S,"StereoSound",tolua_get_CPreference_StereoSound,tolua_set_CPreference_StereoSound);
   tolua_variable(tolua_S,"MineNotifications",tolua_get_CPreference_MineNotifications,tolua_set_CPreference_MineNotifications);
   tolua_variable(tolua_S,"DeselectInMine",tolua_get_CPreference_DeselectInMine,tolua_set_CPreference_DeselectInMine);
   tolua_variable(tolua_S,"NoStatusLineTooltips",tolua_get_CPreference_NoStatusLineTooltips,tolua_set_CPreference_NoStatusLineTooltips);
   tolua_variable(tolua_S,"SimplifiedAutoTargeting",tolua_get_CPreference_SimplifiedAutoTargeting,tolua_set_CPreference_SimplifiedAutoTargeting);
   tolua_variable(tolua_S,"AiChecksDependencies",tolua_get_CPreference_AiChecksDependencies,tolua_set_CPreference_AiChecksDependencies);
   tolua_variable(tolua_S,"AllyDepositsAllowed",tolua_get_CPreference_AllyDepositsAllowed,tolua_set_CPreference_AllyDepositsAllowed);
   tolua_variable(tolua_S,"HardwareCursor",tolua_get_CPreference_HardwareCursor,tolua_set_CPreference_HardwareCursor);
   tolua_variable(tolua_S,"SelectionRectangleIndicatesDamage",tolua_get_CPreference_SelectionRectangleIndicatesDamage,tolua_set_CPreference_SelectionRectangleIndicatesDamage);
   tolua_variable(tolua_S,"FormationMovement",tolua_get_CPreference_FormationMovement,tolua_set_CPreference_FormationMovement);
   tolua_variable(tolua_S,"FrameSkip",tolua_get_CPreference_unsigned_FrameSkip,tolua_set_CPreference_unsigned_FrameSkip);
   tolua_variable(tolua_S,"ShowOrders",tolua_get_CPreference_unsigned_ShowOrders,tolua_set_CPreference_unsigned_ShowOrders);
   tolua_variable(tolua_S,"ShowNameDelay",tolua_get_CPreference_unsigned_ShowNameDelay,tolua_set_CPreference_unsigned_ShowNameDelay);
   tolua_variable(tolua_S,"ShowNameTime",tolua_get_CPreference_unsigned_ShowNameTime,tolua_set_CPreference_unsigned_ShowNameTime);
   tolua_variable(tolua_S,"AutosaveMinutes",tolua_get_CPreference_unsigned_AutosaveMinutes,tolua_set_CPreference_unsigned_AutosaveMinutes);
   tolua_variable(tolua_S,"IconFrameG",tolua_get_CPreference_IconFrameG,tolua_set_CPreference_IconFrameG);
   tolua_variable(tolua_S,"PressedIconFrameG",tolua_get_CPreference_PressedIconFrameG,tolua_set_CPreference_PressedIconFrameG);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CUnitManager","CUnitManager","",NULL);
  tolua_beginmodule(tolua_S,"CUnitManager");
   tolua_function(tolua_S,"GetSlotUnit",tolua_stratagus_CUnitManager_GetSlotUnit00);
  tolua_endmodule(tolua_S);
  tolua_variable(tolua_S,"UnitManager",tolua_get_UnitManager_ptr,NULL);
  tolua_variable(tolua_S,"Preference",tolua_get_Preference,tolua_set_Preference);
  tolua_function(tolua_S,"GetUnitUnderCursor",tolua_stratagus_GetUnitUnderCursor00);
  tolua_function(tolua_S,"UnitNumber",tolua_stratagus_UnitNumber00);
  tolua_cclass(tolua_S,"CUnitType","CUnitType","",NULL);
  tolua_beginmodule(tolua_S,"CUnitType");
   tolua_variable(tolua_S,"Ident",tolua_get_CUnitType_Ident,tolua_set_CUnitType_Ident);
   tolua_variable(tolua_S,"Name",tolua_get_CUnitType_Name,tolua_set_CUnitType_Name);
   tolua_variable(tolua_S,"Slot",tolua_get_CUnitType_Slot,NULL);
   tolua_variable(tolua_S,"MinAttackRange",tolua_get_CUnitType_MinAttackRange,tolua_set_CUnitType_MinAttackRange);
   tolua_variable(tolua_S,"ClicksToExplode",tolua_get_CUnitType_ClicksToExplode,tolua_set_CUnitType_ClicksToExplode);
   tolua_variable(tolua_S,"GivesResource",tolua_get_CUnitType_GivesResource,tolua_set_CUnitType_GivesResource);
   tolua_variable(tolua_S,"TileWidth",tolua_get_CUnitType_TileWidth,tolua_set_CUnitType_TileWidth);
   tolua_variable(tolua_S,"TileHeight",tolua_get_CUnitType_TileHeight,tolua_set_CUnitType_TileHeight);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"UnitTypeByIdent",tolua_stratagus_UnitTypeByIdent00);
  tolua_variable(tolua_S,"UnitTypeHumanWall",tolua_get_UnitTypeHumanWall_ptr,tolua_set_UnitTypeHumanWall_ptr);
  tolua_variable(tolua_S,"UnitTypeOrcWall",tolua_get_UnitTypeOrcWall_ptr,tolua_set_UnitTypeOrcWall_ptr);
  tolua_function(tolua_S,"SetMapStat",tolua_stratagus_SetMapStat00);
  tolua_function(tolua_S,"SetMapSound",tolua_stratagus_SetMapSound00);
  tolua_cclass(tolua_S,"CUpgrade","CUpgrade","",NULL);
  tolua_beginmodule(tolua_S,"CUpgrade");
   tolua_function(tolua_S,"New",tolua_stratagus_CUpgrade_New00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CUpgrade_Get00);
   tolua_variable(tolua_S,"Name",tolua_get_CUpgrade_Name,tolua_set_CUpgrade_Name);
   tolua_array(tolua_S,"Costs",tolua_get_stratagus_CUpgrade_Costs,tolua_set_stratagus_CUpgrade_Costs);
   tolua_variable(tolua_S,"Icon",tolua_get_CUpgrade_Icon_ptr,tolua_set_CUpgrade_Icon_ptr);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"InitVideo",tolua_stratagus_InitVideo00);
  tolua_function(tolua_S,"PlayMovie",tolua_stratagus_PlayMovie00);
  tolua_function(tolua_S,"ShowFullImage",tolua_stratagus_ShowFullImage00);
  tolua_function(tolua_S,"SaveMapPNG",tolua_stratagus_SaveMapPNG00);
  tolua_cclass(tolua_S,"CVideo","CVideo","",NULL);
  tolua_beginmodule(tolua_S,"CVideo");
   tolua_variable(tolua_S,"Width",tolua_get_CVideo_Width,tolua_set_CVideo_Width);
   tolua_variable(tolua_S,"Height",tolua_get_CVideo_Height,tolua_set_CVideo_Height);
   tolua_variable(tolua_S,"Depth",tolua_get_CVideo_Depth,tolua_set_CVideo_Depth);
   tolua_variable(tolua_S,"FullScreen",tolua_get_CVideo_FullScreen,tolua_set_CVideo_FullScreen);
   tolua_function(tolua_S,"ResizeScreen",tolua_stratagus_CVideo_ResizeScreen00);
  tolua_endmodule(tolua_S);
  tolua_variable(tolua_S,"Video",tolua_get_Video,NULL);
  tolua_function(tolua_S,"ToggleFullScreen",tolua_stratagus_ToggleFullScreen00);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CGraphicPtr","CGraphicPtr","",tolua_collect_CGraphicPtr);
  #else
  tolua_cclass(tolua_S,"CGraphicPtr","CGraphicPtr","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CGraphicPtr");
   tolua_function(tolua_S,"Load",tolua_stratagus_CGraphicPtr_Load00);
   tolua_function(tolua_S,"Resize",tolua_stratagus_CGraphicPtr_Resize00);
   tolua_function(tolua_S,"ResizeKeepRatio",tolua_stratagus_CGraphicPtr_ResizeKeepRatio00);
   tolua_function(tolua_S,"SetPaletteColor",tolua_stratagus_CGraphicPtr_SetPaletteColor00);
   tolua_function(tolua_S,"OverlayGraphic",tolua_stratagus_CGraphicPtr_OverlayGraphic00);
  tolua_endmodule(tolua_S);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CPlayerColorGraphicPtr","CPlayerColorGraphicPtr","",tolua_collect_CPlayerColorGraphicPtr);
  #else
  tolua_cclass(tolua_S,"CPlayerColorGraphicPtr","CPlayerColorGraphicPtr","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CPlayerColorGraphicPtr");
   tolua_function(tolua_S,"Load",tolua_stratagus_CPlayerColorGraphicPtr_Load00);
   tolua_function(tolua_S,"Resize",tolua_stratagus_CPlayerColorGraphicPtr_Resize00);
   tolua_function(tolua_S,"SetPaletteColor",tolua_stratagus_CPlayerColorGraphicPtr_SetPaletteColor00);
   tolua_function(tolua_S,"OverlayGraphic",tolua_stratagus_CPlayerColorGraphicPtr_OverlayGraphic00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CGraphic","CGraphic","",NULL);
  tolua_beginmodule(tolua_S,"CGraphic");
   tolua_function(tolua_S,"New",tolua_stratagus_CGraphic_New00);
   tolua_function(tolua_S,"ForceNew",tolua_stratagus_CGraphic_ForceNew00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CGraphic_Get00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"CPlayerColorGraphic","CPlayerColorGraphic","CGraphic",NULL);
  tolua_beginmodule(tolua_S,"CPlayerColorGraphic");
   tolua_function(tolua_S,"New",tolua_stratagus_CPlayerColorGraphic_New00);
   tolua_function(tolua_S,"ForceNew",tolua_stratagus_CPlayerColorGraphic_ForceNew00);
   tolua_function(tolua_S,"Get",tolua_stratagus_CPlayerColorGraphic_Get00);
  tolua_endmodule(tolua_S);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"CColor","CColor","",tolua_collect_CColor);
  #else
  tolua_cclass(tolua_S,"CColor","CColor","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"CColor");
   tolua_function(tolua_S,"new",tolua_stratagus_CColor_new00);
   tolua_function(tolua_S,"new_local",tolua_stratagus_CColor_new00_local);
   tolua_function(tolua_S,".call",tolua_stratagus_CColor_new00_local);
   tolua_variable(tolua_S,"R",tolua_get_CColor_unsigned_R,tolua_set_CColor_unsigned_R);
   tolua_variable(tolua_S,"G",tolua_get_CColor_unsigned_G,tolua_set_CColor_unsigned_G);
   tolua_variable(tolua_S,"B",tolua_get_CColor_unsigned_B,tolua_set_CColor_unsigned_B);
   tolua_variable(tolua_S,"A",tolua_get_CColor_unsigned_A,tolua_set_CColor_unsigned_A);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"SetColorCycleAll",tolua_stratagus_SetColorCycleAll00);
  tolua_function(tolua_S,"ClearAllColorCyclingRange",tolua_stratagus_ClearAllColorCyclingRange00);
  tolua_function(tolua_S,"AddColorCyclingRange",tolua_stratagus_AddColorCyclingRange00);
  tolua_function(tolua_S,"SetColorCycleSpeed",tolua_stratagus_SetColorCycleSpeed00);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"MngPtr","MngPtr","",tolua_collect_MngPtr);
  #else
  tolua_cclass(tolua_S,"MngPtr","MngPtr","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"MngPtr");
   tolua_function(tolua_S,"Load",tolua_stratagus_MngPtr_Load00);
   tolua_function(tolua_S,"Draw",tolua_stratagus_MngPtr_Draw00);
   tolua_function(tolua_S,"Reset",tolua_stratagus_MngPtr_Reset00);
  tolua_endmodule(tolua_S);
  tolua_cclass(tolua_S,"Mng","Mng","",NULL);
  tolua_beginmodule(tolua_S,"Mng");
   tolua_function(tolua_S,"New",tolua_stratagus_Mng_New00);
   tolua_variable(tolua_S,"MaxFPS",tolua_get_Mng_MaxFPS,tolua_set_Mng_MaxFPS);
  tolua_endmodule(tolua_S);
  #ifdef __cplusplus
  tolua_cclass(tolua_S,"Movie","Movie","",tolua_collect_Movie);
  #else
  tolua_cclass(tolua_S,"Movie","Movie","",NULL);
  #endif
  tolua_beginmodule(tolua_S,"Movie");
   tolua_function(tolua_S,"new",tolua_stratagus_Movie_new00);
   tolua_function(tolua_S,"new_local",tolua_stratagus_Movie_new00_local);
   tolua_function(tolua_S,".call",tolua_stratagus_Movie_new00_local);
   tolua_function(tolua_S,"Load",tolua_stratagus_Movie_Load00);
   tolua_function(tolua_S,"IsPlaying",tolua_stratagus_Movie_IsPlaying00);
  tolua_endmodule(tolua_S);
  tolua_function(tolua_S,"SaveGame",tolua_stratagus_SaveGame00);
  tolua_function(tolua_S,"DeleteSaveGame",tolua_stratagus_DeleteSaveGame00);
  tolua_function(tolua_S,"_",tolua_stratagus__00);
  tolua_function(tolua_S,"SyncRand",tolua_stratagus_SyncRand00);
  tolua_function(tolua_S,"SyncRand",tolua_stratagus_SyncRand01);
  tolua_function(tolua_S,"CanAccessFile",tolua_stratagus_CanAccessFile00);
  tolua_function(tolua_S,"Exit",tolua_stratagus_Exit00);
  tolua_variable(tolua_S,"CliMapName",tolua_get_CliMapName,tolua_set_CliMapName);
  tolua_variable(tolua_S,"StratagusLibPath",tolua_get_StratagusLibPath,tolua_set_StratagusLibPath);
  tolua_variable(tolua_S,"IsDebugEnabled",tolua_get_IsDebugEnabled,tolua_set_IsDebugEnabled);
 tolua_endmodule(tolua_S);
 return 1;
}


#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 501
 TOLUA_API int luaopen_stratagus (lua_State* tolua_S) {
 return tolua_stratagus_open(tolua_S);
};
#endif

