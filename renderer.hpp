#include "png++/png.hpp"
#include "png++/solid_pixel_buffer_rev.hpp"

static volatile int domapshot = 0;

constexpr int layer_count = 10;
constexpr int color_mult = 4*6;
constexpr int texpos_mult = 2*6;
constexpr uint32_t HIDDEN_UNDERGROUND = 16777248; //t 32 // fg 0 //bg 0 // bold 1

#define SETTEX(tt) \
    *(tex++) = txt[tt].left; \
    *(tex++) = txt[tt].bottom; \
    *(tex++) = txt[tt].right; \
    *(tex++) = txt[tt].bottom; \
    *(tex++) = txt[tt].left; \
    *(tex++) = txt[tt].top; \
    \
    *(tex++) = txt[tt].left; \
    *(tex++) = txt[tt].top; \
    *(tex++) = txt[tt].right; \
    *(tex++) = txt[tt].bottom; \
    *(tex++) = txt[tt].right; \
    *(tex++) = txt[tt].top;

	//Note: Should replace /2,5 by its reciporal if using this.
static void write_tile_vertexes_oblique(GLfloat x, GLfloat y, GLfloat *vertex, float d)
{
    vertex[0]  = x;   // Upper left
    vertex[1]  = y/2.5 + d*0.1;
    vertex[2]  = x + 1; // Upper right
    vertex[3]  = y/2.5 + d*0.1;
    vertex[4]  = x;   // Lower left
    vertex[5]  = y/2.5 + d*0.1 + 1;
    vertex[6]  = x;   // Lower left again (triangle 2)
    vertex[7]  = y/2.5 + d*0.1 + 1;
    vertex[8]  = x + 1; // Upper right
    vertex[9]  = y/2.5 + d*0.1;
    vertex[10] = x + 1; // Lower right
    vertex[11] = y/2.5 + d*0.1 + 1;
}

static void write_tile_vertexes(GLfloat x, GLfloat y, GLfloat *vertex, float d)
{
    vertex[0]  = x;   // Upper left
    vertex[1]  = y;
    vertex[2]  = x + 1; // Upper right
    vertex[3]  = y;
    vertex[4]  = x;   // Lower left
    vertex[5]  = y + 1;
    vertex[6]  = x;   // Lower left again (triangle 2)
    vertex[7]  = y + 1;
    vertex[8]  = x + 1; // Upper right
    vertex[9]  = y;
    vertex[10] = x + 1; // Lower right
    vertex[11] = y + 1;
}

renderer_cool::renderer_cool()
{
    dummy = 'TWBT';
    gvertexes = 0, gfg = 0, gtex = 0;
    gdimx = 0, gdimy = 0, gdimxfull = 0, gdimyfull = 0;
    gdispx = 0, gdispy = 0;
    goff_x = 0, goff_y = 0, gsize_x = 0, gsize_y = 0;
    needs_reshape = false;
    needs_zoom = 0;
    map_cache = 0;
}

void renderer_cool::update_all()
{
    glClear(GL_COLOR_BUFFER_BIT);
    
    for (int x = 0; x < tdimx; ++x)
        for (int y = 0; y < tdimy; ++y)
            update_tile(x, y);    
}

void renderer_cool::update_tile(int x, int y)
{
    if (!enabled)
    {
        this->update_tile_old(x, y);
        return;
    }

    //XXX: sometimes this might be called while gps->dimx/y are set to map dim > text dim, and this will crash
    //XXX: better not to use NO_DISPLAY_PATCH, but if we must, let's check x/y here
#ifdef NO_DISPLAY_PATCH
    if (x >= tdimx || y >= tdimy)
        return;
#endif

    const int tile = x * tdimy + y;

    GLfloat *_fg  = fg + tile * color_mult;
    GLfloat *_bg  = bg + tile * color_mult;
    GLfloat *_tex = tex + tile * texpos_mult;

    write_tile_arrays_text(this, x, y, _fg, _bg, _tex);
}

