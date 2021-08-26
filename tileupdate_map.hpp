#include "df/item_threadst.h"
#include "df/item_constructed.h"
#include "df/itemimprovement.h"
#include "df/itemimprovement_threadst.h"
#include "df/item_corpsepiecest.h"
#include "df/plant_raw.h"
#include "df/plant_growth.h"
#include "df/item_plant_growthst.h"
#include "df/item_plantst.h"
#include "df/plant_growth_print.h"
using df::item_threadst;
using df::item_constructed;
using df::itemimprovement;
using df::itemimprovement_threadst;
using df::item_corpsepiecest;
using df::item_plantst;
using df::plant_raw;
using df::plant_growth;
using df::item_plant_growthst; //yes, plant growth and item_plant_growthst are different and both necessary
using df::plant_growth_print;

#define SETPAIRTEX(tt, texnew) \
    *(texnew++) = txt[tt].left; \
    *(texnew++) = txt[tt].bottom; \
    *(texnew++) = txt[tt].right; \
    *(texnew++) = txt[tt].bottom; \
    *(texnew++) = txt[tt].left; \
    *(texnew++) = txt[tt].top; \
    \
    *(texnew++) = txt[tt].left; \
    *(texnew++) = txt[tt].top; \
    *(texnew++) = txt[tt].right; \
    *(texnew++) = txt[tt].bottom; \
    *(texnew++) = txt[tt].right; \
    *(texnew++) = txt[tt].top;

static void assign_back(struct texture_fullid &ret, const float r, const float g, const float b){
	ret.br=r;
	ret.bg=g;
	ret.bb=b;
}

static void assign_fore(struct texture_fullid &ret, const float r, const float g, const float b){
	ret.r=r;
	ret.g=g;
	ret.b=b;
}

static void resolve_color_descriptor(int nr, struct texture_fullid &ret, const bool set_to_bg)
{
	nr = (nr-100) % df::global::world->raws.language.colors.size();
	df::descriptor_color *fgdc = df::global::world->raws.language.colors[nr];
	if (set_to_bg){
		assign_back(ret, fgdc->red, fgdc->green, fgdc->blue);
	} else {
		assign_fore(ret, fgdc->red, fgdc->green, fgdc->blue);
	}
}

static void resolve_ccolor(int nr, struct texture_fullid &ret, const bool set_to_bg)
{
	if (set_to_bg){
		assign_back(ret, enabler->ccolor[nr][0], enabler->ccolor[nr][1], enabler->ccolor[nr][2]);
	} else {
		assign_fore(ret, enabler->ccolor[nr][0], enabler->ccolor[nr][1], enabler->ccolor[nr][2]);
	}	
}

static void resolve_color_singlet(struct texture_fullid &ret, const bool set_to_bg, int nr, const int bold=0)
{
	if (nr < 0) nr = (uint8_t)nr;
	
    if (nr >= 100){
		resolve_color_descriptor(nr, ret, set_to_bg);
    } else {
        nr = (nr + bold * 8) % 16;
        resolve_ccolor(nr, ret, set_to_bg);
    }
}


static void resolve_color_pair(int fg, int bg, int bold, struct texture_fullid &ret)
{
	resolve_color_singlet(ret, false, fg, bold);
	resolve_color_singlet(ret, true, bg);
}

static void screen_to_texid_map(renderer_cool *r, int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    const int32_t texpos = gscreentexpos[tile]; //if creature graphic is present, texture id nr for that, otherwise 0
    ret.overlay_texpos = transparent_texpos;

    if (!(texpos && graphics_enabled))
    {
        ret.texpos = map_texpos[ch];
        ret.bg_texpos = tilesets[0].bg_texpos[ch];
        ret.top_texpos = tilesets[0].top_texpos[ch];

        resolve_color_pair(fg, bg, bold, ret);
        return;
    }

    ret.texpos = texpos;
    ret.bg_texpos = transparent_texpos; //unit_transparency ? transparent_texpos : white_texpos;
    ret.top_texpos = transparent_texpos;

    if (gscreentexpos_grayscale[tile])
    {
        const unsigned char cf = gscreentexpos_cf[tile];
        const unsigned char cbr = gscreentexpos_cbr[tile];

        resolve_color_pair(cf, cbr, 0, ret);
    }
    else if (gscreentexpos_addcolor[tile])
        resolve_color_pair(fg, bg, bold, ret);
    else
    {
        ret.r = ret.g = ret.b = 1;	//units get full/white multiplier
        ret.br = ret.bg = ret.bb = 0;
    }
}

static void screen_to_texid_map_simple(int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    ret.texpos = map_texpos[ch];
    ret.bg_texpos = tilesets[0].bg_texpos[ch];
    ret.top_texpos = tilesets[0].top_texpos[ch];
    ret.overlay_texpos = transparent_texpos;

    resolve_color_pair(fg, bg, bold, ret);
}


static void screen_to_texid_map_item3(renderer_cool *r, int tile, struct texture_fullid &ret, int item_tile, foreback fgbg)
{
    const unsigned char *s = gscreen + tile*4; //fallback for corpsepieces et al

    int ch   = item_tile;
    int fg   = fgbg.totalBits ? fgbg.fgbgparts.fg : s[1];
    int bg   = fgbg.totalBits ? fgbg.fgbgparts.bg : s[2];
    int bold = fgbg.totalBits ? fgbg.fgbgparts.bold : (s[3] & 0x0f);

	//no checking/setting for overlays, as that was done earlier
	
        ret.texpos = map_texpos[ch];
        ret.bg_texpos = tilesets[0].bg_texpos[ch];
        ret.top_texpos = tilesets[0].top_texpos[ch];

        resolve_color_pair(fg, bg, bold, ret);
}


static void screen_to_texid_map_building3(renderer_cool *r, int tile, struct texture_fullid &ret, buildingbuffer bldbuf)
{
    int ch   = bldbuf.tile;
    int fg   = bldbuf.portions.fg;
    int bg   = bldbuf.portions.bg;
    int bold = bldbuf.portions.bold;
/*
	if (ret.overlay_texpos == transparent_texpos) //didn't set unit or vermin creature graphics into overlay
		{
			const unsigned char *s = gscreen + tile*4;
			if (s[0] != ch) ret.overlay_texpos = map_texpos[s[0]]; //for thirsty dwarves
		}*/
	ret.texpos = map_texpos[ch];
	ret.bg_texpos = tilesets[0].bg_texpos[ch];
	ret.top_texpos = tilesets[0].top_texpos[ch];

	resolve_color_pair(fg, bg, bold, ret);

	painted_building = true; //for using only once per tile
}

static void screen_to_texid_building_stored(int tile, struct texture_fullid &ret, buildingbuffer bldbuf)
{
    int ch   = bldbuf.tile;
    int fg   = bldbuf.portions.fg;
    int bg   = bldbuf.portions.bg;
    int bold = bldbuf.portions.bold;

	ret.overlay_texpos = transparent_texpos;
	ret.texpos = map_texpos[ch];
	ret.bg_texpos = tilesets[0].bg_texpos[ch];
	ret.top_texpos = tilesets[0].top_texpos[ch];

	resolve_color_pair(fg, bg, bold, ret);
}

static void screen_to_texid_map_item(renderer_cool *r, int tile, struct texture_fullid &ret, int item_tile, foreback fgbg)
{
    const unsigned char *s = gscreen + tile*4;

    int ch   = item_tile;
    int fg   = fgbg.totalBits ? fgbg.fgbgparts.fg : s[1];
    int bg   = fgbg.totalBits ? fgbg.fgbgparts.bg : s[2];
    int bold = fgbg.totalBits ? fgbg.fgbgparts.bold : (s[3] & 0x0f);

    const int32_t texpos = gscreentexpos[tile]; //if creature graphic is present, texture id nr for that, otherwise 0
    if (texpos){
		ret.overlay_texpos = texpos;
	}
	else {
		if (graphics_enabled) {ret.overlay_texpos = transparent_texpos;}
		else ret.overlay_texpos = (s[0] != item_tile) ? s[0] : transparent_texpos;
	}
        ret.texpos = map_texpos[ch];
        ret.bg_texpos = tilesets[0].bg_texpos[ch];
        ret.top_texpos = tilesets[0].top_texpos[ch];

        resolve_color_pair(fg, bg, bold, ret);
	if(texpos){
		if (gscreentexpos_grayscale[tile])
		{
			const unsigned char cf = gscreentexpos_cf[tile];
			const unsigned char cbr = gscreentexpos_cbr[tile];

			resolve_color_pair(cf, cbr, 0, ret);
		}
		else if (gscreentexpos_addcolor[tile])
			resolve_color_pair(fg, bg, bold, ret);
	}
}

#ifdef OLDSTUFF
static void screen_to_texid_map_building(renderer_cool *r, int tile, struct texture_fullid &ret, buildingbuffer bldbuf)
{
    int ch   = bldbuf.tile;
    int fg   = bldbuf.portions.fg;
    int bg   = bldbuf.portions.bg;
    int bold = bldbuf.portions.bold;

    const int32_t texpos = gscreentexpos[tile]; //if creature graphic is present, texture id nr for that, otherwise 0
    if (texpos){
		ret.overlay_texpos = texpos;
	}
	else {
		if (graphics_enabled) {ret.overlay_texpos = transparent_texpos;}
		else {
		//	if ( once > 0) {once--;  *out2 << "TWBT: graphics wasnt enabled " << std::endl;}
			const unsigned char *s = gscreen + tile*4;
			ret.overlay_texpos = (s[0] != ch) ? s[0] : transparent_texpos;
			}
	}
        ret.texpos = map_texpos[ch];
        ret.bg_texpos = tilesets[0].bg_texpos[ch];
        ret.top_texpos = tilesets[0].top_texpos[ch];

        resolve_color_pair(fg, bg, bold, ret);
	if(texpos){
		if (gscreentexpos_grayscale[tile])
		{
			const unsigned char cf = gscreentexpos_cf[tile];
			const unsigned char cbr = gscreentexpos_cbr[tile];

			resolve_color_pair(cf, cbr, 0, ret);
		}
		else if (gscreentexpos_addcolor[tile])
			resolve_color_pair(fg, bg, bold, ret);
	}
	
	painted_building = true; //for using only once per tile
	
    //if (false && painted_building && once > 0) {once--;  *out2 << "TWBT painted building was set true" << std::endl;}
}
#endif

