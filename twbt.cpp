//
//  twbt.cpp
//  twbt
//
//  Created by Vitaly Pronkin on 14/05/14.
//  Copyright (c) 2014 mifki. All rights reserved.
//

#include <sys/stat.h>
#include <stdint.h>
#include <math.h>
#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>

#if defined(WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>

    #define GLEW_STATIC
    #include "glew/glew.h"
    #include "glew/wglew.h"

    float roundf(float x)
    {
       return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
    }

#elif defined(__APPLE__)
    #include <OpenGL/gl.h>

#else
    #include <dlfcn.h>
    #define GL_GLEXT_PROTOTYPES
    #include <GL/gl.h>
    #include <GL/glext.h>
#endif

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "VTableInterpose.h"
#include "MemAccess.h"
#include "VersionInfo.h"
#include "TileTypes.h"
#include "modules/Maps.h"
#include "modules/World.h"
#include "modules/MapCache.h"
#include "modules/Gui.h"
#include "modules/Screen.h"
#include "modules/Buildings.h"
#include "modules/Items.h"
//#include "modules/Units.h"
#include "df/construction.h"
#include "df/block_square_event_frozen_liquidst.h"
#include "df/tiletype.h"
#include "df/graphic.h"
#include "df/enabler.h"
#include "df/d_init.h"
#include "df/renderer.h"
#include "df/interfacest.h"
#include "df/world_raws.h"
#include "df/descriptor_color.h"
#include "df/building.h"
#include "df/building_workshopst.h"
#include "df/building_def_workshopst.h"
#include "df/building_type.h"
#include "df/buildings_other_id.h"
#include "df/item.h"
#include "df/item_type.h"
#include "df/items_other_id.h"
#include "df/unit.h"
#include "df/world.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_setupadventurest.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/viewscreen_choose_start_sitest.h"
#include "df/viewscreen_new_regionst.h"
#include "df/viewscreen_layer_export_play_mapst.h"
#include "df/viewscreen_layer_world_gen_paramst.h"
#include "df/viewscreen_overallstatusst.h"
#include "df/viewscreen_petst.h"
#include "df/viewscreen_movieplayerst.h"
#include "df/viewscreen_layer_militaryst.h"
#include "df/viewscreen_unitlistst.h"
#include "df/viewscreen_buildinglistst.h"
#include "df/viewscreen_layer_unit_relationshipst.h"
#include "df/ui_sidebar_mode.h"
#include "df/ui_advmode.h"
#include "df/vermin.h"
#include "df/creature_raw.h"

#include "renderer_twbt.h"
#include "gui_hooks.hpp"

//#define tweeting
//#define disable_item_color_retrival //normally off
#define oldmouse //normally on
#define omnitransparency
//#define oldunittransparency //exclusive with above option
// using update_map_tile2

#define buildingonlyoverrides
#define unbuiltbuiltinfix
//#define OLDSTUFF //case-sensitive
//#define oldstuff1 //failed
//#define oldstuff2

using namespace DFHack;
using df::global::world;
using std::string;
using std::vector;
using df::global::enabler;
using df::global::gps;
using df::global::ui;
using df::global::init;
using df::global::d_init;
using df::global::gview;
using df::global::cur_year_tick_advmode;

struct texture_fullid {
    unsigned int texpos, bg_texpos, top_texpos, overlay_texpos;
    float r, g, b;
    float br, bg, bb;
};

struct gl_texpos {
    GLfloat left, right, top, bottom;
};

struct tileset {
    int32_t small_texpos[16*16];
    int32_t bg_texpos[16*16];
    int32_t top_texpos[16*16];
};
static vector< struct tileset > tilesets;

enum multi_tile_type
{
    multi_none,
    multi_animation,
    multi_random,
    multi_synchronized
};


struct override {
    int type, subtype, mat_flag;
    vector<int32_t> small_texpos, bg_texpos, top_texpos;
    char bg, fg;
    std::string subtypename;
    t_matpair material;
    std::string material_token;
    multi_tile_type multi = multi_none;
    #ifdef buildingonlyoverrides
    bool building_tile_only = false;
    #endif
    // Because the raws are not available when loading overrides, 
    bool material_matches(int16_t mat_type, int32_t mat_index);
    int32_t get_texpos(vector<int32_t>&collection, unsigned int seed);
    int32_t get_small_texpos(unsigned int seed) { return get_texpos(small_texpos, seed); }
    int32_t get_bg_texpos(unsigned int seed) { return get_texpos(bg_texpos, seed); }
    int32_t get_top_texpos(unsigned int seed) { return get_texpos(top_texpos, seed); }    
};