void renderer_cool::update_map_tile(int x, int y) //can't declare noexcept, renderer_cool has different exception handler
{
    //REDO if layer count changes
    const int tile = x * gdimy + y;/*
        const unsigned char *s = gscreen + tile*4;
    int xx;
    int yy;
    int zz;
            xx = gwindow_x + x;
            yy = gwindow_y + y;
                    zz = gwindow_z - ((s[3]&0xf0)>>4);
    if((xx >= 0 && yy >= 0 && xx < mapx && yy < mapy) &&raw_tile_of_top_vermin(xx,yy,zz))
		((uint32_t*)gscreen_under)[(xx-mwindow_x)*this->gdimy + yy-gwindow_y] = ((uint32_t*)gscreen)[(xx-mwindow_x)*this->gdimy + yy-gwindow_y];*/

	const int layer_tile_count = layer_count*tile;
	
	//GLfloat *_bg_under      = gfg  + (layer_tile_count+0) * color_mult; //tile is multiplied by nr of layers and counts from 0 to last
    //GLfloat *_tex_bg_under  = gtex + (layer_tile_count+0) * texpos_mult;
    GLfloat *_fg_under      = gfg  + (layer_tile_count+0) * color_mult;
    GLfloat *_tex_under     = gtex + (layer_tile_count+0) * texpos_mult;
    GLfloat *_fg_top_under  = gfg  + (layer_tile_count+1) * color_mult;
    GLfloat *_tex_top_under = gtex + (layer_tile_count+1) * texpos_mult;
    
    //GLfloat *_overlay_color_under            = gfg  + (layer_tile_count+3) * color_mult;
    //GLfloat *_overlay_texpos_under           = gtex + (layer_tile_count+3) * texpos_mult;
    
    GLfloat *_bg_building      = gfg  + (layer_tile_count+2) * color_mult; //tile is multiplied by nr of layers and counts from 0 to last
    GLfloat *_tex_bg_building  = gtex + (layer_tile_count+2) * texpos_mult;
    GLfloat *_fg_building      = gfg  + (layer_tile_count+3) * color_mult;
    GLfloat *_tex_building     = gtex + (layer_tile_count+3) * texpos_mult;
    GLfloat *_fg_top_building  = gfg  + (layer_tile_count+4) * color_mult;
    GLfloat *_tex_top_building = gtex + (layer_tile_count+4) * texpos_mult;
    
    GLfloat *_overlay_color_building            = gfg  + (layer_tile_count+5) * color_mult;
    GLfloat *_overlay_texpos_building           = gtex + (layer_tile_count+5) * texpos_mult;

    GLfloat *_bg            = gfg  + (layer_tile_count+6) * color_mult;
    GLfloat *_tex_bg        = gtex + (layer_tile_count+6) * texpos_mult;
    GLfloat *_fg            = gfg  + (layer_tile_count+7) * color_mult;
    GLfloat *_tex           = gtex + (layer_tile_count+7) * texpos_mult;
    GLfloat *_fg_top        = gfg  + (layer_tile_count+8 ) * color_mult;
    GLfloat *_tex_top       = gtex + (layer_tile_count+8 ) * texpos_mult;
    
    GLfloat *_overlay_color            = gfg  + (layer_tile_count+9 ) * color_mult;
    GLfloat *_overlay_texpos           = gtex + (layer_tile_count+9 ) * texpos_mult;
    

	//swapped order so I could track map-written buildings.
	//for some reason it only worked for forge, not mechanic???
	painted_building = false;
	used_map_overlay = false;
	
	if ( maxlevels && (((gscreen[tile * 4 + 3] & 0xf0) >> 4)>((gscreen_building[tile * 4 + 3] & 0xf0) >> 4))){
		write_tile_arrays_building(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
		#ifdef OLDSTUFF
		#ifdef oldstuff1
		write_tile_arrays_map(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
		#else
		#ifdef oldstuff2
		write_tile_arrays_map2(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
		#endif
		#endif
		#else
		write_tile_arrays_map3(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
		#endif

	} else {
		#ifdef OLDSTUFF
		#ifdef oldstuff1
		write_tile_arrays_map(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
		#else
		#ifdef oldstuff2
		write_tile_arrays_map2(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
		#endif
		#endif
		#else
		write_tile_arrays_map3(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
		#endif
		if (maxlevels && painted_building &&  ((gscreen[tile * 4 + 3] & 0xf0) >> 4)!=((gscreen_building[tile * 4 + 3] & 0xf0) >> 4))
			painted_building = false;

		write_tile_arrays_building(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
    }
    
    write_tile_arrays_under(this, x, y, _fg_under, _tex_under, _fg_top_under, _tex_top_under);
    
    
    if (maxlevels)
    {
        float d = (float)((( (((gscreen[tile * 4 + 3] & 0xf0) >> 4)>((gscreen_building[tile * 4 + 3] & 0xf0) >> 4)) ? gscreen_building[tile * 4 + 3] : gscreen[tile * 4 + 3]) & 0xf0) >> 4);

        // for oblique
        //write_tile_vertexes(x, y, gvertexes + 6 * 2 * tile, d);

        depth[tile] = !gscreen[tile*4] ? 0x7f : d; //TODO: no need for this in fort mode

        if (fogdensity > 0)
        {
            if (d > 0)
                d = d*fogstep + fogstart;

            for (int j = 0; j < 6*layer_count; j++) //last = nr of layers
                fogcoord[layer_tile_count * 6 + j] = d; //last = nr of layers
        }
    }
   // if((xx >= 0 && yy >= 0 && xx < mapx && yy < mapy) &&raw_tile_of_top_vermin(xx,yy,zz))
	//	((uint32_t*)screen_under_ptr)[(x-mwindow_x)*this->gdimy + y-gwindow_y] = vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy];
}

void renderer_cool::update_map_tile2(int x, int y) //can't declare noexcept, renderer_cool has different exception handler
{
    //REDO if layer count changes
    const int tile = x * gdimy + y;
	const int layer_tile_count = layer_count*tile;
	
	painted_building = false;//prevention for applying override on building twice when building tile = terrain tile
	used_map_overlay = false;//check that map3 used vermin/creature overlay, which means need to pull building data from storage if it wasn't already pulled
	
	//alas, f(gfg[i], gfg[--i], gfg[--i]) is undefined i.e. the order in which the 3 args are evaluated can vary
	//So need to predefine things that go in
	
	GLfloat *_bg            = gfg  + (layer_tile_count+6) * color_mult;
    GLfloat *_tex_bg        = gtex + (layer_tile_count+6) * texpos_mult;
    GLfloat *_fg            = gfg  + (layer_tile_count+7) * color_mult;
    GLfloat *_tex           = gtex + (layer_tile_count+7) * texpos_mult;
    GLfloat *_fg_top        = gfg  + (layer_tile_count+8 ) * color_mult;
    GLfloat *_tex_top       = gtex + (layer_tile_count+8 ) * texpos_mult;
    GLfloat *_overlay_color            = gfg  + (layer_tile_count+9 ) * color_mult;
    GLfloat *_overlay_texpos           = gtex + (layer_tile_count+9 ) * texpos_mult;
    //At bare minimum, I'll always use 4 layers
    
	//ERROR: tilez is off; x/y only matches xx/yy if gwindow_<> is 0
	//What mess is this, depth for gz?
    #define xd (x+gwindow_x)
    #define yd (y+gwindow_y)
    //int zz = gwindow_z - ((s[3]&0xf0)>>4);
    
    /*
     * Paths to follow:
     * map3, bld, under	= bld on g_b && item/bld on g_m
     * map3, bld, black = gscreentexpos && vermin on g_b
     * map3, triplet, black = item || bld on g_b || gscreentexpos || g_m != g_b
     * bld, black	= contested (vermin vs vermin visible)
     * triplet, black = !(bld || item || gscreentexpos || vermin)
     * 
	*/
    
	const int xx = x+gwindow_x;
	const int yy = y+gwindow_y;
			
	const unsigned char *m = gscreen + tile*4;
	const unsigned char *b = gscreen_building + tile*4;
	//Infoplus(df::global::cursor->x == xx && df::global::cursor->y == yy)
	if (!(((uint32_t*)gscreen_under)[tile])){ //D1 only untrue on layer 8 due wholesale copy and buildings present on all of others
		Infoplus(x==0 && y==0, "no gscreen_under");
		if(raw_tile_of_top_vermin(xd,yd,gwindow_z - ((m[3]&0xf0)>>4))==m[0] || gscreentexpos[tile]){
			write_tile_arrays_map3(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
	
			//need to associate extra level for black bg that is below all the other layers
			GLfloat *_overlay_color_building            = gfg  + (layer_tile_count+5) * color_mult;
			GLfloat *_overlay_texpos_building           = gtex + (layer_tile_count+5) * texpos_mult;
	
			write_black_array(_overlay_color_building, _overlay_texpos_building);
		} else {
			//reminder: toplevels also means replacing gscreen_under with gscreen
			//because under triplet reads g_u32 and we're using g32, need to replace it
			((uint32_t*)gscreen_under)[tile] = ((uint32_t*)gscreen)[tile];
		//	write_tile_arrays_under_triplet(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
			write_tile_arrays_under_triplet(this, x, y, _fg_top, _fg, _tex_top, _tex, _overlay_color, _overlay_texpos); 
				//overlay	- use for fg-top
				//top		- use for fg
				//fg		- use for bg
				//bg		- into black array
				//TODO ISSUE: u_t Doesn't handle cursor = stair override on cursor on stairs?
			write_black_array(_bg, _tex_bg);
		}
	} else if(m[0] == 88 && df::global::cursor->x == xx && df::global::cursor->y == yy) {
			//cursor, which has black space
			//I could otherwise avoid defining xx/yy always, buuut then cursor is not rendered with swapped multilevel
			//though using full triplet would allow pushing cursor to overlay and drawing relevant data for item/bld/tile from saved data, making transparent cursor possible
			write_tile_arrays_map3(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
	
			//need to associate extra level for black bg that is below all the other layers
			GLfloat *_overlay_color_building            = gfg  + (layer_tile_count+5) * color_mult;
			GLfloat *_overlay_texpos_building           = gtex + (layer_tile_count+5) * texpos_mult;
	
			write_black_array(_overlay_color_building, _overlay_texpos_building);
	} else { //complex tile start
		
	const unsigned char *u = gscreen_under + tile*4;
	Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, " under tile " << (int32_t)((u[0])) << " under fg " << (int32_t)((u[1])) << " under bg " << (int32_t)((u[2])) << " under bold " << (int32_t)((u[3]&0x0f)) << " under depth " << (int32_t)((u[3]&0xf0)>>4));
	
	#define zequals (((u[3]&0xf0) == (b[3]&0xf0)) && ((b[3]&0xf0) == (m[3]&0xf0)))
	int ametracker=0; //For tracking how in-depth I need to go, based on presence of vermin/unit/building/item/terrain
	if(maxlevels && !zequals){
		Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "map depth " << (int32_t)((m[3]&0xf0)>>4) << " bld depth " << (int32_t)((b[3]&0xf0)>>4) << " under depth " << (int32_t)((u[3]&0xf0)>>4));
		if (((m[3] & 0xf0))>((b[3] & 0xf0))){//swapped
			ametracker = 0;
Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "in swapped path");
		
		    GLfloat *_fg_under      = gfg  + (layer_tile_count+0) * color_mult;
			GLfloat *_tex_under     = gtex + (layer_tile_count+0) * texpos_mult;
			GLfloat *_fg_top_under  = gfg  + (layer_tile_count+1) * color_mult;
			GLfloat *_tex_top_under = gtex + (layer_tile_count+1) * texpos_mult;
			
			GLfloat *_bg_building      = gfg  + (layer_tile_count+2) * color_mult; //tile is multiplied by nr of layers and counts from 0 to last
			GLfloat *_tex_bg_building  = gtex + (layer_tile_count+2) * texpos_mult;
			GLfloat *_fg_building      = gfg  + (layer_tile_count+3) * color_mult;
			GLfloat *_tex_building     = gtex + (layer_tile_count+3) * texpos_mult;
			GLfloat *_fg_top_building  = gfg  + (layer_tile_count+4) * color_mult;
			GLfloat *_tex_top_building = gtex + (layer_tile_count+4) * texpos_mult;
			
			GLfloat *_overlay_color_building            = gfg  + (layer_tile_count+5) * color_mult;
			GLfloat *_overlay_texpos_building           = gtex + (layer_tile_count+5) * texpos_mult;
			if(raw_tile_of_top_vermin(xx,yy,gwindow_z-((b[3]&0xf0)>>4))==b[0]){
				//Okay, vermin on building layer over terrain doesn't work; it'll render it as vermin graphics/vermin tile on topmost
				if (buildingbuffer_map.find(xx*mapy*mapzb+yy*mapzb+gwindow_z-((b[3]&0xf0)>>4))!=buildingbuffer_map.end()) {
					used_map_overlay=true;
					write_tile_arrays_building(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
				} else {
					swap(*((uint32_t*)gscreen + tile), *((uint32_t*)gscreen_building + tile));
					write_tile_arrays_map3(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
					swap(*((uint32_t*)gscreen + tile), *((uint32_t*)gscreen_building + tile));
				}
			} else
				write_tile_arrays_building(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
			write_tile_arrays_map3(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
			write_tile_arrays_under(this, x, y, _fg_under, _tex_under, _fg_top_under, _tex_top_under);
			
		
		} else {
Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "in minpaint path");
			write_tile_arrays_map3(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
			if( ((m[3] & 0xf0)!=(b[3] & 0xf0) ) ) {
				if (painted_building) painted_building = false;
				ametracker = 4; 
			} else {
			int tilezp = xd*mapy*mapzb+yd*mapzb+gwindow_z;//-((m[3]&0xf0)>>4);
			ametracker = ((gscreentexpos[tile] ) ? 1 : 0) + 
					 (((painted_building_map.find(tilezp-((b[3]&0xf0)>>4)) != painted_building_map.end())) ? 2 : 0 ) + 
					 (( (drawn_item_map.find(tilezp-((m[3]&0xf0)>>4)) != drawn_item_map.end() 
						|| painted_building_map.find(tilezp-((m[3]&0xf0)>>4)) != painted_building_map.end() ) ? 2 : 0));
			}
				//different z-levels - so support for multiz with building over building
			goto minpaint; //TODO do I handle painted_building = false after map3?
		}
	} else {
		//bool ametracker == has_unit || has_item || has_bld; //I can't use g32==g_u32==g_b32 because can have item same color and tile as terrain
		//but g_u32==0 could be useful
		//For undead, ametracker is not met, but g32 !=g_u32 && g_u32 is met, assuming ground isn't also purple N or creature symbol
		//for cases of item and bld, often same applies, but there will be edge cases where there will be, say, a tool on ramp
		//also we checked for zero g_u32 earlier already
		//Also note that were only here if gscreentexpos or gscreen != gscreen old anyway
		Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "TWBT in else path");


		int tilez; tilez = xx*mapy*mapzb+yy*mapzb+gwindow_z-((m[3]&0xf0)>>4);
//		ametracker =  (( (drawn_item_map.find(tilez) != drawn_item_map.end()) || gscreentexpos[tile] && raw_tile_of_top_vermin(xx,yy,gwindow_z-((b[3]&0xf0)>>4))>-1) ? 1 : 0) 
	//				+ (gscreentexpos[tile] || ((painted_building_map.find(tilez) != painted_building_map.end())) ? 1 : 0 );
	
    /*
     * Paths to follow:
     * A: map3, bld, under	= bld on g_b && item/bld on g_m
     * B: map3, bld, black = gscreentexpos && vermin on g_b
     * C: map3, triplet, black = item || bld on g_b || gscreentexpos || g_m != g_b
     * D: bld, black	= vermin && g_m == g_b
     * E: triplet, black = !(bld || item || gscreentexpos || vermin) && g_m == g_b
     * 
	*/
		//ametracker = gtxt && v * 1 + bld_g *2 + item||bld_m *2 
		ametracker = ((gscreentexpos[tile]) ? 1 : 0) + 
					 (((painted_building_map.find(tilez) != painted_building_map.end())) ? 2 : 0 ) + 
					 (( (drawn_item_map.find(tilez) != drawn_item_map.end()) ? 2 : 0));

		if( ((((uint32_t*)gscreen_under)[tile])==(((uint32_t*)gscreen)[tile])) && 
			//ERR: Using this shortcut means I'm not using ametracker
			//But if I remove the check, I'll have vermin dragging. 
			// !(gscreentexpos[tile] || (drawn_item_map.find(tilez) != drawn_item_map.end()) || (painted_building_map.find(tilez) != painted_building_map.end()) ) 
			!ametracker
			){
			Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "equal m/u and no unit, building or item");
			if( /* (painted_building_map.find(tilez) != painted_building_map.end()) || */ 
			(raw_tile_of_top_vermin(xx,yy,gwindow_z-((m[3]&0xf0)>>4)) == m[0]) 
			//&& (HIDDEN_UNDERGROUND !=(((uint32_t*)gscreen)[tile]))  /*==m[0] dragging?*/ 
			){
			Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "Found vermin/building, using building, gscreen is " << (uint32_t)((((uint32_t*)gscreen)[tile])) << "  hidden underground is  " << HIDDEN_UNDERGROUND );
				//vermin that shares gscreen and raw tile
				//not using the building check because invisible vermins get stuck when handled in gscreen
				//also mind that _building uses gscreen_building, but we want it to take values from gscreen
				//((uint32_t*)gscreen_building)[tile] = ((uint32_t*)gscreen)[tile];
				//otoh under == gscreen anyway, _building == gscreen also so this shouldn't be necessary
				//TODO logging+panning check with wellinfo
				
				write_tile_arrays_building(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);
				
				GLfloat *_overlay_color_building            = gfg  + (layer_tile_count+5) * color_mult;
				GLfloat *_overlay_texpos_building           = gtex + (layer_tile_count+5) * texpos_mult;

				write_black_array(_overlay_color_building, _overlay_texpos_building);
				
			} else {
				Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "Using under triplet");
				write_tile_arrays_under_triplet(this, x, y, _fg_top, _fg, _tex_top, _tex, _overlay_color, _overlay_texpos); 
				write_black_array(_bg, _tex_bg);
			}
			
		} else {
			Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "Using map3");
			write_tile_arrays_map3(this, x, y, _fg, _bg, _tex, _tex_bg, _fg_top, _tex_top, _overlay_color, _overlay_texpos);//items/units also fail here. Prob related?
			
			minpaint:;
			
			GLfloat *_bg_building      = gfg  + (layer_tile_count+2) * color_mult; //tile is multiplied by nr of layers and counts from 0 to last
			GLfloat *_tex_bg_building  = gtex + (layer_tile_count+2) * texpos_mult;
			GLfloat *_fg_building      = gfg  + (layer_tile_count+3) * color_mult;
			GLfloat *_tex_building     = gtex + (layer_tile_count+3) * texpos_mult;
			GLfloat *_fg_top_building  = gfg  + (layer_tile_count+4) * color_mult;
			GLfloat *_tex_top_building = gtex + (layer_tile_count+4) * texpos_mult;
			GLfloat *_overlay_color_building            = gfg  + (layer_tile_count+5) * color_mult;
			GLfloat *_overlay_texpos_building           = gtex + (layer_tile_count+5) * texpos_mult;
			
			if( ametracker > 3 //(!painted_building && (painted_building_map.find(xx*mapy*mapzb+yy*mapzb+gwindow_z-((b[3]&0xf0)>>4)) != painted_building_map.end()))
			){
				/* z105 floating fly above z103 used to go here and is now broken 
				 * because gscreen finds no building on g_m z, resulting in v/t/t/t+under triplet taking from g_u
				 * g_u being the actually built floor below g_b
				 * It would also fail to handle building override for the floating floor properly, so it MUST go here
				*/
				Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "Found painted building; using building/under");
				
				write_tile_arrays_building(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
				   
				GLfloat *_fg_under      = gfg  + (layer_tile_count+0) * color_mult;
				GLfloat *_tex_under     = gtex + (layer_tile_count+0) * texpos_mult;
				GLfloat *_fg_top_under  = gfg  + (layer_tile_count+1) * color_mult;
				GLfloat *_tex_top_under = gtex + (layer_tile_count+1) * texpos_mult;
				
				write_tile_arrays_under(this, x, y, _fg_under, _tex_under, _fg_top_under, _tex_top_under);
				
			} else if( (ametracker & 1) && raw_tile_of_top_vermin(xx,yy,gwindow_z-((b[3]&0xf0)>>4))>-1){//on bld_z, no matter if maxlevels is on or off
				Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "Found vermin, using  building/black");
								
				GLfloat *_fg_top_under  = gfg  + (layer_tile_count+1) * color_mult;
				GLfloat *_tex_top_under = gtex + (layer_tile_count+1) * texpos_mult;
			
				write_tile_arrays_building(this, x, y, _fg_building, _bg_building, _tex_building, _tex_bg_building, _fg_top_building, _tex_top_building, _overlay_color_building, _overlay_texpos_building);
				write_black_array(_fg_top_under, _tex_top_under);//can't collapse as nextvals here and above are different.
			
			} else {
				Infoplus((xd+1)==df::global::cursor->x && yd==df::global::cursor->y, "Finishing with triplet/black");
				
				write_tile_arrays_under_triplet	(this, x, y, _fg_top_building, _fg_building, _tex_top_building, _tex_building, _overlay_color_building, _overlay_texpos_building);
		
				write_black_array(_bg_building, _tex_bg_building);
			}
			
		}
	  }
	  
    } //complex tile end end
    
    if (maxlevels)
    {
        float d = (float)((( (((m[3] & 0xf0))>((b[3] & 0xf0))) ? b[3] : m[3]) & 0xf0) >> 4);

        // for oblique
        //write_tile_vertexes(x, y, gvertexes + 6 * 2 * tile, d);

        depth[tile] = !m[0] ? 0x7f : d; //TODO: no need for this in fort mode

        if (fogdensity > 0)
        {
            if (d > 0)
                d = d*fogstep + fogstart;

            for (int j = 0; j < 6*layer_count; j++) //last = nr of layers
                fogcoord[layer_tile_count * 6 + j] = d; //last = nr of layers
        }
    }
   // if((xx >= 0 && yy >= 0 && xx < mapx && yy < mapy) &&raw_tile_of_top_vermin(xx,yy,zz))
	//	((uint32_t*)screen_under_ptr)[(x-mwindow_x)*this->gdimy + y-gwindow_y] = vermin_tile_under_map[xx+yy*mapx+zz*mapx*mapy];
}



void renderer_cool::reshape_graphics()
{
    float tsx = (float)size_x / gps->dimx, tsy = (float)size_y / gps->dimy;

    //int32_t w = gps->dimx, h = gps->dimy;

    int cx = *df::global::window_x + gdimx / 2;
    int cy = *df::global::window_y + gdimy / 2;

    df::viewscreen *ws = df::global::gview->view.child;
    if (df::viewscreen_dwarfmodest::_identity.is_direct_instance(ws))
    {
        gsize_x = (size_x - tsx * (gmenu_w + 1 + 1));
        gsize_y = (size_y - tsy * 2);
        goff_x = off_x + roundf(tsx);
        goff_y = off_y + roundf(tsy);        
    }
    else //Adv. mode
    {
        // *out2 << "reshape_graphics" << std::endl;
        gsize_x = size_x;
        gsize_y = size_y;
        goff_x = off_x;
        goff_y = goff_y_gl = off_y;
    }

    float _dimx = std::min(gsize_x / gdispx, 256.0f);
    float _dimy = std::min(gsize_y / gdispy, 256.0f); //*3 for oblique
    gdimx = ceilf(_dimx);
    gdimy = ceilf(_dimy);
    gdimxfull = floorf(_dimx);
    gdimyfull = floorf(_dimy);

    if (df::viewscreen_dwarfmodest::_identity.is_direct_instance(ws))
        goff_y_gl = goff_y - (gdimy == gdimyfull ? 0 : roundf(gdispy - (gsize_y - gdispy * gdimyfull)));                

    *df::global::window_x = std::max(0, cx - gdimx / 2);
    *df::global::window_y = std::max(0, cy - gdimy / 2);

    int tiles = gdimx * gdimy;

    init_buffers_and_coords(tiles, gdimy + 1);
    
    needs_full_update = true;
}

void renderer_cool::reshape_gl()
{
    reshape_gl_old();

    tdimx = gps->dimx;
    tdimy = gps->dimy;

    static int last_fullscreen = -1;
    if (last_fullscreen != enabler->fullscreen)
    {
        last_fullscreen = enabler->fullscreen;
        map_texpos = tilesets[0].small_texpos;
        text_texpos = tilesets[1].small_texpos;

        if (!gdispx)
            gdispx = small_map_dispx, gdispy = small_map_dispy;
    }

    reshape_graphics();

    glShadeModel(GL_FLAT);    
}

void renderer_cool::draw(int vertex_count)
{
    static bool initial_resize = false;
    if (!initial_resize)
    {
        if (enabler->fullscreen)
            resize(size_x, size_y);
        else
            resize((size_x/init->font.small_font_dispx)*init->font.small_font_dispx, (size_y/init->font.small_font_dispy)*init->font.small_font_dispy);
            //resize(gps->dimx*init->font.small_font_dispx, gps->dimy*init->font.small_font_dispy);

        reshape_gl();
        initial_resize = true;
    }

    display_new(screen_map_type);

#ifdef WIN32
    // We can't do this in plugin_init() because OpenGL context isn't initialized by that time
    static bool glew_init = false;
    if (!glew_init)
    {
        GLenum err = glewInit();
        if (err != GLEW_OK)
            *out2 << glewGetErrorString(err);
        glew_init = true;
    }
#endif

    static int old_dimx, old_dimy, old_winx, old_winy;
    if (domapshot)
    {
        if (domapshot == 10)
        {
            old_dimx = gps->dimx;
            old_dimy = gps->dimy;
            old_winx = *df::global::window_x;
            old_winy = *df::global::window_y;

            goff_x = goff_y = goff_y_gl = 0;
            gdimx = world->map.x_count;
            gdimy = world->map.y_count;
            *df::global::window_x = 0;
            *df::global::window_y = 0;

            int tiles = gdimx * gdimy;
            init_buffers_and_coords(tiles, gdimy + 1);

            needs_full_update = true;     
        }
        domapshot--;
    }

    static GLuint framebuffer, renderbuffer;
    //GLenum status;
    if (domapshot == 5)
    {
        // Set the width and height appropriately for your image
        GLuint imageWidth = gdimx * gdispx,
               imageHeight = gdimy * gdispy;
        //Set up a FBO with one renderbuffer attachment
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, imageWidth, imageHeight);


        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
        
        glViewport(0, 0, gdimx * gdispx, gdimy * gdispy);
    }

    if (screen_map_type)
    {
        bool skip = false;
        if (df::viewscreen_dungeonmodest::_identity.is_direct_instance(Gui::getCurViewscreen()))
        {
            int m = df::global::ui_advmode->menu;
            bool tmode = advmode_needs_map(m);
            skip = !tmode;
        }

        if (!skip)
        {
            /////
            glViewport(goff_x, goff_y_gl, gdimx * gdispx, gdimy * gdispy);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, gdimx, gdimy, 0, -1, 1);
            //glTranslatef(1,-1,0);

            //glScissor(off_x+(float)size_x/gps->dimx, off_y+(float)size_y/gps->dimy, gsize_x, gsize_y);
            //glEnable(GL_SCISSOR_TEST);
            //glClearColor(1,0,0,1);
            //glClear(GL_COLOR_BUFFER_BIT);

            glClearColor(enabler->ccolor[0][0], enabler->ccolor[0][1], enabler->ccolor[0][2], 1);
            glClear(GL_COLOR_BUFFER_BIT);            

            if (multi_rendered && fogdensity > 0)
            {
                glEnable(GL_FOG);
                glFogfv(GL_FOG_COLOR, fogcolor);
                glFogf(GL_FOG_DENSITY, fogdensity);
                glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD);
                glEnableClientState(GL_FOG_COORD_ARRAY);
                glFogCoordPointer(GL_FLOAT, 0, fogcoord);
            }

            glVertexPointer(2, GL_FLOAT, 0, gvertexes);

            // Render map tiles
            glEnable(GL_TEXTURE_2D);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glTexCoordPointer(2, GL_FLOAT, 0, gtex);
            glColorPointer(4, GL_FLOAT, 0, gfg);
            glDrawArrays(GL_TRIANGLES, 0, gdimx * gdimy * 6 * layer_count); //last = nr of layers

            if (multi_rendered)
            {
                glDisableClientState(GL_FOG_COORD_ARRAY);
                glDisable(GL_FOG);
            }

            // Prepare and render shadows
            if (multi_rendered && shadowsloaded)
            {
                int elemcnt = 0;
                //TODO: don't do this if view not moved and tiles with shadows not changed
                {
                    gl_texpos *txt = (gl_texpos *) enabler->textures.gl_texpos;
                    int x1 = std::min(gdimx, world->map.x_count-gwindow_x);                
                    int y1 = std::min(gdimy, world->map.y_count-gwindow_y);

                    for (int tile = 0; tile < gdimx * gdimy; tile++)
                    {
                        int xx = tile / gdimy;
                        int yy = tile % gdimy;

                        int d = depth[tile];
                        if (d && d != 0x7f) //TODO: no need for the second check in fort mode
                        {
                            GLfloat *tex = shadowtex + elemcnt * 2;

                            bool top = false, left = false, btm = false, right = false;
                            if (xx > 0 && (depth[((xx - 1)*gdimy + yy)]) < d)
                            {
									//tile needs to be multiplied by 8 (layer count), everything else can remain unchanged
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float)); 
                                SETTEX(shadow_texpos[0]);
                                elemcnt += 6; //doesn't relate to layers but to *6 multiplier
                                left = true;
                            }
                            if (yy < y1 - 1 && (depth[((xx)*gdimy + yy + 1)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[1]);
                                elemcnt += 6;
                                btm = true;
                            }
                            if (yy > 0 && (depth[((xx)*gdimy + yy - 1)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[2]);
                                elemcnt += 6;
                                top = true;
                            }
                            if (xx < x1-1 && (depth[((xx + 1)*gdimy + yy)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[3]);
                                elemcnt += 6;
                                right = true;
                            }

                            if (!right && !btm && xx < x1 - 1 && yy < y1 - 1 && (depth[((xx + 1)*gdimy + yy + 1)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[4]);
                                elemcnt += 6;
                            }
                            if (!left && !btm && xx > 0 && yy < y1 - 1 && (depth[((xx - 1)*gdimy + yy + 1)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[5]);
                                elemcnt += 6;
                            }
                            if (!left && !top && xx > 0 && yy > 0 && (depth[((xx - 1)*gdimy + yy - 1)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[6]);
                                elemcnt += 6;
                            }
                            if (!top && !right && xx < x1 - 1 && yy > 0 && (depth[((xx + 1)*gdimy + yy - 1)]) < d)
                            {
                                memcpy(shadowvert + elemcnt * 2, gvertexes + tile*layer_count * 6 * 2, 6 * 2 * sizeof(float));
                                SETTEX(shadow_texpos[7]);
                                elemcnt += 6;
                            }
                        }
                    }
                }

                if (elemcnt)
                {
                    glDisableClientState(GL_COLOR_ARRAY);
                    glColor4fv(shadowcolor);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    //glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
                    glTexCoordPointer(2, GL_FLOAT, 0, shadowtex);
                    glVertexPointer(2, GL_FLOAT, 0, shadowvert);
                    glDrawArrays(GL_TRIANGLES, 0, elemcnt);
                    glEnableClientState(GL_COLOR_ARRAY);
                }
            }

            glDisable(GL_SCISSOR_TEST);

            glViewport(off_x, off_y, size_x, size_y);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, tdimx, tdimy, 0, -1, 1);            
        }
    }

    if (!domapshot)    
    {
        glVertexPointer(2, GL_FLOAT, 0, vertexes);

        // Render background colors
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnable(GL_BLEND);
        glColorPointer(4, GL_FLOAT, 0, bg);
        glDrawArrays(GL_TRIANGLES, 0, tdimx*tdimy*6);//vertex count?

        // Render foreground
        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glTexCoordPointer(2, GL_FLOAT, 0, tex);
        glColorPointer(4, GL_FLOAT, 0, fg);
        glDrawArrays(GL_TRIANGLES, 0, tdimx*tdimy*6);
    }

    if (domapshot == 1)
    {
        int w = gdimx * gdispx;
        int h = gdimy * gdispy;
        
        png::image<png::rgb_pixel, png::solid_pixel_buffer_rev<png::rgb_pixel> > image(w, h);

        unsigned char *data = (unsigned char*)&image.get_pixbuf().get_bytes().at(0);

        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

        try
        {
            image.write(mapshotname);
            *out2 << "Saved a " << w << "x" << h << " image to " << mapshotname << " in DF folder." << std::endl;	
        }
        catch(...)
        {
            *out2 << "Failed to write to mapshot.png in DF folder." << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &renderbuffer);

        grid_resize(old_dimx, old_dimy);
        *df::global::window_x = old_winx;
        *df::global::window_y = old_winy;
        gps->force_full_display_count = 1;
        domapshot = 0;
    }
}

void renderer_cool::display_new(bool update_graphics)
{
#ifndef NO_DISPLAY_PATCH
        // In this case this function replaces original (non-virtual) renderer::display()
        // So update text tiles here

    if (gps->force_full_display_count)
    {
        update_all();
    }
    else
    {
        uint32_t *screenp = (uint32_t*)screen, *oldp = (uint32_t*)screen_old;
        for (int x2=0; x2 < tdimx; ++x2)
        {
            for (int y2=0; y2 < tdimy; ++y2, ++screenp, ++oldp)
            {
                if (*screenp != *oldp)
                    update_tile(x2, y2);
            }
        }
    }

    if (gps->force_full_display_count > 0) gps->force_full_display_count--;
#endif

    // Update map tiles if current screen has a map
    if (update_graphics)
        display_map();
} 

void renderer_cool::display_map()
{
    //trash the previous map cache, where materials, etc, are stored.
    if (map_cache)
    {
        map_cache->trash();

        //If the map size has changed, get rid of it entirely.
        unsigned int x_bmax, y_bmax, z_max;
        Maps::getSize(x_bmax, y_bmax, z_max);
        if (map_cache->maxBlockX() != x_bmax || map_cache->maxBlockY() != y_bmax || map_cache->maxZ() != z_max)
        {
            delete(map_cache);
            map_cache = NULL;
        }
    }
    if (needs_full_update || always_full_update || (lastz != gwindow_z))
    {
        uint32_t *gscreenp = (uint32_t*)gscreen, *goldp = (uint32_t*)gscreen_old;
        needs_full_update = false;
        //clock_t c1 = clock();
        last_advmode_tick = -1; //forces vermin to update once
        for (int x2 = 0; x2 < gdimx; x2++)
            for (int y2 = 0; y2 < gdimy; y2++)
                update_map_tile2(x2, y2);
        //clock_t c2 = clock();
        //*out2 << (c2-c1) << std::endl;
        lastz = gwindow_z;//forces an update on z-level change
    }
    else
    {
        uint32_t *gscreenp = (uint32_t*)gscreen, *goldp = (uint32_t*)gscreen_old;
        int off = 0;
        for (int x2=0; x2 < gdimx; x2++) {
            for (int y2=0; y2 < gdimy; y2++, ++off, ++gscreenp, ++goldp) {
                // We don't use pointers for the non-screen arrays because we mostly fail at the
                // *first* comparison, and having pointers for the others would exceed register
                // count.
                if (*gscreenp == *goldp &&
                #ifdef unittracking 
					unit_tracker[off >> 3] & (1 << (off & 7)) &&
				#endif
                gscreentexpos[off] == gscreentexpos_old[off] &&
                gscreentexpos_addcolor[off] == gscreentexpos_addcolor_old[off] &&
                gscreentexpos_grayscale[off] == gscreentexpos_grayscale_old[off] &&
                gscreentexpos_cf[off] == gscreentexpos_cf_old[off] &&
                gscreentexpos_cbr[off] == gscreentexpos_cbr_old[off])
                    ;
                else
                    update_map_tile2(x2, y2);
            }
        }
    }
}

void renderer_cool::gswap_arrays()
{
    static int j = 0;

    this->gscreen = ::gscreen = _gscreen[j];
    this->gscreentexpos = ::gscreentexpos = _gscreentexpos[j];
    gscreentexpos_addcolor = _gscreentexpos_addcolor[j];
    gscreentexpos_grayscale = _gscreentexpos_grayscale[j];
    gscreentexpos_cf = _gscreentexpos_cf[j];
    gscreentexpos_cbr = _gscreentexpos_cbr[j];

    j ^= 1;

    gscreen_old = _gscreen[j];
    gscreentexpos_old = _gscreentexpos[j];
    gscreentexpos_addcolor_old = _gscreentexpos_addcolor[j];
    gscreentexpos_grayscale_old = _gscreentexpos_grayscale[j];
    gscreentexpos_cf_old = _gscreentexpos_cf[j];
    gscreentexpos_cbr_old = _gscreentexpos_cbr[j];
}

	//used if, say, screen is resized
void renderer_cool::allocate_buffers(int tiles, int extra_tiles)
{
#define REALLOC(var,type,count) var = (type*)realloc(var, (count) * sizeof(type));

    REALLOC(gscreen_origin,                 uint8_t, (tiles+extra_tiles) * 4 * 2)
    REALLOC(gscreentexpos_origin,           int32_t,    (tiles+extra_tiles) * 2);
    REALLOC(gscreentexpos_addcolor_origin,  int8_t,  (tiles+extra_tiles) * 2);
    REALLOC(gscreentexpos_grayscale_origin, uint8_t, (tiles+extra_tiles) * 2);
    REALLOC(gscreentexpos_cf_origin,        uint8_t, (tiles+extra_tiles) * 2);
    REALLOC(gscreentexpos_cbr_origin,       uint8_t, (tiles+extra_tiles) * 2);

    REALLOC(gscreen_under,                  uint8_t, tiles * 4);
    REALLOC(mscreen_under,                  uint8_t, tiles * 4);
    
    REALLOC(gscreen_building,                  uint8_t, tiles * 4);
    REALLOC(mscreen_building,                  uint8_t, tiles * 4);
    #ifdef unittracking 
		REALLOC(unit_tracker,                  uint8_t, tiles);
	#endif

    _gscreen[0]                 = gscreen_origin                 + extra_tiles * 4;
    _gscreentexpos[0]           = gscreentexpos_origin           + extra_tiles;
    _gscreentexpos_addcolor[0]  = gscreentexpos_addcolor_origin  + extra_tiles;
    _gscreentexpos_grayscale[0] = gscreentexpos_grayscale_origin + extra_tiles;
    _gscreentexpos_cf[0]        = gscreentexpos_cf_origin        + extra_tiles;
    _gscreentexpos_cbr[0]       = gscreentexpos_cbr_origin       + extra_tiles;

    _gscreen[1]                 = gscreen_origin                 + (extra_tiles * 2 + tiles) * 4;
    _gscreentexpos[1]           = gscreentexpos_origin           + extra_tiles * 2 + tiles;
    _gscreentexpos_addcolor[1]  = gscreentexpos_addcolor_origin  + extra_tiles * 2 + tiles;
    _gscreentexpos_grayscale[1] = gscreentexpos_grayscale_origin + extra_tiles * 2 + tiles;
    _gscreentexpos_cf[1]        = gscreentexpos_cf_origin        + extra_tiles * 2 + tiles;
    _gscreentexpos_cbr[1]       = gscreentexpos_cbr_origin       + extra_tiles * 2 + tiles;

    gswap_arrays();
    #ifdef unittracking 
    int ttexpos = 0;
    int gtexpos = 0;
    int otexpos = 0;
    int ftexpos = 0;
    int ltexpos = 0;
    for (int t = 0; t < tiles; t++)
    {
		if(_gscreentexpos[0][t])
			unit_tracker[t>>3] |= (1 <<(t&7)); //bitmap saving for bool
		if(this->gscreentexpos[t])//0 on maximizing
			ttexpos= 1;
		if(::gscreentexpos[t])//0 on maximizing
			gtexpos= 1;
		if(gscreentexpos_old[t])//1 on maximizing
			otexpos= 1;
		if(_gscreentexpos[0][t])//1 on maximizing
			ftexpos= 1;
		if(_gscreentexpos[1][t])//0 on maximizing
			ltexpos= 1;
	}
	if(well_info_count>0){well_info_count--; 
		*out2 << "TWBT detects t " << ttexpos << " g " << gtexpos << " o " << otexpos << " f " << ftexpos << " l " << ltexpos << std::endl;
		}
	#endif
	
    //TODO: don't allocate arrays below if multilevel rendering is not enabled
    //TODO: calculate maximum possible number of shadows
    REALLOC(depth,      int8_t,  tiles)
    REALLOC(shadowtex,  GLfloat, tiles * 4 * texpos_mult)
    REALLOC(shadowvert, GLfloat, tiles * 4 * 2 * 6) //vertex count?
    REALLOC(fogcoord,   GLfloat, tiles * 6 * layer_count)	//last = nr of layers...but whyyyy And maybe not? TODO check

    REALLOC(mscreen_origin,                 uint8_t, (tiles+extra_tiles) * 4)
    REALLOC(mscreentexpos_origin,           int32_t,    tiles+extra_tiles);
    REALLOC(mscreentexpos_addcolor_origin,  int8_t,  tiles+extra_tiles);
    REALLOC(mscreentexpos_grayscale_origin, uint8_t, tiles+extra_tiles);
    REALLOC(mscreentexpos_cf_origin,        uint8_t, tiles+extra_tiles);
    REALLOC(mscreentexpos_cbr_origin,       uint8_t, tiles+extra_tiles);

    mscreen                 = mscreen_origin                     + extra_tiles * 4;
    mscreentexpos           = mscreentexpos_origin               + extra_tiles;
    mscreentexpos_addcolor  = mscreentexpos_addcolor_origin      + extra_tiles;
    mscreentexpos_grayscale = mscreentexpos_grayscale_origin     + extra_tiles;
    mscreentexpos_cf        = mscreentexpos_cf_origin            + extra_tiles;
    mscreentexpos_cbr       = mscreentexpos_cbr_origin           + extra_tiles;

    // We need to zero out these buffers because game doesn't change them for tiles without creatures,
    // so there will be garbage that will cause every tile to be updated each frame and other bad things
    memset(_gscreen[0],                 0, (tiles * 2 + extra_tiles) * 4);
    memset(_gscreentexpos[0],           0, (tiles * 2 + extra_tiles) * sizeof(int32_t));
    memset(_gscreentexpos_addcolor[0],  0, tiles * 2 + extra_tiles);
    memset(_gscreentexpos_grayscale[0], 0, tiles * 2 + extra_tiles);
    memset(_gscreentexpos_cf[0],        0, tiles * 2 + extra_tiles);
    memset(_gscreentexpos_cbr[0],       0, tiles * 2 + extra_tiles);
    memset(mscreentexpos,               0, tiles * sizeof(int32_t));
}

void renderer_cool::init_buffers_and_coords(int tiles, int extra_tiles)
{
    // Recreate tile buffers
    allocate_buffers(tiles, gdimy + 1);

    // Recreate OpenGL buffers
    gvertexes = (GLfloat*)realloc(gvertexes, sizeof(GLfloat) * tiles * texpos_mult * layer_count);
    gfg = (GLfloat*)realloc(gfg, sizeof(GLfloat) * tiles * color_mult * layer_count); 
    gtex = (GLfloat*)realloc(gtex, sizeof(GLfloat) * tiles * texpos_mult * layer_count); //last = nr of layers

    // Initialise vertex coords
    //REDO if layer count changes
    int tile = 0;   
    for (GLfloat x = 0; x < gdimx; x++)
    {
        for (GLfloat y = 0; y < gdimy; y++, tile++)
        { //nr of layers vertex calls
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+0), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+1), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+2), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+3), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+4), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+5), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+6), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+7), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+8), 0);
            write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+9), 0);
           // write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+10), 0);
         //   write_tile_vertexes(x, y, gvertexes + 6 * 2 * (tile*layer_count+11), 0);
        }
    }
}