static void screen_to_texid_map_old(renderer_cool *r, int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen_old + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    const int32_t texpos = gscreentexpos_old[tile];
    ret.overlay_texpos = transparent_texpos;

    if (!(texpos && graphics_enabled))
    {
        ret.texpos = map_texpos[ch];
        ret.bg_texpos = tilesets[0].bg_texpos[ch];
        ret.top_texpos = tilesets[0].top_texpos[ch];

        resolve_color_pair(fg, bg, bold, ret);
        return;
    }        

    ret.texpos = texpos;
    ret.bg_texpos = transparent_texpos; //white_texpos; //unit_transparency ? transparent_texpos : white_texpos;
    //transparent texpos is necessary for unit transparency, that then works like vermin
    //But there may be downsides with non-transparent buildings like bridges, hatches, 
    //TODO use transparent_texpos + writing to bg for transparent units
    ret.top_texpos = transparent_texpos;

    if (gscreentexpos_grayscale_old[tile])
    {
        const unsigned char cf = gscreentexpos_cf_old[tile];
        const unsigned char cbr = gscreentexpos_cbr_old[tile];

        resolve_color_pair(cf, cbr, 0, ret);
    }
    else if (gscreentexpos_addcolor_old[tile])
        resolve_color_pair(fg, bg, bold, ret);
    else
    {
        ret.r = ret.g = ret.b = 1;
        ret.br = ret.bg = ret.bb = 0;
    }
}

static void screen_to_texid_under(const int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen_under + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    ret.texpos = map_texpos[ch];
    ret.bg_texpos = tilesets[0].bg_texpos[ch];
    ret.top_texpos = tilesets[0].top_texpos[ch];
    ret.overlay_texpos = transparent_texpos;

    resolve_color_pair(fg, bg, bold, ret);
}

static void screen_to_texid_building(int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen_building + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    ret.texpos = map_texpos[ch];
    ret.bg_texpos = tilesets[0].bg_texpos[ch];
    ret.top_texpos = tilesets[0].top_texpos[ch];
    ret.overlay_texpos = transparent_texpos;

    resolve_color_pair(fg, bg, bold, ret);
}

static void apply_override (texture_fullid &ret, override &o, unsigned int seed)
{
    if (o.small_texpos.size())
    {
        switch (o.multi)
        {
        case multi_none:
        default:
            ret.texpos = o.get_small_texpos(seed);
            ret.bg_texpos = o.get_bg_texpos(seed);
            ret.top_texpos = o.get_top_texpos(seed);
            break;
        }
    }

	//if ( once > 0 && o.fg != -1) {once--;  *out2 << "TWBT: override fg was " << (int32_t)o.fg << " in int32 uint8_t " << (int32_t)(uint8_t)o.fg << std::endl;}
    if (o.bg != -1)
		resolve_color_singlet(ret, true, o.bg);
    if (o.fg != -1)
		resolve_color_singlet(ret, false, o.fg);
}

static void apply_override_under (texture_fullid &ret, override &o, unsigned int seed)
{
    if (o.small_texpos.size())
    {
        switch (o.multi)
        {
        case multi_none:
        default:
            ret.texpos = o.get_small_texpos(seed);
            ret.top_texpos = o.get_top_texpos(seed);
            break;
        }
    }

    if (o.fg != -1)
		resolve_color_singlet(ret, false, o.fg);
}

static void set_vermin_positions(){
    vermin_position_map.clear();
    vermin_position_map.reserve(world->vermin.all.size());
    vermin_graphics_map.clear();
    vermin_graphics_map.reserve(world->vermin.all.size()>>2);
    painted_vermin_map.clear();
    painted_vermin_map.reserve(world->vermin.all.size());
    for (auto vermin = world->vermin.all.begin(); vermin != world->vermin.all.end(); vermin++){
		if((*vermin)->visible && !((*vermin)->flags.bits.is_colony))
		{
			df::coord pos = (*vermin)->pos; //need either brackets or auto &temp inbetween step
			//TODO check. Hm, can't I use (**vermin).pos here?
			uint32_t tilez = pos.x*mapy*mapzb+pos.y*mapzb+pos.z;
			if(vermin_raw_tile_map.find((*vermin)->race) == vermin_raw_tile_map.end()){
				auto vermin_raws = world->raws.creatures.all[(*vermin)->race];
				vermin_raw_tile_map[(*vermin)->race] = (uint8_t)vermin_raws->creature_tile;
				vermin_raw_to_graphics_map[(*vermin)->race] = (uint16_t)vermin_raws->graphics.texpos[0];
			}
			vermin_position_map[tilez] = (*vermin)->amount > 9 ? ( (*vermin)->amount>49 ? 177 : 176) : vermin_raw_tile_map[(*vermin)->race];
			vermin_graphics_map[tilez] = vermin_raw_to_graphics_map[(*vermin)->race];
		}
	}/*
    for (int i = 0; i<world->vermin.all.size(); i++){
        if (world->vermin.all[i]->visible && !(world->vermin.all[i]->flags.bits.is_colony) && world->raws.creatures.all[world->vermin.all[i]->race]->graphics.texpos[0]>0){
            //vermin_positions |= 1<<(world->vermin.all[i]->pos.x+world->vermin.all[i]->pos.y*mapx+world->vermin.all[i]->pos.z*mapx*mapy);
            //print_vermin_iteration_data(world->vermin.all[i]->pos.x,world->vermin.all[i]->pos.y,world->vermin.all[i]->pos.z, world->vermin.all[i]->race);
            
            vermin_position_map[world->vermin.all[i]->pos.x+world->vermin.all[i]->pos.y*mapx+world->vermin.all[i]->pos.z*mapx*mapy] = world->vermin.all[i]->race;
            if (vermin_graphics_map.find(world->vermin.all[i]->race) == vermin_graphics_map.end()){ //direct [] out of range is undefined and .at throws an error
				//177 is hardcoded vermin swarm, which overrides creature tile
				 int vermin_amount = world->vermin.all[i]->amount;
                 vermin_raw_tile_map[world->vermin.all[i]->race] = vermin_amount > 9 ? (vermin_amount>49 ? 177 : 176) : (uint8_t)world->raws.creatures.all[world->vermin.all[i]->race]->creature_tile;
                 //lower bound for starting to use swarm tile is 10 for regular vermin
                 //actually seems like there's several stages
                 //1-9 vermin own raw tile
                 //10-49 tile 1 = 176
                 //50+ tile 2 = 177
                 //oh darn. Well I now see why my flies are failing:
                 //it stores one tile per raw, but every species can have 3 different ones, depending on tile
                 //I only use race as a shorthand for access. If I can't use it...might as well set position_map directly to tile, and graphics be position-textid pair
                 vermin_graphics_map[world->vermin.all[i]->race] = (uint16_t)world->raws.creatures.all[world->vermin.all[i]->race]->graphics.texpos[0];
                 
            }
        }
    }*/
    
    /*
    for (auto vrm = world->vermin.all.begin(); vrm != world->vermin.all.end(); vrm++){
        if ( vrm.visible && !(*vrm->flags.bits.is_colony) ){
            vermin_positions |= 1<<(*vrm->pos.x+*vrm->pos.y*mapx+*vrm->pos.z*mapx*mapy);
        }
    }
    */
    /*
    for (auto vrm = world->vermin.all.begin(); vrm != world->vermin.all.end(); vrm++){
        if ( *vrm.visible && !(*vrm.flags.is_colony) ){
            vermin_positions |= 1<<(*vrm.pos.x+*vrm.pos.y*mapx+*vrm.pos.z*mapx*mapy);
        }
    }*/
}

	//TODO Add option to disable vermin?
	//Making it autorefresh on redraw all brought GFPS down to 6 and maxed out core.
static int raw_tile_of_top_vermin(int xv, int yv, int zv){
    if (last_advmode_tick != *df::global::cur_year_tick_advmode){
      //  *out2 << "TWBT assign tick " << *df::global::cur_year_tick_advmode << " pointer value " << last_advmode_tick << " is smaller " << (last_advmode_tick < *df::global::cur_year_tick_advmode) << std::endl; //0
        last_advmode_tick = *df::global::cur_year_tick_advmode;
        set_vermin_positions();
        //*out2 << "TWBT cap hopper race " << (uint16_t)vermin_position_map[43240] << std::endl;
        //*out2 << "TWBT cap hopper tile " << vermin_raw_tile_map[vermin_position_map[43240]] << std::endl;
        //*out2 << "TWBT cap hopper tile in uint16 " << (uint16_t)vermin_raw_tile_map[vermin_position_map[43240]] << std::endl;
        //*out2 << "TWBT cap hopper graphics " << vermin_graphics_map[vermin_position_map[43240]] << std::endl;
        //*out2 << "TWBT zvalue is " << zv << " and sum at 40,36 is " << (40+36*mapx+zv*mapx*mapy) << std::endl;
        }
    if (vermin_position_map.empty())
        return -2; //no vermin in game at all
    const int tilez = xv*mapy*mapzb+yv*mapzb+zv;
    if (vermin_position_map.find(tilez) != vermin_position_map.end())
        return vermin_position_map[tilez]; //finds vermin at position and returns it's tile id
        
    return -1; //didn't find vermin on a tile
}

static int get_vermin_texpos(int xv, int yv, int zv){
	//returns vermin graphics, unless they're absent then uses raw tile
	//assumes one has already confirmed presence of vermin on tile
	const int tilez = xv*mapy*mapzb+yv*mapzb+zv;
	return vermin_graphics_map[tilez] ? : vermin_position_map[tilez]; //if graphics is 0, returns raw tile
	/*
	if (vermin_graphics_map.find(tilez) != vermin_graphics_map.end()) 
		return vermin_graphics_map[tilez];
	return vermin_raw_tile_map[tilez];//else
	*/
}

static void write_values_from_ret(struct texture_fullid &ret, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top, GLfloat *overlay_color, GLfloat *overlay_texpos){
    // Set colour for both buffers
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.r;
        *(fg++) = ret.g;
        *(fg++) = ret.b;
        *(fg++) = 1;
        
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;

        fg_top += 8;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        
        //overlay texture color is white
        
        overlay_color+=8;
        *(overlay_color++) = 1;
        *(overlay_color++) = 1;
        *(overlay_color++) = 1;
        *(overlay_color++) = 1;
    }
    
    // Set texture coordinates
    {
        int32_t texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex);
	}
    // Set bg texture coordinates
    {
        int32_t texpos = ret.bg_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_bg);
    }

    // Set top texture coordinates
    {
        int32_t texpos = ret.top_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_top);
    }    
    
    
    // Set overlay texture coordinates
	{
		int32_t texpos = ret.overlay_texpos;
		gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, overlay_texpos);
	}
}