int coord_hash(int x, int y = 0, int z = 0) {
    int h = x * 374761393 + y * 668265263 + z * 15487313; //all constants are prime
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}



struct override_group {
    int other_id;

    vector< struct override > overrides;
};

struct tile_overrides {
    vector< struct override_group > item_overrides;
    vector< struct override_group > building_overrides;
    vector< struct override > tiletype_overrides;
    bool has_material_overrides = false;
};

static struct tile_overrides *overrides[256];

int32_t *text_texpos, *map_texpos;
int32_t white_texpos, transparent_texpos;

int32_t cursor_small_texpos;

DFHACK_PLUGIN_IS_ENABLED(enabled);
static bool has_textfont, has_overrides;
static color_ostream *out2;
static df::item_flags bad_item_flags;

static bool graphics_enabled;
//static df::item_flags hauled_item_flags;

//static uint32_t vermin_positions = 0; //actually bitflags
    /* Vermin positions as bitflags didn't quite work out - for one, it was tile indices - for 32 tiles, which is not enough for even 1 map block
     * For another, I was trying to store probably more than 32 vermin
     * But it worked on like half the vermin, which is surprisingly well
     * 
     * I might be worth investigating lossy storage of vermin positions in the future to save memory, taking advantage that I also have tile displayed
     * Which means that if there's indication of vermin but not vermin tile, it doesn't display
     * This could make for conflict with whatever other things share tiles with vermin, like partially dug walls for swarm vermin (hardcoded tiles for both)
     * If all vermin have unique tile that is used by nothing else in the tileset, there is no need to check vermin positions at all, which would be even faster
     */



std::string mapshotname = "mapshot.png";


static int well_info_count = 0;

static int maxlevels = 0;
static bool multi_rendered;
static float fogdensity = 0.15f;
static float fogstart = 0;
static float fogstep = 1;
static float fogcolor[4] = { 0.1f, 0.1f, 0.3f, 1 };
static float shadowcolor[4] = { 0, 0, 0, 0.4f };

static int small_map_dispx, small_map_dispy;
static int large_map_dispx, large_map_dispy;

static int tdimx, tdimy;
static int gwindow_x, gwindow_y, gwindow_z;
static int mwindow_x;

static float addcolors[][3] = { {1,0,0} }; //Warn-unused

static int8_t *depth;
static GLfloat *shadowtex;
static GLfloat *shadowvert;
static GLfloat *fogcoord;
static int32_t shadow_texpos[8];
static bool shadowsloaded;
static int gmenu_w;
static uint8_t skytile;
static uint8_t chasmtile;
static bool always_full_update;
static bool hide_stockpiles;
static bool workshop_transparency;
static bool unit_transparency = true;

//TODO: need double buffers?
static uint8_t *gscreen_under, *gscreen_building, *mscreen_under, *mscreen_building;
#ifdef unittracking
static uint8_t *unit_tracker; //bitmap for saving graphical unit locations
#endif

static uint8_t *screen_under_ptr, *screen_building_ptr, *screen_ptr;

// Buffers for map rendering
static uint8_t *_gscreen[2];
static int32_t *_gscreentexpos[2];
static int8_t *_gscreentexpos_addcolor[2];
static uint8_t *_gscreentexpos_grayscale[2];
static uint8_t *_gscreentexpos_cf[2];
static uint8_t *_gscreentexpos_cbr[2];

static uint8_t *gscreen_origin;
static int32_t *gscreentexpos_origin;
static int8_t *gscreentexpos_addcolor_origin;
static uint8_t *gscreentexpos_grayscale_origin, *gscreentexpos_cf_origin, *gscreentexpos_cbr_origin;

// Current buffers
static uint8_t *gscreen;
static int32_t *gscreentexpos;
static int8_t *gscreentexpos_addcolor;
static uint8_t *gscreentexpos_grayscale, *gscreentexpos_cf, *gscreentexpos_cbr;

// Previous buffers to determine changed tiles
static uint8_t *gscreen_old;
static int32_t *gscreentexpos_old;
static int8_t *gscreentexpos_addcolor_old;
static uint8_t *gscreentexpos_grayscale_old, *gscreentexpos_cf_old, *gscreentexpos_cbr_old;

