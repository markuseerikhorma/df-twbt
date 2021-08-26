#include "df/building_drawbuffer.h"

#include "df/building_animaltrapst.h"
#include "df/building_archerytargetst.h"
#include "df/building_armorstandst.h"
#include "df/building_axle_horizontalst.h"
#include "df/building_axle_verticalst.h"
#include "df/building_bars_floorst.h"
#include "df/building_bars_verticalst.h"
#include "df/building_bedst.h"
#include "df/building_bookcasest.h"
#include "df/building_boxst.h"
#include "df/building_bridgest.h"
#include "df/building_cabinetst.h"
#include "df/building_cagest.h"
#include "df/building_chainst.h"
#include "df/building_chairst.h"
#include "df/building_civzonest.h"
#include "df/building_coffinst.h"
#include "df/building_constructionst.h"
#include "df/building_doorst.h"
#include "df/building_farmplotst.h"
#include "df/building_floodgatest.h"
#include "df/building_furnacest.h"
#include "df/building_gear_assemblyst.h"
#include "df/building_grate_floorst.h"
#include "df/building_grate_wallst.h"
#include "df/building_hatchst.h"
#include "df/building_hivest.h"
#include "df/building_instrumentst.h"
#include "df/building_nest_boxst.h"
#include "df/building_nestst.h"
#include "df/building_road_dirtst.h"
#include "df/building_road_pavedst.h"
#include "df/building_rollersst.h"
#include "df/building_screw_pumpst.h"
#include "df/building_shopst.h"
#include "df/building_siegeenginest.h"
#include "df/building_slabst.h"
#include "df/building_statuest.h"
#include "df/building_stockpilest.h"
#include "df/building_supportst.h"
#include "df/building_tablest.h"
#include "df/building_traction_benchst.h"
#include "df/building_tradedepotst.h"
#include "df/building_trapst.h"
#include "df/building_wagonst.h"
#include "df/building_water_wheelst.h"
#include "df/building_weaponrackst.h"
#include "df/building_weaponst.h"
#include "df/building_wellst.h"
#include "df/building_windmillst.h"
#include "df/building_window_gemst.h"
#include "df/building_window_glassst.h"
#include "df/building_windowst.h"
#include "df/building_workshopst.h"


//#include <typeinfo>
//		if (once>0) {once--; *out2 << "TWBT building typeid name" << typeid(this).name() << "smth" << (int)zlevel << std::endl;}

//The \ continues the line which is also why there }; at the end
//TODO: apply overwrite here instead, to fix multilevel rendering reverting to old situation.
//But first, triple layering
/*
 * 
 * 
struct cls##_hook : public df::cls \
{ \
   typedef df::cls interpose_base; \
\
    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer* dbuf, int16_t smth)) \
    { \
\
        renderer_cool *r = (renderer_cool*)enabler->renderer; \
\
        int xmax = std::min(dbuf->x2-mwindow_x, r->gdimx-1); \
        int ymax = std::min(dbuf->y2-gwindow_y, r->gdimy-1); \
\
        for (int x = dbuf->x1-mwindow_x; x <= xmax; x++) \
            for (int y = dbuf->y1-gwindow_y; y <= ymax; y++) { \
                if (x >= 0 && y >= 0 && x < r->gdimx && y < r->gdimy){ \
                    if (!((uint32_t*)screen_under_ptr)[x*r->gdimy + y] && !((uint32_t*)screen_building_ptr)[x*r->gdimy + y]){ \
						((uint32_t*)screen_under_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_building_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y]; \
						} else if (((uint32_t*)screen_under_ptr)[x*r->gdimy + y]==((uint32_t*)screen_building_ptr)[x*r->gdimy + y]){ \
							((uint32_t*)screen_building_ptr)[x*r->gdimy + y]=((uint32_t*)screen_ptr)[x*r->gdimy + y]; \
							} \
				}    \
			}    \
\
\
        INTERPOSE_NEXT(drawBuilding)(dbuf, smth); \
\
        for (int x = dbuf->x1-mwindow_x; x <= xmax; x++) \
            for (int y = dbuf->y1-gwindow_y; y <= ymax; y++) { \
                if (x >= 0 && y >= 0 && x < r->gdimx && y < r->gdimy) { \
                    painted_building_map[mwindow_x + x+(gwindow_y + y)*mapx+(gwindow_z - (((*(screen_ptr + (x*r->gdimy + y)*4)+3)&0xf0)>>4))*mapx*mapy] == true ; \
                 }    \
			}    \
\
} \
}; \

drawn_item_map[x+y*mapx+this->z*mapx*mapy]
*/