static void write_values_from_ret_under(struct texture_fullid &ret, GLfloat *fg, GLfloat *tex, GLfloat *fg_top, GLfloat *tex_top){
    // Set colour for both buffers
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.r;// ? ret.r : ret.br; //Doesn't work as dynamic engraving detection
        *(fg++) = ret.g;
        *(fg++) = ret.b;
        *(fg++) = 1;
        
        fg_top += 8;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        /* missing bg results in fail with inverted tiles as terrain i.e. designations, engravings
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;*/
    }
    
    // Set texture coordinates
    {
        int32_t texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex);
    }

    // Set top texture coordinates
    {
        int32_t texpos = ret.top_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_top);
    }    
    
}

static void write_values_from_ret_under_back(struct texture_fullid &ret, GLfloat *fg, GLfloat *tex, GLfloat *fg_top, GLfloat *tex_top){
    // Set colour for both buffers
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.br;
        *(fg++) = ret.bg;
        *(fg++) = ret.bb;
        *(fg++) = 1;
        
        fg_top += 8;
        *(fg_top++) = ret.r;
        *(fg_top++) = ret.g;
        *(fg_top++) = ret.b;
        *(fg_top++) = 1;
        /* missing bg results in fail with inverted tiles as terrain i.e. designations, engravings
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;*/
    }
    
    // Set texture coordinates
    {
        int32_t texpos = white_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex);
    }

    // Set top texture coordinates
    {
        int32_t texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_top);
    }    
    
}

static void write_values_from_ret_under_triplet(struct texture_fullid &ret, GLfloat *fg, GLfloat *tex, GLfloat *bg, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top){
    // Set colour for both buffers
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.r;// ? ret.r : ret.br; //Doesn't work as dynamic engraving detection
        *(fg++) = ret.g;
        *(fg++) = ret.b;
        *(fg++) = 1;
        
        fg_top += 8;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;
    }
    
    // Set texture coordinates
    {
        int32_t texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex);
    }

    // Set bg texture coordinates
    {
        int32_t texpos = ret.bg_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_bg);
    }
    
    // Set top texture coordinates
    {
        int32_t texpos = ret.top_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_top);
    }    
    
}


static void write_values_from_ret_nonwhite_overlay(struct texture_fullid &ret, struct texture_fullid &overlay_ret, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top, GLfloat *overlay_color, GLfloat *overlay_texpos){
    // Set colour for both buffers
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.r;
        *(fg++) = ret.g;
        *(fg++) = ret.b;
        *(fg++) = 1;//alpha?
        
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;

        fg_top += 8;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        
        //overlay texture color is overwritten
        
        overlay_color+=8;
        *(overlay_color++) = overlay_ret.r;
        *(overlay_color++) = overlay_ret.g;
        *(overlay_color++) = overlay_ret.b;
        *(overlay_color++) = 1;
    }
    
    // Set texture coordinates
    {
        int32_t texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex);
    }

    // Set bg texture coordinates
    {
        int32_t texpos = ret.bg_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_bg);
    }

    // Set top texture coordinates
    {
        int32_t texpos = ret.top_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, tex_top);
    }    
    
    
    // Set overlay texture coordinates
	{
		int32_t texpos = ret.overlay_texpos;
		gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(texpos, overlay_texpos);
	}
}


/*
 * 	//needs all buildings having over 1 or sth
static int has_transparent_building_at(int x, int y, int z){
		uint32_t tilez = x*mapyb*mapzb + y*mapzb + gwindow_z;
		if (painted_building_map.find(tilez) != painted_building_map.end()) 
			return painted_building_map[tilez];
		return false;
}
*/

/*
static bool isInvertedTile(df::item item){
	switch (item->getType()){
		case df::item_type::TOOL: //anon_vmethod28/hasInvertedTile vmethod identified in 47.04-beta1
				if (auto toolitem = virtual_cast<item_toolst>(item)) {
					return toolitem.subtype.flags.INVERTED_TILE;
				} else return false;
			break;
		default:
			return false; //nothing outside tools is inverted
			break;
	}
}
*/

static bool hasDualColors(const int itemtype){
	switch (itemtype){
		case df::item_type::BARREL:
			return true;
			break;
		case df::item_type::HATCH_COVER:
			return true;
			break;
		case df::item_type::DOOR: 
			return true;
			break;
		case df::item_type::CAGE:
			return true;
			break;
		case df::item_type::BIN:
			return true;
			break;
		case df::item_type::FLOODGATE:
			return true;
			break;
		case df::item_type::TOOL: //seems so, based on fire clay bookcase vs fire clay hatch cover. However, note that inverted tile tools never use bright color
			return false;
			break;
		default:
			return false;
			break;
	}
}

	//functions instead of unrolling for ease of debugging, but currently using a macro below instead of function
#ifdef get_item_colors_split
static void getDyedPair( uint8_t & fg, uint8_t & bg, df::item * item){
	uint16_t mat_type = 0; //initialization in case both casts fail
	uint32_t mat_index = 0;
	if(auto threaditem = virtual_cast<item_threadst>(item)){
		//there's static_cast<item_threadst>(item), maybe
			mat_type = threaditem->dye_mat_type;
			mat_index = threaditem->dye_mat_index;
		} else {
			auto impitem = virtual_cast<item_constructed>(item);
			for (auto improvement = impitem->improvements.begin(); improvement != impitem->improvements.end(); improvement++)
			{
				if (auto threadimp = virtual_cast<itemimprovement_threadst>(*improvement)) {
					//void *holdconvert2 = *improvement;
					//itemimprovement_threadst *threadimp = (itemimprovement_threadst*)holdconvert2;
					mat_type = (threadimp)->dye.mat_type;
					mat_index = (threadimp)->dye.mat_index;
					}
					
			}
		}
		Infomin(mat_type==0 && mat_index ==0, " Found dyed item with mat type and index not set, id " << item->id);
		MaterialInfo dye_info(mat_type, mat_index);
		fg = 100+dye_info.material->state_color[3];
		bg = dye_info.material->build_color[0];
}
#endif

#define getDyedPair(fg,bg,item) \
	if(auto threaditem = virtual_cast<item_threadst>(item)){ \
		/* there's static_cast<item_threadst>(item), maybe */ \
			mat_type = threaditem->dye_mat_type; \
			mat_index = threaditem->dye_mat_index; \
		} else { \
			auto impitem = virtual_cast<item_constructed>(item); \
			for (auto improvement = impitem->improvements.begin(); improvement != impitem->improvements.end(); improvement++) \
			{ \
				if (auto threadimp = virtual_cast<itemimprovement_threadst>(*improvement)) { \
					/* void *holdconvert2 = *improvement;
					// itemimprovement_threadst *threadimp = (itemimprovement_threadst*)holdconvert2; */  \
					mat_type = (threadimp)->dye.mat_type; \
					mat_index = (threadimp)->dye.mat_index; \
					} \
			} \
		} \
		Infomin(mat_type==0 && mat_index ==0, " Found dyed item with mat type and index not set, id " << item->id); \
		MaterialInfo dye_info(mat_type, mat_index); \
		fg = 100+dye_info.material->state_color[3]; \
		bg = dye_info.material->build_color[0];


static bool useSimpleColors(df::item * item){
		switch(item->getType()){
			case df::item_type::CORPSEPIECE: 
				//Err: reports white/green correctly for some stuff, erronously for other stuff (i.e. yak skin)
				//Err2: prepared yak brain is lightgray, horn is dark gray, basic color is 7:0 for both, same build color too
				//the proper values are stored in .appearance.color for ...skin
				//The same 5/2/0 is stored for dark gray horn too....Or the yak in general as whole
				//unit.genes.colors does give me something: 5 8 6 2 0 0 ....None of these are 7:0, though
				if(auto cpitem = virtual_cast<item_corpsepiecest>(item)) {
					if (cpitem->corpse_flags.bits.shell) //only override shell colors for now until I figure out the mess with other ones
						return true;
					break;
				} else break;
			case df::item_type::EGG:
			case df::item_type::FISH_RAW:
			case df::item_type::FISH:
					return true;
				break;
			default:
				break;
		}
	return false;
}

#ifdef get_item_colors_split
static void setSimpleColors(uint8_t & item_fore, uint8_t & item_back, uint16_t & item_bold, MaterialInfo & minfo, df::item * item){
		if(minfo.material){
			if (hasDualColors(item->getType()) ){
				item_fore = (uint8_t)(minfo.material->build_color[0]);
				item_back = (uint8_t)(minfo.material->build_color[1]);
				item_bold = (uint16_t)(minfo.material->build_color[2]);
			} else if( item->hasInvertedTile() ){	//nothing is inverted and dual
				item_fore = (uint8_t)(minfo.material->basic_color[1]);
			//	item_back = 0; //simple tiles never have bg
			//	item_bold = 0; //inverted tiles are never bright
			} else {
				item_fore = (uint8_t)(minfo.material->basic_color[0]);
			//	item_back = 0; //simple tiles never have bg
				item_bold = (uint16_t)(minfo.material->basic_color[1]);
			}
	  }
}
#endif

#define setSimpleColors(fg,bg,bold,minfo,sitem) \
		if(minfo.material){ \
			if (hasDualColors(sitem->getType()) ){ \
				fg = (uint8_t)(minfo.material->build_color[0]); \
				bg = (uint8_t)(minfo.material->build_color[1]); \
				bold = (uint16_t)(minfo.material->build_color[2]); \
			} else if( sitem->hasInvertedTile() ){	/* nothing is inverted and dual */ \
				fg = (uint8_t)(minfo.material->basic_color[1]); \
			/*	item_back = 0; //simple tiles never have bg
				item_bold = 0; //inverted tiles are never bright */ \
			} else { \
				fg = (uint8_t)(minfo.material->basic_color[0]); \
			/* item_back = 0; //simple tiles never have bg */ \
				bold = (uint16_t)(minfo.material->basic_color[1]); \
			} \
	  }