// Buffers for rendering lower levels before merging
static uint8_t *mscreen;
static uint8_t *mscreen_origin;
static int32_t *mscreentexpos;
static int32_t *mscreentexpos_origin;
static int8_t *mscreentexpos_addcolor;
static int8_t *mscreentexpos_addcolor_origin;
static uint8_t *mscreentexpos_grayscale, *mscreentexpos_cf, *mscreentexpos_cbr;
static uint8_t *mscreentexpos_grayscale_origin, *mscreentexpos_cf_origin, *mscreentexpos_cbr_origin;

static int screen_map_type;

	//map storage handling
static df::map_block **my_block_index;
static int block_index_size;
static int mapx;//to reduce accessing df's map struct and for convenience.
static int mapy;
//static int mapz; //not necessary as iterators nor extents use the value.
static int mapxb; //as above, tbh only takes 4 bits maximum.
static int mapyb;
static int mapzb;

//All map pos coords use uint32_t with value of x*mapy*mapzb + y*mapzb + z. This minimizes the number for typical fort in upper-half-centre of embark that is more tiles wide than tall

			//Vermin handling
			
static std::unordered_map<uint32_t, uint8_t> vermin_position_map; //formerly position-race pairs, now position-raw tile pairs
static std::unordered_map<uint16_t, uint8_t> vermin_raw_tile_map; //race-raw tile pairs, used to prevent constant raw lookups
static std::unordered_map<uint32_t, uint16_t> vermin_graphics_map; //now position-textid pairs
static std::unordered_map<uint16_t, uint16_t> vermin_raw_to_graphics_map; //only used for quicker loading
static std::unordered_map<uint32_t, struct texture_fullid> vermin_tile_under_map; //for storing tiles under vermin for vermin transparency in redraw mode
static int32_t last_advmode_tick = 0; //for only polling vermin when map changes, since unlike items they can't change while game is paused.
static std::unordered_map<uint32_t, bool> painted_vermin_map; //used for testing TODO after finish test if it is necessary

			//item handling

static std::unordered_map<uint32_t, int32_t> drawn_item_map; //I assume item ids can't reach 2^31, but not unsigned on the offchance of -1 id
static std::unordered_map<int32_t, uint8_t> item_tile_map; //for matching item id to drawn tile, together with above being enough to display item with or w/o override
union foreback {
	uint32_t totalBits;
	struct {uint8_t fg; uint8_t bg; uint16_t bold;} fgbgparts;
};

			//building handling
			
union buildingbuffer {
	uint32_t totalBits;
	uint8_t tile;
	struct {uint8_t tile; uint8_t fg; uint8_t bg; int8_t bold;} portions;
};
static std::unordered_map<uint32_t, buildingbuffer> buildingbuffer_map; //tilez and building values
static std::unordered_map<uint32_t, int32_t> painted_building_map;
static bool painted_building = false; //for preventing drawing building in _map and _building both in tileupdate_map.hpp
static bool used_map_overlay = false; //for using saved building data only when needed
	//Should really merge _under and regular, but this is to prevent same building being painted twice
	//that has unfortunate effect of applying override of building on floor.

			//unit handling
			
static bool made_units_transparent = false; //since unit transparency is handled in items/buildings, a check to ensure I only do the work once until needed
static std::unordered_map<uint32_t, int32_t> unit_position_map; //position-unitid pairs
static std::unordered_map<int32_t, uint16_t> unit_graphics_map; //unitid-gscreentexpos pairs. Unlike with vermin, direct assocation due no easy way to get ids for GoTG & co

static int once = 24;

static void swap(uint32_t &a, uint32_t &b) noexcept //passes by reference
{
    uint32_t iTemp = a; //a is number
    a = b; //value at address a is set to number 
    b = iTemp;
}

static int lastz=0; //for fully updating when zlevel changes

#include "patches.hpp"





#ifdef tweeting
	#define Infomin(cond,outputstr) \
		if(cond)if(well_info_count<0){well_info_count++; \
			*out2 << outputstr << std::endl; \
		}

	#define Infoplus(cond,outputstr) \
		if(cond)if(well_info_count>0){well_info_count--; \
				*out2 << outputstr << std::endl; \
			}
#else

	#define Infomin(cond,outputstr) {};
	#define Infoplus(cond,outputstr) {};