/*okay, vermin blink issue is following:
 * the code aim to do either building/terrain/terrain or item/building/terrain
 * in the first case, vermin overwrites 1, so it becomes vermin/terrain/terrain
 * L2 doesn't become building because L2 was already set to terrain
 * thankfully I can add hasvermin check 
*/

//its pointless to swap a second time after: the things have exact same value


//okay I can get at tile/fore/back/bright through dbuf, actually. 
//I also realized that swap is useless at this point
//Items are never put below buildings which would be the case if it went I-T-T>TIT>ITT
// ...Though that seems to be true at glance over fort and test with older/newer building than item, not long-term testing
#ifdef oldunittransparency

#define OVER1(cls) \
struct cls##_hook : public df::cls \
{ \
   typedef df::cls interpose_base; \
\
    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer* dbuf, int16_t zlevel)) \
    { \
\
        INTERPOSE_NEXT(drawBuilding)(dbuf, zlevel); \
        renderer_cool *r = (renderer_cool*)enabler->renderer; \
\
        int xmax = std::min(dbuf->x2-mwindow_x, r->gdimx-1); \
        int ymax = std::min(dbuf->y2-gwindow_y, r->gdimy-1); \
\
		if(!made_units_transparent && my_block_index){\
			apply_unitvermin_transparency(r->gdimx, r->gdimy);\
		}\
		uint8_t bx = 0; uint8_t by = 0;\
        for (int x = dbuf->x1-mwindow_x; x <= xmax; x++, bx++){ \
			by = 0;\
            for (int y = dbuf->y1-gwindow_y;  y <= ymax; y++, by++) { \
                if (x >= 0 && y >= 0 && x < r->gdimx && y < r->gdimy){ \
                  int tilez = (dbuf->x1+bx)*mapy*mapzb + (dbuf->y1+by)*mapzb + zlevel; \
                  painted_building_map[tilez] = this->id ; \
\
                  if(dbuf->tile[bx][by]!=32){\
					if (!((uint32_t*)screen_under_ptr)[x*r->gdimy + y]) {((uint32_t*)screen_under_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y];} \
                    if (!((uint32_t*)screen_building_ptr)[x*r->gdimy + y]) {((uint32_t*)screen_building_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y];} else \
                    swap(((uint32_t*)screen_ptr)[x*r->gdimy + y],((uint32_t*)screen_building_ptr)[x*r->gdimy + y]); \
                    buildingbuffer_map[tilez].totalBits = (((dbuf->tile[bx][by]))) + (((uint8_t)(dbuf->fore[bx][by])) << (8)) + (((uint8_t)(dbuf->back[bx][by])) << (16)) + (((((dbuf->bright[bx][by])) & 0x0f) > 0 ? 1 : 0)<< (24));\
				  }\
				}    \
			}    \
		}\
} \
}; \
IMPLEMENT_VMETHOD_INTERPOSE(cls##_hook, drawBuilding);

#else

#define OVER1(cls) \
struct cls##_hook : public df::cls \
{ \
   typedef df::cls interpose_base; \
\
    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer* dbuf, int16_t zlevel)) \
    { \
\
        INTERPOSE_NEXT(drawBuilding)(dbuf, zlevel); \
        renderer_cool *r = (renderer_cool*)enabler->renderer; \
\
        int xmax = std::min(dbuf->x2-mwindow_x, r->gdimx-1); \
        int ymax = std::min(dbuf->y2-gwindow_y, r->gdimy-1); \
\
		if(!made_units_transparent && my_block_index){\
			apply_unitvermin_transparency(r->gdimx, r->gdimy);\
		}\
		uint8_t bx = 0; uint8_t by = 0;\
        for (int x = dbuf->x1-mwindow_x; x <= xmax; x++, bx++){ \
			by = 0;\
            for (int y = dbuf->y1-gwindow_y;  y <= ymax; y++, by++) { \
                if (x >= 0 && y >= 0 && x < r->gdimx && y < r->gdimy){ \
                  int tilez = (dbuf->x1+bx)*mapy*mapzb + (dbuf->y1+by)*mapzb + zlevel; \
                  painted_building_map[tilez] = this->id ; \
\
                  if(dbuf->tile[bx][by]!=32){\
                    swap(((uint32_t*)screen_ptr)[x*r->gdimy + y],((uint32_t*)screen_building_ptr)[x*r->gdimy + y]); \
                    buildingbuffer_map[tilez].totalBits = (((dbuf->tile[bx][by]))) + (((uint8_t)(dbuf->fore[bx][by])) << (8)) + (((uint8_t)(dbuf->back[bx][by])) << (16)) + (((((dbuf->bright[bx][by])) & 0x0f) > 0 ? 1 : 0)<< (24));\
				  }\
				}    \
			}    \
		}\
} \
};\
IMPLEMENT_VMETHOD_INTERPOSE(cls##_hook, drawBuilding);

#endif




/*
 * 

				if(false) *out2 << "TWBT building buffer print" << std::endl;\
        for (int x = dbuf->x1-mwindow_x; x <= xmax; x++) \
            for (int y = dbuf->y1-gwindow_y; y <= ymax; y++) { \
                if (x >= 0 && y >= 0 && x < r->gdimx && y < r->gdimy) { \
					if(false) swap(((uint32_t*)screen_ptr)[x*r->gdimy + y],((uint32_t*)screen_building_ptr)[x*r->gdimy + y]); \
				const unsigned char *s = screen_ptr + (x * r->gdimy + y)*4;\
				int s0 = s[0];\
				int dbuftile = ((*dbuf->tile)[0]);\
				if(false) *out2 << "TWBT post-interpose tile " << s0 << " dbuf->tile[0] is " << dbuftile << std::endl;\
                    painted_building_map[mwindow_x + x+(gwindow_y + y)*mapx+(gwindow_z - (((*(screen_ptr + (x*r->gdimy + y)*4)+3)&0xf0)>>4))*mapx*mapy] == true ; \
                 }    \
			}    \
			* 
			*                     if ( (((*(screen_ptr + (x*r->gdimy + y)*4)+3)&0xf0)>>4)!=0 && once>0) {once--;\
*out2 << "TWBT nonzero gwindow " << gwindow_z << " zoffset " << ((int)(((*(screen_ptr + (x*r->gdimy + y)*4)+3)&0xf0)>>4)) << "building chars " << ((int)buildingbuffer_map[tilez].portions.tile) << " " \
 << ((int)buildingbuffer_map[tilez].portions.fg) <<  " vs actual fore " << (int)(uint8_t)((dbuf->fore[bx][by])) << " " << ((int)buildingbuffer_map[tilez].portions.bg) <<  " " << ((int)buildingbuffer_map[tilez].portions.bold) \
 << " x " << (x+mwindow_x) << " y " << (y+gwindow_y) << " z " << gwindow_z \
 << "db->tile[" << bx << "][" << by << "] " << (int)dbuf->tile[bx][by] \
 << std::endl;} \
 */

#define OVER1_ENABLE(cls) INTERPOSE_HOOK(cls##_hook, drawBuilding).apply(true);

OVER1(building_animaltrapst);
OVER1(building_archerytargetst);
OVER1(building_armorstandst);
OVER1(building_axle_horizontalst);
OVER1(building_axle_verticalst);
OVER1(building_bars_floorst);
OVER1(building_bars_verticalst);
OVER1(building_bedst);
OVER1(building_bookcasest);
OVER1(building_boxst);
OVER1(building_cabinetst);
OVER1(building_cagest);
OVER1(building_chainst);
OVER1(building_chairst);
OVER1(building_coffinst);
OVER1(building_doorst);
OVER1(building_furnacest);
OVER1(building_gear_assemblyst);
OVER1(building_hatchst);
OVER1(building_hivest);
OVER1(building_instrumentst);
OVER1(building_nest_boxst);
OVER1(building_rollersst);
OVER1(building_screw_pumpst);
OVER1(building_siegeenginest);
OVER1(building_slabst);
OVER1(building_statuest);
OVER1(building_tablest);
OVER1(building_supportst);
OVER1(building_traction_benchst);
OVER1(building_tradedepotst);
OVER1(building_trapst);
OVER1(building_weaponrackst);
OVER1(building_weaponst);
//OVER1(building_wellst); //has separate thing
OVER1(building_workshopst);
OVER1(building_grate_wallst);
OVER1(building_grate_floorst);

//missing transparency overrides
//mind, any tile using transparency or override on top of another tile using an override results in the latter's override not being used
//unless the latter is construction-type
//Well, it's exactly that item overrides are looked at before building overrides and it grabs the first of those.
//Any tile using transparency, below another tile using transparency, is not displayed
//I think it might be less transparency and adding ground tile to it


OVER1(building_bridgest); //Bridge corners, possibly grated floor
OVER1(building_civzonest); //Nah, only visible in zone view mode.
OVER1(building_constructionst); //Only briefly visible
OVER1(building_farmplotst); //walking roads between lines; like with windmills plants are going to flash anyway
OVER1(building_floodgatest); //floodgates fill the whole tile; however they can be opened and thus have "opened" override, and also be made of glass
OVER1(building_nestst);
OVER1(building_road_dirtst); //Only briefly + above comment
OVER1(building_road_pavedst); //no mortar?
//OVER1(building_shopst); no longer used
//OVER1(building_stockpilest); There's separate hook for stockpiles below
OVER1(building_wagonst); //See depot
OVER1(building_water_wheelst);
OVER1(building_windmillst);//items flashing on windmill will instead display the floor underneath instead of non-overriden windmill tile. Superior.
OVER1(building_window_gemst);
OVER1(building_window_glassst);
OVER1(building_windowst); //Not gem or glass window? What even is this


struct well_hook : public df::building_wellst
{
   typedef df::building_wellst interpose_base; \
\
    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer* dbuf, int16_t zlevel)) \
    { \
\
        INTERPOSE_NEXT(drawBuilding)(dbuf, zlevel); \
        renderer_cool *r = (renderer_cool*)enabler->renderer; \
        
		if(!made_units_transparent && my_block_index){\
			apply_unitvermin_transparency(r->gdimx, r->gdimy);\
		}\

		int x = dbuf->x1-mwindow_x;
		int y = dbuf->y1-gwindow_y;

		#ifdef oldunittransparency
		if (!((uint32_t*)screen_under_ptr)[x*r->gdimy + y]) {((uint32_t*)screen_under_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y];}
		if (!((uint32_t*)screen_building_ptr)[x*r->gdimy + y]) {((uint32_t*)screen_building_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y];}
		#endif
		swap(((uint32_t*)screen_ptr)[x*r->gdimy + y],((uint32_t*)screen_building_ptr)[x*r->gdimy + y]); \

			int tilez = (dbuf->x1)*mapy*mapzb + (dbuf->y1)*mapzb + zlevel; \
			painted_building_map[tilez] = this->id ; \
			buildingbuffer_map[tilez].totalBits = (((dbuf->tile[0][0]))) + (((uint8_t)(dbuf->fore[0][0])) << (8)) + (((uint8_t)(dbuf->back[0][0])) << (16)) + (((((dbuf->bright[0][0])) & 0x0f) > 0 ? 1 : 0)<< (24));
	/*
	 * Hang on, same logic in both branches? using just one should work
		if (zlevel == this->z) {
			int tilez = (dbuf->x1)*mapy*mapzb + (dbuf->y1)*mapzb + this->z; \
			painted_building_map[tilez] = this->id ; \
			buildingbuffer_map[tilez].totalBits = (((dbuf->tile[0][0]))) + (((uint8_t)(dbuf->fore[0][0])) << (8)) + (((uint8_t)(dbuf->back[0][0])) << (16)) + (((((dbuf->bright[0][0])) & 0x0f) > 0 ? 1 : 0)<< (24));
		}
		
		if(zlevel != this->z) {
			int bucket_tilez = (dbuf->x1)*mapy*mapzb + (dbuf->y1)*mapzb + zlevel;
				painted_building_map[bucket_tilez] = this->id ; \
				buildingbuffer_map[bucket_tilez].totalBits = (((dbuf->tile[0][0]))) + (((uint8_t)(dbuf->fore[0][0])) << (8)) + (((uint8_t)(dbuf->back[0][0])) << (16)) + (((((dbuf->bright[0][0])) & 0x0f) > 0 ? 1 : 0)<< (24));
		}
		*/
		
}
};
IMPLEMENT_VMETHOD_INTERPOSE(well_hook, drawBuilding);