static bool usingComplicatedColors(uint8_t & item_fore, uint8_t & item_back, uint16_t & item_bold, df::item * item, MaterialInfo & mat_infoPair){
	switch(item->getType()){
		case df::item_type::REMAINS:
			item_fore = 5;
			return true;
			break;
		case df::item_type::PLANT: //surprisingly, does use dual colors, i.e. orange item with green border for carrots.
			{
			if(mat_infoPair.plant) if(auto plantItem = virtual_cast<item_plantst>(item)) {
				bool isRotten = plantItem->flags.bits.rotten;
				auto pcolors = mat_infoPair.plant->colors;
				item_fore = isRotten ? (uint8_t)(pcolors.dead_picked_color[0]) : (uint8_t)(pcolors.picked_color[0]);
				item_back = isRotten ? (uint8_t)(pcolors.dead_picked_color[1]) : (uint8_t)(pcolors.picked_color[1]);
				item_bold = isRotten ? (uint16_t)(pcolors.dead_picked_color[2]) : (uint16_t)(pcolors.picked_color[2]);
			}
			}
			return true;
			break; 
		case df::item_type::PLANT_GROWTH:
			{
			//plant_growth_anon_1 is which print, though triggers withered look when out of range
			if(mat_infoPair.plant) if (auto growthItem = virtual_cast<item_plant_growthst>(item)){
				if(auto growths = mat_infoPair.plant->growths[growthItem->subtype]->prints[growthItem->anon_1 < 0 ? 0 : growthItem->anon_1]) {
				item_fore = (uint8_t)(growths->color[0]);
				item_back = (uint8_t)(growths->color[1]);
				item_bold = (uint16_t)(growths->color[2]);
				}
			}
			}
			return true;
			break;
		case df::item_type::SEEDS:
			{
				if(mat_infoPair.plant){
					auto scolor = (mat_infoPair.plant->colors).seed_color;
					item_fore = (uint8_t)(scolor[0]);
					item_back = (uint8_t)(scolor[1]);
					item_bold = (uint16_t)(scolor[2]);
				}
			}
			return true;
			break;
		default:
			break;
	}
	
	return false;
}


static foreback get_item_color(df::item * item){//We don't modify item, but materialInfo cannot accept const item
	//TODO partially: Only use this function when necessary (unit/vermin/mist?/multiple items on tile)
	//DONE prefer main color in fact for buildings too, for vomit and such
	//TODO use tuple instead of foreback
	uint8_t item_fore=0;
	uint8_t item_back=0;
	uint16_t item_bold=0;
	int16_t mat_type = item->getMaterial(); //While these do not change, matinfo demands non-constant pair
	int32_t mat_index = item->getMaterialIndex();
	
	if (item->isDyed()){
		getDyedPair(item_fore, item_back, item);
	} else {
		if (useSimpleColors(item)){
			MaterialInfo mat_infoItem(item); //The critical difference from the below option which uses mat type and index pair gotten from item instead.
			setSimpleColors(item_fore, item_back, item_bold, mat_infoItem, item);
		}
		else
		{
		MaterialInfo mat_infoPair(mat_type, mat_index);
		//checkSpecificColorsBeforeUsingSimple(item_fore, item_back, item_bold, item, mat_infoPair);
		if(!usingComplicatedColors(item_fore, item_back, item_bold, item, mat_infoPair))
			setSimpleColors(item_fore, item_back, item_bold, mat_infoPair, item);
		}
	}

	foreback retvalue;
	retvalue.fgbgparts.fg=item_fore;
	retvalue.fgbgparts.bg=item_back;
	retvalue.fgbgparts.bold=item_bold;
	
    			//if (once>0) {once--;
				//*out2 << "TWBT color values fore " << (int)item_fore << " back " << (int)item_back << " item id " << (int)item->id << std::endl;} 
				//after recompiling dfhack to set anon28 to hasInvertedTile, I must conclude it was utterly useless: only thing that flags it is bookcase
	return retvalue;
}

static foreback get_item_color(int32_t itemId){
	#ifdef disable_item_color_retrival
	foreback nothing;
	nothing.totalBits = 0;
	return nothing;
	#else
	return get_item_color(df::item::find(itemId));
	#endif
}

