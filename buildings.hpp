#include <df/building_bedst.h>
#include <df/building_doorst.h>
#include <df/building_workshopst.h>

#define DEFINE_VARS(id) \
static bool id##_loaded; \
static int id##_frames; \
static long *id##_tiles;

#define LOAD_BEGIN \
static bool loaded = false; \
if (!loaded) \
{ \
    bool ok = false;

#define LOAD_END \
    if (!ok) \
    { \
        unhook(); \
        return; \
    } \
    loaded = true; \
}

#define LOAD_IMAGE(id,fn) \
    id##_loaded = load_tiles("data/art/tiles/" fn ".png", &id##_tiles, &id##_frames, bldw, bldh); \
    ok |= id##_loaded;

#define IS_LOADED(id) id##_loaded

#define DEFINE_TICK \
unsigned int tick = df::global::enabler->gputicks.value / 10;

#define TICK tick

#define DEFINE_SIZE(w,h) \
const int bldw = w, bldh = h;

#define FILL_PLACEHOLDER(fg,br) \
    for (int i = 0; i < bldw; i++) \
        for (int j = 0; j < bldh; j++) \
        { \
            dbuf->fore[i][j] = fg; \
            dbuf->bright[i][j] = br; \
            dbuf->back[i][j] = 0; \
            dbuf->tile[i][j] = 0xfe + (TICK%2); \
        }

#define DRAW_IMAGE(id) \
    for (int i = 0; i < bldw; i++) \
        for (int j = 0; j < bldh; j++) \
            gscreen_over[(dbuf->x1+i)*256 + dbuf->y1 + j] = id##_tiles[((TICK % id##_frames) + j*id##_frames) * bldw + i];


bool load_tiles(const char *fn, long **tiles, int *frames, int w, int h)
{
    long dx, dy;
    DFSDL_Surface *surf = IMG_Load(fn);
    if (!surf)
        return false;

    if (surf->w % surf->h != 0)
    {
        *out2 << "invalid size " << fn << std::endl;
        return false;
    }

    *frames = surf->w / surf->h;
    *tiles = (long*) malloc(sizeof(long)*(*frames));

    *out2 << surf->w << " " << surf->h << std::endl;
    load_tileset(fn, *tiles, *frames*w, h, &dx, &dy);

    SDL_FreeSurface(surf);    

    return true;
}

//INTERPOSE_NEXT(drawBuilding)(dbuf, smth);

#define OVR_BEGIN(cls,code) \
struct cls##_twbt : public df::cls \
{ \
    typedef df::cls interpose_base; \
\
    void unhook() { \
        INTERPOSE_HOOK(cls##_twbt, drawBuilding).apply(false); \
    } \
    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer* dbuf, int16_t smth)) \
    { \
        DEFINE_TICK \
        code \
    } \
}; \
IMPLEMENT_VMETHOD_INTERPOSE(cls##_twbt, drawBuilding);

#define OVR_END(cls) \
    } \
}; \
IMPLEMENT_VMETHOD_INTERPOSE(cls##_twbt, drawBuilding);

OVR_BEGIN(building_doorst,
{
    DEFINE_SIZE(1,1)
    DEFINE_VARS(normal)
    DEFINE_VARS(locked)

    LOAD_BEGIN
    LOAD_IMAGE(normal, "door")
    LOAD_IMAGE(locked, "door-locked")
    LOAD_END

    MaterialInfo material;
    material.decode(this->mat_type, this->mat_index);
    int fg = material.material->basic_color[0];
    int br = material.material->basic_color[1];

    FILL_PLACEHOLDER(fg,br)

    if (IS_LOADED(locked) && this->door_flags.bits.forbidden)
        DRAW_IMAGE(locked)
    else
        DRAW_IMAGE(normal)
})


OVR_BEGIN(building_workshopst,
{
    DEFINE_SIZE(3,3)
    DEFINE_VARS(normal)

    LOAD_BEGIN
    LOAD_IMAGE(normal, "workshop")
    LOAD_END

    MaterialInfo material;
    material.decode(this->mat_type, this->mat_index);
    int fg = material.material->basic_color[0];
    int br = material.material->basic_color[1];

    FILL_PLACEHOLDER(fg,br)

    DRAW_IMAGE(normal)
})