#endif

#ifdef WIN32aaa
    // On Windows there's no convert_magenta parameter. Arguments are pushed onto stack,
    // except for tex_pos and filename, which go into ecx and edx. Simulating this with __fastcall.
    typedef void (__fastcall *LOAD_MULTI_PDIM)(int32_t *tex_pos, const string &filename, void *tex, int32_t dimx, int32_t dimy, int32_t *disp_x, int32_t *disp_y);

    // On Windows there's no parameter pointing to the map_renderer structure
    typedef void (_stdcall *RENDER_MAP)(int);
    typedef void (_stdcall *RENDER_UPDOWN)();

#else
    typedef void (*LOAD_MULTI_PDIM)(void *tex, const string &filename, int32_t *tex_pos, int32_t dimx, int32_t dimy, bool convert_magenta, int32_t *disp_x, int32_t *disp_y);

    typedef void (*RENDER_MAP)(void*, int);
    typedef void (*RENDER_UPDOWN)(void*);
#endif

LOAD_MULTI_PDIM _load_multi_pdim;
RENDER_MAP _render_map;
RENDER_UPDOWN _render_updown;

#ifdef WIN32aaa
    #define render_map() _render_map(0)
    #define render_updown() _render_updown()
    #define load_tileset(filename,tex_pos,dimx,dimy,disp_x,disp_y) _load_multi_pdim(tex_pos, filename, &enabler->textures, dimx, dimy, disp_x, disp_y)
#else
    #define render_map() _render_map(df::global::map_renderer, 0)
    #define render_updown() _render_updown(df::global::map_renderer)
    #define load_tileset(filename,tex_pos,dimx,dimy,disp_x,disp_y) _load_multi_pdim(&enabler->textures, filename, tex_pos, dimx, dimy, true, disp_x, disp_y)
#endif

static void patch_rendering(bool enable_lower_levels)
{
#ifndef NO_RENDERING_PATCH
    static bool ready = false;
    static unsigned char orig[MAX_PATCH_LEN];

    intptr_t addr = p_render_lower_levels.addr;
    #ifdef WIN32
        addr += Core::getInstance().vinfo->getRebaseDelta();
    #endif

    if (!ready)
    {
        (new MemoryPatcher(Core::getInstance().p))->makeWritable((void*)addr, sizeof(p_render_lower_levels.len));
        memcpy(orig, (void*)addr, p_render_lower_levels.len);
        ready = true;
    }

    if (enable_lower_levels)
        memcpy((void*)addr, orig, p_render_lower_levels.len);
    else
        apply_patch(NULL, p_render_lower_levels);
#endif
}