static void write_tile_arrays_map3(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top, GLfloat *overlay_color, GLfloat *overlay_texpos)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;
        const unsigned char *s = gscreen + tile*4;
        int s0 = s[0];
    const int xx = gwindow_x + x;
    const int yy = gwindow_y + y;
      if (s0 == 88 && df::global::cursor->x == xx && df::global::cursor->y == yy)//since I'm going to check cursor anyway, might as well get it out of the way early
        {
		   screen_to_texid_map(r, tile, ret); 
           int32_t texpos = cursor_small_texpos;
           if (texpos) ret.texpos = texpos;
		   write_values_from_ret(ret, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
           return;
        }
       const unsigned char *so = gscreen_old + tile*4;
	    int   so0 = so[0];
    int zz = gwindow_z - ((s[3]&0xf0)>>4);
    int tilez = xx*mapy*mapzb+yy*mapzb+zz;
    
    bool has_vermin = false;
		bool has_vermin_tile = false;
	bool has_item = false;
	   int item_id =0;
	bool has_building = false;
	bool is_wet = false;
	 //  int building_id = 0;
		ret.overlay_texpos = transparent_texpos; //default; to be replaced in case of unit/vermin
	bool is_hidden=false; //default assuming revealed
		bool skiphidden = false;
	if (my_block_index && (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy) ){
		df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];
        if(block){
			is_wet = (block->designation[xx&15][yy&15].bits.flow_size) > 0 ? true : false;
			is_hidden = block->designation[xx&15][yy&15].bits.hidden;
			if(is_hidden){
				screen_to_texid_map(r, tile, ret); 
				skiphidden = true;
				//goto map2hidden;//skip over all the draw vermin/unit stuff; ERR: must not skip initialization and assignment of variables by any means
			}
		} else 
			{Infoplus(true, "TWBT failed to find block at x " << xx << " y " << yy << " z " << zz);}
	} else { //probaby out of bounds, skip in-bounds things
		screen_to_texid_map(r, tile, ret); 
		skiphidden = true;
		//goto map2hidden;
	}
	if (!skiphidden){
	   if (drawn_item_map.find(tilez) != drawn_item_map.end()){
		   has_item = true;
		   item_id = drawn_item_map.at(tilez);
		   Infomin(item_id==0, "TWBT found zero id item at x " << xx << " y " << yy << " z " << zz);
		} else if (painted_building_map.find(tilez) != painted_building_map.end())if (buildingbuffer_map.find(tilez)!=buildingbuffer_map.end()){
		   has_building = true;
	//	   building_id = painted_building_map[tilez];
		}
		has_vermin = raw_tile_of_top_vermin(xx,yy,zz)==s0;//there's a vermin visible
		has_vermin_tile=raw_tile_of_top_vermin(xx,yy,zz)>-1; //using this, I could display both items/buildings and vermins regardless of which is visible.
		
		if (gscreentexpos[tile]) ret.overlay_texpos = gscreentexpos[tile]; //set unit to overlay
		else if (has_vermin){
			ret.overlay_texpos = get_vermin_texpos(xx,yy,zz);
			painted_vermin_map[tilez] = true;
		} else if( (has_item && (s0 != item_tile_map.at(item_id))) || (has_building && ( s0 != buildingbuffer_map[tilez].portions.tile)) ){//TODO use only status icon codes
			if(s[0] != (gscreen_under+tile*4)[0] ){
			df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];
			if(block) if (block->occupancy[xx&15][yy&15].bits.unit) {
					ret.overlay_texpos = map_texpos[s[0]]; //for thirsty dwarves & Night creatures that don't have graphics defined
				}
			}
		}
		if (ret.overlay_texpos != transparent_texpos) { //default values will not do, possible below: item/building/nothing
			used_map_overlay = true;
			if(has_item)
				screen_to_texid_map_item3(r, tile, ret, item_tile_map.at(item_id), get_item_color(item_id));
				//TODO test goto to vermin fallback if foreback is empty
			else if(has_building){	//TODO: check fluid in block and change/apply color as appropriate
				if (is_wet) { //adjusting building color with liquid color, as to ensure submerged buildings are blue/red
					const unsigned char *u = gscreen_under + tile*4;
					buildingbuffer_map[tilez].portions.fg = u[1];
					buildingbuffer_map[tilez].portions.bg = u[2];
					buildingbuffer_map[tilez].portions.bold = 0;// u[3] & 0x0f; //otherwise tile flashes brighter when vermin is visible
				}
				screen_to_texid_map_building3(r, tile, ret, buildingbuffer_map[tilez]);
				}
			else {	//no item or building; therefore should just keep overlay
				if(gscreentexpos[tile]) screen_to_texid_map(r, tile, ret); //conveniently adds grayscale values to unit, might remove later for consistency
				else 
				{ //vermin; 
				ret.texpos = transparent_texpos;
				ret.bg_texpos = transparent_texpos;
				ret.top_texpos = transparent_texpos;
				goto matchedm; //necessary because s0 isn't replaced, which means overrides still get applied if vermin tile nr matches terrain/bld
				}
			}
		} else { //there wasn't unit or vermin present, which means I can use more accurate straight values than stored values
			if(has_item && item_tile_map.at(item_id) == s0 && ((df::item::find(item_id)))->isDyed())
				//Check is to not apply in case of flows, though getting color is necessary due dyes
				screen_to_texid_map_item3(r, tile, ret, item_tile_map.at(item_id), get_item_color(item_id));
			else{
				screen_to_texid_map_simple(tile, ret);
				if(has_building) painted_building = true;
			}
		}
		
		if(has_vermin_tile && ret.overlay_texpos == transparent_texpos){ //only apply vermin graphics on unitless tile for top layer
		   if(!made_units_transparent && !has_item && !has_building && has_vermin){//only apply to vermin that are visible and not sharing tile with something transparent
			   //no items or buildings in visible play area or transparency is off got to use fallback option of old tile
			   painted_vermin_map[tilez] = true;
			   if (s0 != so0){
					zz = gwindow_z - ((so[3]&0xf0)>>4);
					tilez = xx*mapy*mapzb+yy*mapzb+zz;
					screen_to_texid_map_old(r,tile, vermin_tile_under_map[tilez]);
					s0 = so[0];
					ret = vermin_tile_under_map[tilez];
			   }
			   else
			   {
				 if (vermin_tile_under_map.find(tilez) != vermin_tile_under_map.end()){
				   ret = vermin_tile_under_map[tilez];
				 }
			   }
		   }
	   }
     }
			//map2hidden:;
    if (has_overrides && my_block_index)
    {
		if(has_item && ((s0 == item_tile_map.at(item_id))||(ret.overlay_texpos != transparent_texpos))) s0 = item_tile_map.at(item_id);//for units/vermins
		else if(ret.overlay_texpos != transparent_texpos){//use fallback tiles only if necessary to preserve mist-types
			if(has_building) s0 = buildingbuffer_map[tilez].tile;
			//this also means that it reads ice floor in workshop as ice floor tile, not 32 (empty) tile
		}

        if ((overrides[s0]))
			//check for overrides if there are any for the tile, unless I have a vermin using old tile
        {
			
            if (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy)
            {
                {
                    {
                    tile_overrides *to = overrides[s0];

                    // Items now also needs block for occupancy check
                    df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];//[xx>>4][yy>>4][zz];
                    //TODO place block+hidden check togeter for items+buildings
                  if (has_item) {
                  if (block) //tbh I don't think it's possible for there to be an item but not block
                  {
                    for (auto it = to->item_overrides.begin(); it != to->item_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->items.other[og.other_id];
                        for (auto it2 = ilist.rbegin(); it2 != ilist.rend(); it2++)
                        {
                            df::item *item = *it2;
                            if(item->id != item_id) //2021 unnecessary; I've already set item_id. Also, .at vs []?
								continue;

                            MaterialInfo mat_info(item->getMaterial(), item->getMaterialIndex());
							

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && item->getType() != o.type)
                                    continue;
                                if (o.subtype != -1 && item->getSubtype() != o.subtype)
                                    continue;

                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }

                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;

                                apply_override(ret, o, item->id);
                                goto matchedm;
                            }
                        }
                    }
                  }
			  } else {
                    // Buildings
                  if(!is_hidden) { //TODO add check: only apply bld override if it matches bld tile
									//Hang on, that check would mean ice floors would stop being overridden...
									//But it'd also mean that "retracted" spike overrides would stop working
									//argh
									//TODO are all blds covered? depot, wagon, well which use items.hpp? 

                    for (auto it = to->building_overrides.begin(); it != to->building_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->buildings.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::building *bld = *it2;
                            
                            //if (bld->id !=has_transparent_building_at(tilez)) //on hold unless I ensure all buildings have over1 set
							//	continue;
							
                            if (not (has_building && (painted_building_map[tilez] == bld->id))) //skip the xyz check (for wells) if building matches
								if (zz != bld->z || xx < bld->x1 || xx > bld->x2 || yy < bld->y1 || yy > bld->y2)
									continue;
                                
                            MaterialInfo mat_info(bld->mat_type, 
                            #ifdef unbuiltbuiltinfix 
                            (bld->mat_type < 19 && bld->mat_type > 0 ) ? -1 : 
                            #endif
                            bld->mat_index);
								//ternary operator fix for yet built glass bridges which take mat type from glass but mat index from last material
								//Only really happens with stuff like not yet built bridges and roads, but easier to check mat type than building type then mat type.

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && bld->getType() != o.type)
                                    continue;
                                
                                if (o.subtype != -1)
                                {
                                    int subtype = (og.other_id == buildings_other_id::WORKSHOP_CUSTOM || og.other_id == buildings_other_id::FURNACE_CUSTOM) ?
                                        bld->getCustomType() : bld->getSubtype();

                                    if (subtype != o.subtype)
                                        continue;
                                }
                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }
                                
                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;
                                #ifdef buildingonlyoverrides                                    
                                Infoplus((x+gwindow_x+1)==df::global::cursor->x && (y+gwindow_y)==df::global::cursor->y && o.building_tile_only, "Checking building tile only override for ch " << (int32_t)s0 << "buildingbuffer is " << ((buildingbuffer_map.find(tilez)!=buildingbuffer_map.end()) ? "present" : "absent") );
                                if (o.building_tile_only && !(buildingbuffer_map.find(tilez)!=buildingbuffer_map.end() && buildingbuffer_map[tilez].portions.tile == s0))
									continue;
								#endif
                                    
                                apply_override(ret, o, bld->id);
                                #ifdef buildingonlyoverrides
                                Infoplus((x+gwindow_x+1)==df::global::cursor->x && (y+gwindow_y)==df::global::cursor->y && o.building_tile_only, "Applied building tile only override");
                                #endif
                                painted_building = true; //TODO is every circumstance better than using map only for painted_building check better?
                                
                                //Hm ice floors in workshops would happen regardless; tile is X and there's override for it;
                                //if I pull tile 32 from pbmap that is solved but at the same time mist-types get broken.
                                goto matchedm;
                            }
                        }
                    }
                  }
                    // Tile types //block declaration was moved earlier
                    //df::map_block *block = my_block_index[(xx>>4)*world->map.y_count_block*world->map.z_count_block + (yy>>4)*world->map.z_count_block + zz];//[xx>>4][yy>>4][zz];
                    if (block)
                    {
                        int tiletype = block->tiletype[xx&15][yy&15];
                        bool fluid_tile = (s0>48) && (s0<56) && ((block->designation[xx&15][yy&15].bits.flow_size)==(s0-48)) && (!is_hidden);
                        //Tile number matches unhidden fluid flow

                        df::tiletype tt = (df::tiletype)tiletype;

                        t_matpair mat(-1,-1);

                        if (to->has_material_overrides && Maps::IsValid())
                        {
                            if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                            {
                                //material is ice/water.
                                mat.mat_index = 6;
                                mat.mat_type = -1;
                            }
                            else
                            {
                                if (!r->map_cache)
                                    r->map_cache = new MapExtras::MapCache(); //Can't alter pointer type to unique/shared due r being renderer_cool
                                mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                            }
                        }
                        MaterialInfo mat_info(mat);
                        
                        for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                        {
                            override &o = *it3;

                            if (tiletype != o.type)
                                continue;

                            if (o.mat_flag != -1)
                            {
                                if (!mat_info.material)
                                    continue;
                                if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                    continue;
                            }

                            if (!(o.material_matches(mat_info.type, mat_info.index) || (fluid_tile && o.material_matches(6, -1)))) //also matches magma for now
                                continue;
                                
                            if (is_hidden && (s0 == 219)) //hidden digging designation isn't overriden
                                continue;
                            
                            if (is_hidden && (s0>48) && (s0<56) && tiletype == 32) //hidden openspace 1-7 isn't overriden
                                continue;
                                
                            apply_override(ret, o, coord_hash(xx,yy,zz));
                            goto matchedm;
                        }
                        
                    }
				  }//has_item else end
                }}
            }
        }
    }
    matchedm:;
              if (!is_hidden && has_vermin && !made_units_transparent && !has_item && !has_building){ 
				  //handling the case where there are no items or buildings visible at all
					if (s[0] != so0){
						ret.overlay_texpos=get_vermin_texpos(xx,yy,zz);
						vermin_tile_under_map[tilez]=ret;
					}
				}
			  //TODO decide
			  if (!has_vermin && ret.overlay_texpos != transparent_texpos){ //there is unit texpos visible; either a status icon or actual unit
				  if(gscreentexpos[tile]) {//actual unit, potentially with addcolor or in grayscale
					  struct texture_fullid unithold;
					  screen_to_texid_map(r, tile, unithold);
					  write_values_from_ret_nonwhite_overlay(ret, unithold, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
				  } else { //status icon on building
					  struct texture_fullid unithold;
					  screen_to_texid_map_simple(tile, unithold);
					  write_values_from_ret_nonwhite_overlay(ret, unithold, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
				  }
				  return;
				}
			  
			  write_values_from_ret(ret, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
}

#ifdef OLDSTUFF
static void write_tile_arrays_map2(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top, GLfloat *overlay_color, GLfloat *overlay_texpos)
{//note to self: to skip layer, just set all four texpos of ret to transparent
	if(false && !made_units_transparent)
        *out2 << "TWBT hasn't made units transparent" << std::endl;
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;
        const unsigned char *s = gscreen + tile*4;
        int s0 = s[0];
    const int xx = gwindow_x + x;
    const int yy = gwindow_y + y;
      if (s0 == 88 && df::global::cursor->x == xx && df::global::cursor->y == yy)//since I'm going to check cursor anyway, might as well get it out of the way early
        {
		   screen_to_texid_map(r, tile, ret); 
           int32_t texpos = cursor_small_texpos;
           if (texpos) ret.texpos = texpos;
		   write_values_from_ret(ret, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
           return;
        }
       const unsigned char *so = gscreen_old + tile*4;
	    int   so0 = so[0];
    int zz = gwindow_z - ((s[3]&0xf0)>>4);
    int tilez = xx*mapy*mapzb+yy*mapzb+zz;
    bool has_vermin = false;
    //int vermin_pos;
    //    const unsigned char *s = gscreen_under + tile*4;
    //has_vermin = (raw_tile_of_top_vermin(gwindow_x + x,gwindow_y + y,gwindow_z - ((s[3]&0xf0)>>4))==s[0]);
	
	   bool has_item = false;
	   int item_id =0;
	   bool has_building = false;
	 //  int building_id = 0;
	bool is_hidden=false; //default assuming revealed
		bool isnt_main_vermin_tile = true; //must be before goto jump
		bool has_vermin_tile = false;
		bool skiphidden = false;
	if (my_block_index && (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy) ){
		df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];
        if(block){
			is_hidden = block->designation[xx&15][yy&15].bits.hidden;
			if(is_hidden){
				screen_to_texid_map(r, tile, ret); 
				skiphidden = true;
				//goto map2hidden;//skip over all the draw vermin/unit stuff; ERR: must not skip initialization and assignment of variables by any means
			}
		} else 
			*out2 << "TWBT failed to find block at x " << xx << " y " << yy << " z " << zz << std::endl;
	} else { //probaby out of bounds, skip in-bounds things
		screen_to_texid_map(r, tile, ret); 
		skiphidden = true;
		//goto map2hidden;
	}
	if (!skiphidden){
	   if (drawn_item_map.find(tilez) != drawn_item_map.end()){
		   has_item = true;
		   item_id = drawn_item_map[tilez];//ok this can be 0 in Premigrantssummer. WTF TODO FIX
		   if (item_id == 0) *out2 << "TWBT found zero id item at x " << xx << " y " << yy << " z " << zz << std::endl;
		} else if (painted_building_map.find(tilez) != painted_building_map.end()){
		   has_building = true;
	//	   building_id = painted_building_map[tilez];
		}
		//foreback nothing;
		//nothing.totalBits=0;
		//foreback discard = get_item_color(item_id); //goto err: crosses initialization
		if(has_item)
			screen_to_texid_map_item(r, tile, ret, item_tile_map[item_id], get_item_color(item_id));
		else if(has_building)
			screen_to_texid_map_building(r, tile, ret, buildingbuffer_map[tilez]);
		else 
			screen_to_texid_map(r, tile, ret); 
			//the above part works even with unit transparency off
	   has_vermin = raw_tile_of_top_vermin(xx,yy,zz)==s0;//there's a vermin visible
		has_vermin_tile=raw_tile_of_top_vermin(xx,yy,zz)>-1; //using this, I could display both items/buildings and vermins regardless of which is visible.
		//if I do that, however, small items below large vermin will become invisible
		//What I'd want is to handle vermin in 2nd layer if there is item or creature on tile
		//unless it is vermin/terrain, and unit transparency is on or off
		
		//So! There is vermin and either terrain or building
		//Nb: Possibility of painting vermin here might mean needing to check for unit/item so I don't paint vermin second time in under
		//Though that isn't exactly bad thing
		if(has_vermin_tile && ret.overlay_texpos == transparent_texpos){ //only apply vermin graphics on unitless tile for top layer
		   if(!made_units_transparent && !has_item && !has_building&& has_vermin){
			   //no items or buildings in visible play area or transparency is off got to use fallback option of old tile
			   if (zz < 6) *out2 << "TWBT nontransparent vermin handling at " << xx << " y " << yy << " z " << zz << std::endl;
			   if (s0 != so0){
					zz = gwindow_z - ((so[3]&0xf0)>>4);
					tilez = xx*mapy*mapzb+yy*mapzb+zz;
					screen_to_texid_map_old(r,tile, vermin_tile_under_map[tilez]);
					s0 = so[0];
					ret = vermin_tile_under_map[tilez];
			   }
			   else
			   {
				isnt_main_vermin_tile = false;
				 if (vermin_tile_under_map.find(tilez) != vermin_tile_under_map.end()){
				   ret = vermin_tile_under_map[tilez];
				 }
			   }
		   }
		   else
		   {//Use main texpos for vermin if it visible, and overlay if invisible
			   if(has_vermin){
				isnt_main_vermin_tile = false;
				if(has_item || has_building)
					ret.overlay_texpos = get_vermin_texpos(xx,yy,zz);
				else{
					ret.overlay_texpos = get_vermin_texpos(xx,yy,zz);
					ret.texpos = transparent_texpos;
					ret.bg_texpos = transparent_texpos;
					ret.top_texpos = transparent_texpos;
					}
				}
				else ret.overlay_texpos = get_vermin_texpos(xx,yy,zz);//also mess with bg and top texpos?
				
				painted_vermin_map[tilez] =true;
		   }
	   }
     }
			//map2hidden:;
    if (has_overrides && my_block_index)
    {
		if(has_item) s0 = item_tile_map[item_id];
		else if(has_building) s0 = buildingbuffer_map[tilez].tile;
        if ((overrides[s0]))
			//check for overrides if there are any for the tile, unless I have a vermin using old tile
        {
			
            if (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy)
            {
                {
                    {
                    tile_overrides *to = overrides[s0];

                    // Items now also needs block for occupancy check
                    df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];//[xx>>4][yy>>4][zz];
                    //hasunit doesn't need to be inside for loop for items, even if only used there
                    //bool hasunit = false;
                    //TODO place block+hidden check togeter for items+buildings
                  if (has_item) {
                  if (block) //tbh I don't think it's possible for there to be an item but not block
                  {
                   //hasunit = block->occupancy[xx&15][yy&15].bits.unit;
                   if (true) //has_item check already done
                   //can't use block has items check since they're not tracked for floating minecarts
                   {
                    for (auto it = to->item_overrides.begin(); it != to->item_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->items.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::item *item = *it2;
                            if(item->id != drawn_item_map.at(tilez))
								continue;
						//	if(gscreentexpos[tile]) //has creature, no longer necessary
						//		continue;
								/*
                            df::coord apos = Items::getPosition(item);
                            if (!(zz == apos.z && xx == apos.x && yy == apos.y))
                                continue;
                            
                            if ((item->flags.whole & bad_item_flags.whole)||(hasunit && (s0 == 1 || s0 == 2 || (s0>63 && s0<90) || (s0>96 && s0<124) || s0 ==125 || s0 == 142 || s0 == 143 || s0 == 149)))//&& item->flags.bits.dead_dwarf //needs individual tiles instead because of carried items
                                continue; //TODO fix: clashes with other alive creatures; instead should get what the units creature tile is
                                
                            if (!(item->flags.bits.in_inventory == hasunit && (!hasunit || item->flags.bits.in_job)))
                                continue;
                            */    
                            //should add a check for dead dwarf here, using simultaneously matching occupancy.unit flag (teleport doesn't respect it though)

                            MaterialInfo mat_info(item->getMaterial(), item->getMaterialIndex());
							
							/*
							uint8_t item_fore;
							uint8_t item_back;
							if(item->isDyed()){
								//if it is thread, it is stored directly in item, otherwise it is under improvements
								uint16_t dye_mat_type = 0;
								uint32_t dye_mat_index = 0;
								df::item_type item_type;
								if(item->getType() == item_type::THREAD){
									void *holdconvert = *it2; //There's got to be better way to convert from item to threaditem
									//there's static_cast<item_threadst>(item), maybe
									item_threadst *threaditem = (item_threadst*)holdconvert;
									dye_mat_type = threaditem->dye_mat_type;
									dye_mat_index = threaditem->dye_mat_index;
								} else {
									void *holdconvert = *it2;
									item_constructed *impitem = (item_constructed*)holdconvert;
									for (auto improvement = impitem->improvements.begin(); improvement != impitem->improvements.end(); improvement++)
									{
										if ((*improvement)->getType() == df::improvement_type::THREAD) {
											void *holdconvert = *improvement;
											itemimprovement_threadst *threadimp = (itemimprovement_threadst*)holdconvert;
											dye_mat_type = (threadimp)->dye.mat_type;
											dye_mat_index = (threadimp)->dye.mat_index;
											}
									}
								}
								if(dye_mat_type > 0 || dye_mat_index > 0) {
									MaterialInfo dye_info(dye_mat_type, dye_mat_index);
									item_fore = dye_info.material->state_color[3];
									item_back = dye_info.material->build_color[0];
									if (once>0) {once--;
										*out2 << "TWBT dye values fore " << (int)item_fore << "back" << (int)item_back << "itemid" << (int)item->id << std::endl;} 
								}
								
							} else {
								item_fore = mat_info.material->basic_color[0];
								item_back = mat_info.material->build_color[0];
							}
							*/
							

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && item->getType() != o.type)
                                    continue;
                                if (o.subtype != -1 && item->getSubtype() != o.subtype)
                                    continue;

                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }

                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;

                                apply_override(ret, o, item->id);
                                goto matched;
                            }
                        }
                    }
                   }
                  }
			  } else {
                    // Buildings
                  if(!is_hidden) {
                    for (auto it = to->building_overrides.begin(); it != to->building_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->buildings.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::building *bld = *it2;
                            
                            //if (bld->id !=has_transparent_building_at(tilez) //on hold unless I ensure all buildings have over1 set
							//	continue;
                            
                            if (zz != bld->z || xx < bld->x1 || xx > bld->x2 || yy < bld->y1 || yy > bld->y2)
                                continue;
                                
                            MaterialInfo mat_info(bld->mat_type, bld->mat_index);

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && bld->getType() != o.type)
                                    continue;
                                
                                if (o.subtype != -1)
                                {
                                    int subtype = (og.other_id == buildings_other_id::WORKSHOP_CUSTOM || og.other_id == buildings_other_id::FURNACE_CUSTOM) ?
                                        bld->getCustomType() : bld->getSubtype();

                                    if (subtype != o.subtype)
                                        continue;
                                }
                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }
                                
                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;
                                    
                                apply_override(ret, o, bld->id);
                                goto matched;
                            }
                        }
                    }
                  }
                    // Tile types //block declaration was moved earlier
                    //df::map_block *block = my_block_index[(xx>>4)*world->map.y_count_block*world->map.z_count_block + (yy>>4)*world->map.z_count_block + zz];//[xx>>4][yy>>4][zz];
                    if (block)
                    {
                        int tiletype = block->tiletype[xx&15][yy&15];
                        bool fluid_tile = (s0>48) && (s0<56) && ((block->designation[xx&15][yy&15].bits.flow_size)==(s0-48)) && (!is_hidden);
                        //Tile number matches unhidden fluid flow

                        df::tiletype tt = (df::tiletype)tiletype;

                        t_matpair mat(-1,-1);

                        if (to->has_material_overrides && Maps::IsValid())
                        {
                            if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                            {
                                //material is ice/water.
                                mat.mat_index = 6;
                                mat.mat_type = -1;
                            }
                            else
                            {
                                if (!r->map_cache)
                                    r->map_cache = new MapExtras::MapCache();
                                mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                            }
                        }
                        MaterialInfo mat_info(mat);
                        
                        for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                        {
                            override &o = *it3;

                            if (tiletype != o.type)
                                continue;

                            if (o.mat_flag != -1)
                            {
                                if (!mat_info.material)
                                    continue;
                                if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                    continue;
                            }

                            if (!(o.material_matches(mat_info.type, mat_info.index) || (fluid_tile && o.material_matches(6, -1)))) //also matches magma for now
                                continue;
                                
                            if (is_hidden && (s0 == 219)) //hidden digging designation isn't overriden
                                continue;
                            
                            if (is_hidden && (s0>48) && (s0<56) && tiletype == 32) //hidden openspace 1-7 isn't overriden
                                continue;
                                
                            apply_override(ret, o, coord_hash(xx,yy,zz));
                            goto matched;
                        }
                        
                    }
				  }//has_item else end
                }}
            }
        }
    }
    matched:;/*
			  if(xx==22 && yy == 21 && zz == 1){
					 *out2 << "TWBT z74 center overlay is " << (ret.overlay_texpos == transparent_texpos ? "transparent" : (ret.overlay_texpos == 0 ? "zero" : "something")) << "; tile is " << (is_hidden ? "hidden" : "not hidden") << std::endl;
			  }
			  if(!is_hidden && !isnt_main_vermin_tile && once > 0){
				  if(ret.overlay_texpos == transparent_texpos)
					 *out2 << "TWBT main vermin pos has transparent overlay" << std::endl;
					else
					 *out2 << "TWBT has non-transparent overlay" << std::endl;
					 once--;
				  }*/
              if (!is_hidden && has_vermin && !made_units_transparent){ //handling the case where there are no items
					//204 is useful vermin tile //only matches visible vermins with graphics.
					if (s[0] != so0){
						ret.overlay_texpos=get_vermin_texpos(xx,yy,zz);
						vermin_tile_under_map[tilez]=ret;						
					}
				}
			  write_values_from_ret(ret, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
}