struct stockpile_hook : public df::building_stockpilest
{
   typedef df::building_stockpilest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer* dbuf, int16_t zlevel))
    {
        if (!hide_stockpiles||(df::global::ui->main.mode == df::ui_sidebar_mode::QueryBuilding ||
            df::global::ui->main.mode == df::ui_sidebar_mode::LookAround ||
            df::global::ui->main.mode == df::ui_sidebar_mode::Stockpiles)){
            
        INTERPOSE_NEXT(drawBuilding)(dbuf, zlevel); \
        renderer_cool *r = (renderer_cool*)enabler->renderer; \
\
        int xmax = std::min(dbuf->x2-mwindow_x, r->gdimx-1); \
        int ymax = std::min(dbuf->y2-gwindow_y, r->gdimy-1); \
\
		if(!made_units_transparent && my_block_index){\
			apply_unitvermin_transparency(r->gdimx, r->gdimy);\
		}\
		uint8_t bx = 0; uint8_t by = 0;\
        for (int x = dbuf->x1-mwindow_x; x <= xmax; x++, bx++){ \
			by = 0;\
            for (int y = dbuf->y1-gwindow_y;  y <= ymax; y++, by++) { \
                if (x >= 0 && y >= 0 && x < r->gdimx && y < r->gdimy){ \
                  if(dbuf->tile[bx][by]!=32){
                    #ifdef oldunittransparency
                    if (!((uint32_t*)screen_under_ptr)[x*r->gdimy + y]) {((uint32_t*)screen_under_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y];}
                    if (!((uint32_t*)screen_building_ptr)[x*r->gdimy + y]) {((uint32_t*)screen_building_ptr)[x*r->gdimy + y] = ((uint32_t*)screen_ptr)[x*r->gdimy + y];}
                    #endif
                    swap(((uint32_t*)screen_ptr)[x*r->gdimy + y],((uint32_t*)screen_building_ptr)[x*r->gdimy + y]); \
                    int tilez = (dbuf->x1+bx)*mapy*mapzb + (dbuf->y1+by)*mapzb + zlevel; \
                    painted_building_map[tilez] = this->id ; \
                    buildingbuffer_map[tilez].totalBits = (((dbuf->tile[bx][by]))) + (((uint8_t)(dbuf->fore[bx][by])) << (8)) + (((uint8_t)(dbuf->back[bx][by])) << (16)) + (((((dbuf->bright[bx][by])) & 0x0f) > 0 ? 1 : 0)<< (24));\
				  }\
				}    \
			}    \
		}\
		}
        else
        {
            memset(dbuf->tile, 32, 31*31); // only within standard tileset, 32 is air (empty space/transparent), transparency will use black-coloured tile 0 as bg
            //other buildings may override background, i.e. on loom take overriden tile fg, loom floor override bg.
            //will also apply where extents are 0, i.e. overwriting walls
            //terrain will blink normally, items are placed on top
            //color seems to have something to do with the floor underneath, but is not entirely determined by it
            //memset(dbuf->tile, 33, 1); // sufficient to replace, what is the rest of the range for??
        }
    }
}; 
IMPLEMENT_VMETHOD_INTERPOSE(stockpile_hook, drawBuilding);