static void replace_renderer()
{
    if (enabled)
        return;

    MemoryPatcher p(Core::getInstance().p);

    //XXX: This is a crazy work-around for vtable address for df::renderer not being available yet
    //in dfhack for 0.40.xx, which prevents its subclasses form being instantiated. We're overwriting
    //original vtable anyway, so any value will go.
    unsigned char zz[] = { 0xff, 0xff, 0xff, 0xff };
#ifdef WIN32
    //p.write((char*)&df::renderer::_identity + 72, zz, 4);
#else
    p.write((char*)&df::renderer::_identity + 128, zz, 4);
#endif

    renderer_opengl *oldr = (renderer_opengl*)enabler->renderer;
    renderer_cool *newr = new renderer_cool;

    void **vtable_old = ((void ***)oldr)[0];
    void ** volatile vtable_new = ((void ***)newr)[0];

#undef DEFIDX
#define DEFIDX(n) int IDX_##n = vmethod_pointer_to_idx(&renderer_cool::n);

    DEFIDX(draw)
    DEFIDX(update_tile)
    DEFIDX(get_mouse_coords)
    DEFIDX(get_mouse_coords_old)
    DEFIDX(update_tile_old)
    DEFIDX(reshape_gl)
    DEFIDX(reshape_gl_old)
    DEFIDX(zoom)
    DEFIDX(zoom_old)
    DEFIDX(_last_vmethod)

    void *get_mouse_coords_new = vtable_new[IDX_get_mouse_coords];
    void *draw_new             = vtable_new[IDX_draw];
    void *reshape_gl_new       = vtable_new[IDX_reshape_gl];
    void *update_tile_new      = vtable_new[IDX_update_tile];
    void *zoom_new             = vtable_new[IDX_zoom];

    p.verifyAccess(vtable_new, sizeof(void*)*IDX__last_vmethod, true);
    memcpy(vtable_new, vtable_old, sizeof(void*)*IDX__last_vmethod);

    vtable_new[IDX_draw] = draw_new;

    vtable_new[IDX_update_tile] = update_tile_new;
    vtable_new[IDX_update_tile_old] = vtable_old[IDX_update_tile];

    vtable_new[IDX_reshape_gl] = reshape_gl_new;
    vtable_new[IDX_reshape_gl_old] = vtable_old[IDX_reshape_gl];

    vtable_new[IDX_get_mouse_coords] = get_mouse_coords_new;
    vtable_new[IDX_get_mouse_coords_old] = vtable_old[IDX_get_mouse_coords];

    vtable_new[IDX_zoom] = zoom_new;
    vtable_new[IDX_zoom_old] = vtable_old[IDX_zoom];

    memcpy(&newr->screen, &oldr->screen, (char*)&newr->dummy-(char*)&newr->screen);

    newr->reshape_graphics();

    enabler->renderer = (df::renderer*)newr;

    // Disable original renderer::display
    #ifndef NO_DISPLAY_PATCH
        apply_patch(&p, p_display);
    #endif

    // On Windows original map rendering function must be called at least once to initialize something (?)
    #ifndef WIN32
        // Disable dwarfmode map rendering
        apply_patch(&p, p_dwarfmode_render);

        // Disable advmode map rendering
        for (uint j = 0; j < sizeof(p_advmode_render)/sizeof(patchdef); j++)
            apply_patch(&p, p_advmode_render[j]);
    #endif

    twbt_gui_hooks::get_tile_hook.enable();
    twbt_gui_hooks::set_tile_hook.enable();
    twbt_gui_hooks::get_dwarfmode_dims_hook.enable();
    twbt_gui_hooks::get_depth_at_hook.enable();

    enabled = true;
}

static void restore_renderer()
{
    /*if (!enabled)
        return;

    enabled = false;

    gps->force_full_display_count = true;*/
}

static bool advmode_needs_map(int m)
{
    //XXX: these are mostly numerical becuase enumeration seems to be incorrect in dfhack for 0.40.xx

    return (m == df::ui_advmode_menu::Default ||
            m == df::ui_advmode_menu::Look ||
            m == df::ui_advmode_menu::ThrowAim ||
            m == df::ui_advmode_menu::ConversationAddress ||
            m == 14 ||
            m == df::ui_advmode_menu::Fire ||
            m == df::ui_advmode_menu::Jump ||   // (j)
            m == df::ui_advmode_menu::Unk17 ||
            m == 21 ||                          // (S)
            m == 3  || m == 4 ||                // (k)
            m == 20 ||                          // (m)
            m == 16 ||                          // (g)
            m == 37 || m == 38 ||               // (A)
            m == 19 ||                          // companions
            m == 29 ||                          // sleep/wait
            m == 23 ||                          // move carefully/climbing
            m == 33 ||                          // falling/grab screen (?)
            m == 43 ||                          // dodge direction choice
            m == 39 ||                          // target for striking choice
            m == 18 ||                          // combat preferences
            m == 44 || m == 45                  // performance
            );
}

#include "tileupdate_text.hpp"
#include "tileupdate_map.hpp"

	//using gwindow_z for vermin check is not accurate with multilevel view, it is saved value multilevel doesn't modify
#ifdef omnitransparency
static void apply_unitvermin_transparency(int gdimx, int gdimy){
	int xumax = gdimy*std::min(mapx-mwindow_x, gdimx);
	int yumax = std::min(mapy-gwindow_y, gdimy);
	for (int xu = 0; xu < xumax; xu+=gdimy) //Optimization: exclude edges by 1 to gdimx-1?
	for (int yu = 0; yu < yumax; yu++){
		((uint32_t*)screen_under_ptr)[xu+yu] = ((uint32_t*)screen_ptr)[xu+yu];
		((uint32_t*)screen_building_ptr)[xu+yu] = ((uint32_t*)screen_ptr)[xu+yu];
	}
	made_units_transparent = true;
}
#endif