static void write_tile_arrays_map(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top, GLfloat *overlay_color, GLfloat *overlay_texpos)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;        
    screen_to_texid_map(r, tile, ret);
    int xx;
    int yy;
    int zz;
    bool has_vermin;
        const unsigned char *s = gscreen + tile*4;
        int s0 = s[0];	       
       const unsigned char *so = gscreen_old + tile*4;
	    int   so0 = so[0];
    //int vermin_pos;
    //    const unsigned char *s = gscreen_under + tile*4;
    //has_vermin = (raw_tile_of_top_vermin(gwindow_x + x,gwindow_y + y,gwindow_z - ((s[3]&0xf0)>>4))==s[0]);
    if (has_overrides && my_block_index)
    {
            xx = gwindow_x + x;
            yy = gwindow_y + y;
                    zz = gwindow_z - ((s[3]&0xf0)>>4);
       has_vermin = raw_tile_of_top_vermin(xx,yy,zz)==s0;//there's a vermin visible, so we're getting the rest of the data from old tile
       if(has_vermin && !made_units_transparent){
		   if (s0 != so0){
                zz = gwindow_z - ((so[3]&0xf0)>>4);
				screen_to_texid_map_old(r,tile, vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy]);
				s0 = so[0];
                ret = vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy];
           }
           else
           {
			 if (vermin_tile_under_map.find(xx+yy*mapx+zz*mapx*mapy) != vermin_tile_under_map.end()){
			   ret = vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy];
			 }
           }
	      }
        if ((overrides[s0])&& !(has_vermin && s[0]==so0))//Fucking hell, removing boulder override blocked out cap hopper, i.e. it only applied vermin graphics when there was override for that tile.
        {

            if (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy)
            {

                if (s0 == 88 && df::global::cursor->x == xx && df::global::cursor->y == yy)
                {
                    int32_t texpos = cursor_small_texpos;
                    if (texpos)
                        ret.texpos = texpos;
                }
                else
                {
                    {

                    tile_overrides *to = overrides[s0];

                    // Items now also needs block for occupancy check
                    df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];//[xx>>4][yy>>4][zz];
                    //hasunit doesn't need to be inside for loop for items, even if only used there
                    //bool hasunit = false;
                    //TODO place block+hidden check togeter for items+buildings
                  if (block) //tbh I don't think it's possible for there to be an item but not block
                  {
                   //hasunit = block->occupancy[xx&15][yy&15].bits.unit;
                   if (drawn_item_map[xx+yy*mapx+zz*mapx*mapy]) //Second fails on floating minecarts, if I leave just first check it fails on any item that doesn't share tile with unit
                   {
                    for (auto it = to->item_overrides.begin(); it != to->item_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->items.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::item *item = *it2;
                            if(item->id != drawn_item_map[xx+yy*mapx+zz*mapx*mapy])
								continue;
							if(gscreentexpos[tile]) //has creature
								continue;
								/*
                            df::coord apos = Items::getPosition(item);
                            if (!(zz == apos.z && xx == apos.x && yy == apos.y))
                                continue;
                            
                            if ((item->flags.whole & bad_item_flags.whole)||(hasunit && (s0 == 1 || s0 == 2 || (s0>63 && s0<90) || (s0>96 && s0<124) || s0 ==125 || s0 == 142 || s0 == 143 || s0 == 149)))//&& item->flags.bits.dead_dwarf //needs individual tiles instead because of carried items
                                continue; //TODO fix: clashes with other alive creatures; instead should get what the units creature tile is
                                
                            if (!(item->flags.bits.in_inventory == hasunit && (!hasunit || item->flags.bits.in_job)))
                                continue;
                            */    
                            //should add a check for dead dwarf here, using simultaneously matching occupancy.unit flag (teleport doesn't respect it though)

                            MaterialInfo mat_info(item->getMaterial(), item->getMaterialIndex());

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && item->getType() != o.type)
                                    continue;
                                if (o.subtype != -1 && item->getSubtype() != o.subtype)
                                    continue;

                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }

                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;

                                apply_override(ret, o, item->id);
                                goto matched;
                            }
                        }
                    }
                   }
                  }
                  
                    // Buildings
                    for (auto it = to->building_overrides.begin(); it != to->building_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->buildings.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::building *bld = *it2;
                            if (zz != bld->z || xx < bld->x1 || xx > bld->x2 || yy < bld->y1 || yy > bld->y2)
                                continue;
                                
                            MaterialInfo mat_info(bld->mat_type, bld->mat_index);

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && bld->getType() != o.type)
                                    continue;
                                
                                if (o.subtype != -1)
                                {
                                    int subtype = (og.other_id == buildings_other_id::WORKSHOP_CUSTOM || og.other_id == buildings_other_id::FURNACE_CUSTOM) ?
                                        bld->getCustomType() : bld->getSubtype();

                                    if (subtype != o.subtype)
                                        continue;
                                }
                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }
                                
                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;
                                    
                                apply_override(ret, o, bld->id);
                                goto matched;
                            }
                        }
                    }

                    // Tile types //block declaration was moved earlier
                    //df::map_block *block = my_block_index[(xx>>4)*world->map.y_count_block*world->map.z_count_block + (yy>>4)*world->map.z_count_block + zz];//[xx>>4][yy>>4][zz];
                    if (block)
                    {
                        int tiletype = block->tiletype[xx&15][yy&15];
                        bool is_hidden = block->designation[xx&15][yy&15].bits.hidden;
                        bool fluid_tile = (s0>48) && (s0<56) && ((block->designation[xx&15][yy&15].bits.flow_size)==(s0-48)) && (!is_hidden);
                        //Tile number matches unhidden fluid flow

                        df::tiletype tt = (df::tiletype)tiletype;

                        t_matpair mat(-1,-1);

                        if (to->has_material_overrides && Maps::IsValid())
                        {
                            if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                            {
                                //material is ice/water.
                                mat.mat_index = 6;
                                mat.mat_type = -1;
                            }
                            else
                            {
                                if (!r->map_cache)
                                    r->map_cache = new MapExtras::MapCache();
                                mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                            }
                        }
                        MaterialInfo mat_info(mat);
                        
                        for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                        {
                            override &o = *it3;

                            if (tiletype != o.type)
                                continue;

                            if (o.mat_flag != -1)
                            {
                                if (!mat_info.material)
                                    continue;
                                if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                    continue;
                            }

                            if (!(o.material_matches(mat_info.type, mat_info.index) || (fluid_tile && o.material_matches(6, -1)))) //also matches magma for now
                                continue;
                                
                            if (is_hidden && (s0 == 219)) //hidden digging designation isn't overriden
                                continue;
                            
                            if (is_hidden && (s0>48) && (s0<56) && tiletype == 32) //hidden openspace 1-7 isn't overriden
                                continue;
                                
                            apply_override(ret, o, coord_hash(xx,yy,zz));
                            goto matched;
                        }
                        
                    }
                }}
            }
        }
    }
    matched:;
              if (has_vermin)//204 is useful vermin tile //only matches visible vermins with graphics.
                    {/*
						vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy] = ret;
						vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy].top_texpos=vermin_graphics_map[vermin_position_map[xx+yy*mapx+zz*mapx*mapy]];
						vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy].bg_texpos=gscreen_old[tile*4];
						vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy].texpos=gscreen_old[tile*4];
						resolve_color_pair(gscreentexpos_cf_old, gscreentexpos_cbr_old, 0, vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy]);
						
						*/
						
					if (s[0] != so0){
						ret.overlay_texpos=get_vermin_texpos(xx,yy,zz);
						vermin_tile_under_map[xx*mapy*mapzb+yy*mapzb+zz]=ret;						
					}
						//ret.bg_texpos=white_texpos;//gscreen_old[tile*4];
						//ret.texpos=gscreen_old[tile*4];
						//resolve_color_pair(gscreentexpos_cf_old, gscreentexpos_cbr_old, 0, ret);
						
						
						
							//hardcoded tiles not set by raws: 9 for vermin colony (not important)
							// 177 for vermin swarm (messes with acorn flies on z28)
      //  *out2 << "TWBT found vermin" << std::endl;
						//ret.top_texpos = vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy];
						//ret.texpos = vermin_graphics_map[vermin_position_map[xx+yy*mapx+zz*mapx*mapy]]; //TODO transparency
						//if using top texpos instead of default texpos, the red ramp behind can take some colour from the vermin instead
						//need to deal with default texpos because otherwise it'll be top-default vermin tile-bg
						//ret.top_texpos = transparent_texpos;
						//const unsigned char *v = gscreen_old + tile*4;
						//ret.texpos = 1;
						//ret.texpos = transparent_texpos;
						//applying ret_texpos overrides the default vermin texpos
						
						//ret.bg_texpos = v[0];
						//actually displayed when 31, default slug tile being partially opaque partially obscures it.
						//also by default black or inheriting brown from partial-transparent tile, olive ground cover makes it green
						//ret.br=4;
						//ret.texpos = vermin_graphics_map[vermin_position_map[xx+yy*mapx+zz*mapx*mapy]]; //TODO transparency
					//	ret.bg_texpos = transparent_texpos;  //Improvement, but still needs gscreen/screen_ptr to be set to under like when sharing tile with item.
						//Also makes slug/louise become black rather than inherit green colour from tile
						//but also makes transparency for magpie work
						//and also removes guppy tile from behind it; i.e. it replaces the original tile as bg with transparent bg
					//	ret.top_texpos = transparent_texpos; //makes guppy tile display fish tile behind it??
						//something went wrong with vermin, I don't see cap hopper anymore
						//ret = vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy];
						
					}
					write_values_from_ret(ret, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
}