void enable_building_hooks()
{
    OVER1_ENABLE(building_animaltrapst);
    OVER1_ENABLE(building_archerytargetst);
    OVER1_ENABLE(building_armorstandst);
    OVER1_ENABLE(building_axle_horizontalst);
    OVER1_ENABLE(building_axle_verticalst);
    OVER1_ENABLE(building_bars_floorst);
    OVER1_ENABLE(building_bars_verticalst);
    OVER1_ENABLE(building_bedst);
    OVER1_ENABLE(building_bookcasest);
    OVER1_ENABLE(building_boxst);
    OVER1_ENABLE(building_cabinetst);
    OVER1_ENABLE(building_cagest);
    OVER1_ENABLE(building_chainst);
    OVER1_ENABLE(building_chairst);    
    OVER1_ENABLE(building_coffinst);
    OVER1_ENABLE(building_doorst);
    OVER1_ENABLE(building_gear_assemblyst);
    OVER1_ENABLE(building_hatchst);
    OVER1_ENABLE(building_hivest);
    OVER1_ENABLE(building_instrumentst);
    OVER1_ENABLE(building_nest_boxst);
    OVER1_ENABLE(building_rollersst);
    OVER1_ENABLE(building_screw_pumpst);
    OVER1_ENABLE(building_siegeenginest);
    OVER1_ENABLE(building_slabst);
    OVER1_ENABLE(building_statuest);
    OVER1_ENABLE(building_tablest);
    OVER1_ENABLE(building_supportst);
    OVER1_ENABLE(building_traction_benchst);
    OVER1_ENABLE(building_tradedepotst);
    OVER1_ENABLE(building_trapst);
    OVER1_ENABLE(building_weaponrackst);
    OVER1_ENABLE(building_weaponst);
//    OVER1_ENABLE(building_wellst);
    INTERPOSE_HOOK(well_hook, drawBuilding).apply(true);

    OVER1_ENABLE(building_grate_floorst);
    OVER1_ENABLE(building_grate_wallst);
    OVER1_ENABLE(building_bridgest);
    OVER1_ENABLE(building_civzonest);
    OVER1_ENABLE(building_constructionst);
    OVER1_ENABLE(building_farmplotst);
    OVER1_ENABLE(building_floodgatest);
    OVER1_ENABLE(building_nestst);
    OVER1_ENABLE(building_road_dirtst);
    OVER1_ENABLE(building_road_pavedst);
    //OVER1_ENABLE(building_shopst);
    //OVER1_ENABLE(building_stockpilest);
    OVER1_ENABLE(building_wagonst);
    OVER1_ENABLE(building_water_wheelst);
    OVER1_ENABLE(building_windmillst);
    OVER1_ENABLE(building_window_gemst);
    OVER1_ENABLE(building_window_glassst);
    OVER1_ENABLE(building_windowst);
    
    INTERPOSE_HOOK(building_furnacest_hook, drawBuilding).apply(workshop_transparency);
    INTERPOSE_HOOK(building_workshopst_hook, drawBuilding).apply(workshop_transparency);

    INTERPOSE_HOOK(stockpile_hook, drawBuilding).apply(true);//.apply(hide_stockpiles);
}