#ifdef oldunittransparency
static void apply_unitvermin_transparency(int gdimx, int gdimy){
	int xumax = gdimy*std::min(mapx-mwindow_x, gdimx);
	int yumax = std::min(mapy-gwindow_y, gdimy);
	for (int xu = 0; xu < xumax; xu++) //Optimization: exclude edges by 1 to gdimx-1?
	for (int yu = 0; yu < yumax; yu++) {//xu/yu is local tile coords, which doesn't fit with vermin/unit, right? those use x/y+respective window
			df::map_block *block = my_block_index[((xu+mwindow_x)>>4)*mapyb*mapzb + ((yu+gwindow_y)>>4)*mapzb + ((maxlevels) ? (*df::global::window_z) : gwindow_z)];
		#ifdef z22x23y23Infomin
		if (22==gwindow_z && well_info_count<0 && 23==(xu+mwindow_x) && 23 ==(yu+gwindow_y)){well_info_count++; \
			*out2 << "In pig area block is " << (block ? "present" : "absent") << " block pos is x " << (uint32_t)block->map_pos.x << " y " << (uint32_t)block->map_pos.y << " z " << (uint32_t)block->map_pos.z << " dx " << (uint32_t)((xu+mwindow_x)&15) << " dy " << (uint32_t)((yu+gwindow_y)&15) << " hasunit " << ((block->occupancy[(xu+mwindow_x)&15][(yu+gwindow_x)&15].bits.unit) ? "present" : "absent")<< std::endl;
		}
		#endif

			if(block)
			{
				int tile = xu*gdimy+yu;
				  //maybe gscreen would be more efficient than checking literally every visible map tile
				  //However current is always 0 at this time, and _old is only nonzero with redraw_all
				  //bool hasunit = gscreentexpos_old[tile] > 0;
				  bool hasunit = block->occupancy[(xu+mwindow_x)&15][(yu+gwindow_x)&15].bits.unit;
				  #ifdef unittracking
				  bool has_tracker = unit_tracker[tile >> 3] & (1 << (tile & 7));
				  #endif
				  //downside is that it only grants transparency for units with graphics, which hits for ex zombies which don't have separate graphics defined
				  bool has_vermin= raw_tile_of_top_vermin(xu+mwindow_x,yu+gwindow_y, maxlevels ? (*df::global::window_z) : gwindow_z) > -1;//needs to be after includes because of this line
				  #ifdef z22x23y23Infomin
				  #ifdef unittracking
				  if (22==gwindow_z && well_info_count<0 && 23==(xu+mwindow_x) && 23 ==(yu+gwindow_y) && has_tracker){well_info_count++; \
					*out2 << "Pig found, x is " << (uint32_t)xu << " y is " << (int32_t)yu << " screen_ptr tile is " << (uint32_t)(((uint32_t*)screen_ptr)[tile]&255) << std::endl;
				}
				#endif
				#endif

				if (
				#ifdef unittracking 
					has_tracker || 
				#endif
				has_vermin){
					#ifdef z22x23y23Infomin
					#ifdef unittracking
					if (22==gwindow_z && well_info_count<0 && 23==(xu+mwindow_x) && 23 ==(yu+gwindow_y)){well_info_count++; \
					*out2 << " hasunit is " << (hasunit ? " present" : " absent") << " has_tracker is " << (has_tracker ? "present" : "absent") << std::endl;}
					#endif
					#endif
					if (!((uint32_t*)screen_under_ptr)[tile]) //these checks are not necessary as they should be always met
						((uint32_t*)screen_under_ptr)[tile] = ((uint32_t*)screen_ptr)[tile]; 
					if (!((uint32_t*)screen_building_ptr)[tile]) 
						((uint32_t*)screen_building_ptr)[tile] = ((uint32_t*)screen_ptr)[tile];
				#ifdef unittracking 
					if(has_tracker)gscreen[tile*4] = 183; //fix for redraw 
				#endif
				}	
			}    
	}
	//memset(unit_tracker, 0, 1+ceil((gdimx*gdimy*sizeof(uint8_t)*0.125)));
	
	made_units_transparent = true;
}
#endif

#include "renderer.hpp"
#include "dwarfmode.hpp"
#include "dungeonmode.hpp"
#include "zoomfix.hpp"
#include "legacy/renderer_legacy.hpp"
#include "legacy/twbt_legacy.hpp"
#include "config.hpp"
#include "buildings.hpp"
#include "items.hpp"
//#include "units.hpp"
#include "commands.hpp"
#include "plugin.hpp"