#endif /* oldstuff */

static void write_tile_arrays_under(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *tex, GLfloat *fg_top, GLfloat *tex_top)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;
    screen_to_texid_under(tile, ret);
    int xx;
    int yy;
    int zz;
        const unsigned char *s = gscreen_under + tile*4;
        int s0 = s[0];
    if (has_overrides && my_block_index)
    {
            xx = gwindow_x + x;
            yy = gwindow_y + y;
                zz = gwindow_z - ((s[3]&0xf0)>>4);
        if (overrides[s0])
        {

            if (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy)
            {

                tile_overrides *to = overrides[s0];

                // Tile types
                df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];//[xx>>4][yy>>4][zz];
                if (block)
                {
                    int tiletype = block->tiletype[xx&15][yy&15];
                    bool is_hidden = block->designation[xx&15][yy&15].bits.hidden;
                    bool fluid_tile = (s0>48) && (s0<56) && ((block->designation[xx&15][yy&15].bits.flow_size)==(s0-48)) && (!is_hidden);
                        
                    df::tiletype tt = (df::tiletype)tiletype;

                    t_matpair mat(-1, -1);

                    if (to->has_material_overrides && Maps::IsValid())
                    {
                        if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                        {
                            //material is ice.
                            mat.mat_index = 6;
                            mat.mat_type = -1;
                        }
                        else
                        {
                            if (!r->map_cache)
                                r->map_cache = new MapExtras::MapCache();
                            mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                        }
                    }
                    MaterialInfo mat_info(mat);

                    for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                    {
                        override &o = *it3;

                        if (tiletype != o.type)
                            continue;

                        if (o.mat_flag != -1)
                        {
                            if (!mat_info.material)
                                continue;
                            if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                continue;
                        }

                        if (!(o.material_matches(mat_info.type, mat_info.index) || (fluid_tile && o.material_matches(6, -1)))) //also matches magma for now
                                continue;
                        
                        //It is unlikely but theoreticaly possible to have hidden tiles, even hidden walls, with items.
                        if (is_hidden && (s0 == 219)) //hidden digging designation isn't overriden
                                continue;
                            
                        if (is_hidden && (s0>48) && (s0<56) && tiletype == 32) //hidden openspace in 1-7 isn't overridden
                                continue;

                        apply_override_under(ret, o, coord_hash(xx,yy,zz));
                        goto matchedu;
                    }
                }
            }
        }
    }
    matchedu:;
	if(s[2]) //there is bg present, i.e. engravings or designations, so we discard the top layer
		//technically s2 will also match natural walls with stuff inside them (but not fortifications or ice, i.e. just obsidian cast stuff)
	{
		//note that this check fails under multilevel due it storing height in bold 
		//transparent texpos fails
		
		write_values_from_ret_under_back(ret, fg, tex, fg_top, tex_top);
		//write_values_from_ret_under(ret, fg_top, tex_top,fg, tex);// doesn't work
	}
	else write_values_from_ret_under(ret, fg, tex, fg_top, tex_top);
}