void renderer_cool::reshape_zoom_swap()
{
    static int game_mode = 3;
    if (game_mode != *df::global::gamemode)
    {
        needs_reshape = true;
        game_mode = *df::global::gamemode;
    }

    if (needs_reshape)
    {
        if (needs_zoom)
        {
            if (needs_zoom > 0)
            {
                gdispx += needs_zoom;
                gdispy += needs_zoom;
                //gdispy = gdispx*2.5; // for oblique
            }
            else if (gdispx > 1 && gdispy > 1 && (gdimxfull < world->map.x_count || gdimyfull < world->map.y_count))
            {
                gdispx += needs_zoom;
                gdispy += needs_zoom;
                //gdispy = gdispx*2.5; // for oblique
            }
        
            needs_zoom = 0;
        }
        
        needs_reshape = false;
        reshape_graphics();
        gps->force_full_display_count = 1;
        DFHack::Gui::getCurViewscreen()->resize(gps->dimx, gps->dimy);
        
        //clearing inaccurate buffers
        needs_full_update=true;
        
    }
    else
        gswap_arrays();
}

void renderer_cool::zoom(df::zoom_commands cmd)
{
    if (!screen_map_type)
    {
        zoom_old(cmd);
        return;
    }

    if (cmd == df::zoom_commands::zoom_in)
    {
        gdispx++;
        gdispy++;
        //gdispy = gdispx*2.5; // for oblique
    }
    else if (cmd == df::zoom_commands::zoom_out)
    {
        if (gdispx > 1 && gdispy > 1 && (gdimxfull < world->map.x_count || gdimyfull < world->map.y_count))
        {
            gdispx--;
            gdispy--;
            //gdispy = gdispx*2.5; // for oblique
        }
    }
    else if (cmd == df::zoom_commands::zoom_reset)
    {
        gdispx = small_map_dispx;
        gdispy = small_map_dispy;
    }
    else
    {
        zoom_old(cmd);
        return;
    }

    needs_reshape = true;
}

extern "C" {
    uint8_t SDL_GetMouseState(int *x, int *y);
}

bool renderer_cool::get_mouse_coords(int32_t *x, int32_t *y)
{
    if (!screen_map_type)
        return get_mouse_coords_old(x, y);

    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y); //actual pixels, divided with gdispx from .init.font to get tilenumber   
    
    mouse_x -= goff_x;//To put mouse 0,0 at DF's 0,0
    mouse_y -= goff_y;

    int _x = (float) mouse_x / gdispx + 1;
    int _y = (float) mouse_y / gdispy + 1;

    if (_x < 1 || _y < 1 || _x > gdimx || _y > gdimy)
    #ifdef oldmouse
        return get_mouse_coords_old(x, y); //Fix to show mouse coords instead of -1/-1 for DF
    #else
        return false;
    #endif

    *x = _x;
    *y = _y;
    
    return true;
}