static void write_black_array(GLfloat *fg, GLfloat *tex){
	//set black rgb
	//float r=enabler->ccolor[0][0];
	//float g=enabler->ccolor[0][1];
	//float b= enabler->ccolor[0][2];
        fg += 8;
        *(fg++) = 0;//r;
        *(fg++) = 0;//g;
        *(fg++) = 0;//b;
        *(fg++) = 1;
        
        fg += 8;
        *(fg++) = 0;//r;
        *(fg++) = 0;//g;
        *(fg++) = 0;//b;
        *(fg++) = 1;

        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
		SETPAIRTEX(white_texpos, tex);//white or transparent texpos? Former would need only 1 overwrite, latter might be easier to render
}

static void write_tile_arrays_under_triplet(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;
    screen_to_texid_under(tile, ret);
    const unsigned char *s = gscreen_under + tile*4;
    int s0 = s[0];
    if (has_overrides && my_block_index)
    {
		  if (overrides[s0])
        {
            int xx = gwindow_x + x;
            int yy = gwindow_y + y;
            int zz = gwindow_z - ((s[3]&0xf0)>>4);
      

            if (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy)
            {

                tile_overrides *to = overrides[s0];

                // Tile types
                df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];//[xx>>4][yy>>4][zz];
                if (block)
                {
                    int tiletype = block->tiletype[xx&15][yy&15];
                    bool is_hidden = block->designation[xx&15][yy&15].bits.hidden;
                    bool fluid_tile = (s0>48) && (s0<56) && ((block->designation[xx&15][yy&15].bits.flow_size)==(s0-48)) && (!is_hidden);
                        
                    df::tiletype tt = (df::tiletype)tiletype;

                    t_matpair mat(-1, -1);

                    if (to->has_material_overrides && Maps::IsValid())
                    {
                        if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                        {
                            //material is ice.
                            mat.mat_index = 6;
                            mat.mat_type = -1;
                        }
                        else
                        {
                            if (!r->map_cache)
                                r->map_cache = new MapExtras::MapCache();
                            mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                        }
                    }
                    MaterialInfo mat_info(mat);

                    for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                    {
                        override &o = *it3;

                        if (tiletype != o.type)
                            continue;

                        if (o.mat_flag != -1)
                        {
                            if (!mat_info.material)
                                continue;
                            if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                continue;
                        }

                        if (!(o.material_matches(mat_info.type, mat_info.index) || (fluid_tile && o.material_matches(6, -1)))) //also matches magma for now
                                continue;
                        
                        //It is unlikely but theoreticaly possible to have hidden tiles, even hidden walls, with items.
                        if (is_hidden && (s0 == 219)) //hidden digging designation isn't overriden
                                continue;
                            
                        if (is_hidden && (s0>48) && (s0<56) && tiletype == 32) //hidden openspace in 1-7 isn't overridden
                                continue;

                        apply_override(ret, o, coord_hash(xx,yy,zz));
                        goto matchedt;
                    }
                }
            }
        }
    }
    matchedt:; 
    write_values_from_ret_under_triplet(ret, fg, tex, bg, tex_bg, fg_top, tex_top);
}

static void write_tile_arrays_building(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top, GLfloat *overlay_color, GLfloat *overlay_texpos)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;
	const unsigned char *s = gscreen_building + tile*4;
	int s0 = s[0];
	int  xx = gwindow_x + x;
	int yy = gwindow_y + y;
	int zz = gwindow_z - ((s[3]&0xf0)>>4);
	int tilez = xx*mapy*mapzb+yy*mapzb+zz;
	bool present_building = painted_building_map.find(tilez) != painted_building_map.end();
	bool has_building =  present_building && (buildingbuffer_map.find(tilez)!=buildingbuffer_map.end());
    if (painted_building == false && used_map_overlay && has_building)
		{//gscreen is vermin/unit, other 2 are terrain, building is nowhere to be found...'cept for the map
			//TODO check multilevel (creature/vermin)/bld/bld/terrain??
			screen_to_texid_building_stored(tile, ret, buildingbuffer_map[tilez]);
			s0 = buildingbuffer_map[tilez].portions.tile;
		}
    else  
		screen_to_texid_building(tile, ret);
		
	//if (painted_building != false) has_building = false; //so I don't apply buiding override twice
    
    if (has_overrides && my_block_index)
    {
        if (overrides[s0])
        {
            if (xx >= 0 && yy >= 0 && xx < mapx && yy < mapy)
            {
                tile_overrides *to = overrides[s0];
					// Buildings
					// Negative check that I didn't paint building rather than positive check of having building due non-overridden depot and wagon
					// not using present building until I've checked how that affects depot
					if (present_building && !painted_building) //(painted_building_map[xx+yy*mapx+zz*mapx*mapy] == false) //TODO fix display error for building L1/ice floor L2
					{
                    for (auto it = to->building_overrides.begin(); it != to->building_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->buildings.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::building *bld = *it2;
                           if (not (has_building && (painted_building_map[tilez] == bld->id))) //well chain/bucket overrides would be disabled by z check without this
								if (zz != bld->z || xx < bld->x1 || xx > bld->x2 || yy < bld->y1 || yy > bld->y2)
									continue;
                                
                            MaterialInfo mat_info(bld->mat_type, 
                            #ifdef unbuiltbuiltinfix 
                            (bld->mat_type < 19 && bld->mat_type > 0 ) ? -1 : 
                            #endif
                            bld->mat_index);

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && bld->getType() != o.type)
                                    continue;
                                
                                if (o.subtype != -1)
                                {
                                    int subtype = (og.other_id == buildings_other_id::WORKSHOP_CUSTOM || og.other_id == buildings_other_id::FURNACE_CUSTOM) ?
                                        bld->getCustomType() : bld->getSubtype();

                                    if (subtype != o.subtype)
                                        continue;
                                }
                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }
                                
                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;
                                #ifdef buildingonlyoverrides
                                if (o.building_tile_only && !(buildingbuffer_map.find(tilez)!=buildingbuffer_map.end() && buildingbuffer_map[tilez].portions.tile == s0))
									continue;
                                #endif    
                                apply_override(ret, o, bld->id);
                                goto matchedb;
                            }
                        }
                    }
					}

                // Tile types
                
                df::map_block *block = my_block_index[(xx>>4)*mapyb*mapzb + (yy>>4)*mapzb + zz];//[xx>>4][yy>>4][zz];
                if (block)
                {
                    int tiletype = block->tiletype[xx&15][yy&15];
                    bool is_hidden = block->designation[xx&15][yy&15].bits.hidden;
                    bool fluid_tile = (s0>48) && (s0<56) && ((block->designation[xx&15][yy&15].bits.flow_size)==(s0-48)) && (!is_hidden);
                        
                    df::tiletype tt = (df::tiletype)tiletype;

                    t_matpair mat(-1, -1);

                    if (to->has_material_overrides && Maps::IsValid())
                    {
                        if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                        {
                            //material is ice.
                            mat.mat_index = 6;
                            mat.mat_type = -1;
                        }
                        else
                        {
                            if (!r->map_cache)
                                r->map_cache = new MapExtras::MapCache();
                            mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                        }
                    }
                    MaterialInfo mat_info(mat);

                    for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                    {
                        override &o = *it3;

                        if (tiletype != o.type)
                            continue;

                        if (o.mat_flag != -1)
                        {
                            if (!mat_info.material)
                                continue;
                            if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                continue;
                        }

                        if (!(o.material_matches(mat_info.type, mat_info.index) || (fluid_tile && o.material_matches(6, -1)))) //also matches magma for now
                                continue;
                        
                        //It is unlikely but theoreticaly possible to have hidden tiles, even hidden walls, with items.
                        if (is_hidden && (s0 == 219)) //hidden digging designation isn't overriden
                                continue;
                            
                        if (is_hidden && (s0>48) && (s0<56) && tiletype == 32) //hidden openspace in 1-7 isn't overridden
                                continue;

                        apply_override(ret, o, coord_hash(xx,yy,zz));
                        goto matchedb;
                    }
                }
                
            }
        }
    }
    matchedb:;
                  if ((raw_tile_of_top_vermin(xx,yy,zz)>0)&&(painted_vermin_map.find(xx*mapy*mapzb+yy*mapzb+zz) == painted_vermin_map.end()))//doing this permits using vermin picture even when vermin is invisible in vanilla UI
                    {//TODO: set painted_vermin map when using non-unit transparency override
						//-1 didn't work for some reason i.e. outputted text at locations with no vermin at tile
						ret.overlay_texpos = get_vermin_texpos(xx,yy,zz);
					//	*out2 << "TWBT painted building vermin x " << (xx) << " y " << (yy)  << std::endl;
					}
					write_values_from_ret(ret, fg, bg, tex, tex_bg, fg_top, tex_top, overlay_color, overlay_texpos);
}

