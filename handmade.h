#ifndef HANDMADE_H
#define HANDMADE_H

#include "handmade_platform.h"
#include "handmade_tile.h"

struct MemoryArena {
    size_t size;
    u8 *base;
    size_t used;
};

struct World {
    TileMap *tile_map;
};

struct LoadedBitmap {
    s32 width;
    s32 height;
    u32 *pixels;
};

struct HeroBitmaps {
    s32 align_x;
    s32 align_y;
    LoadedBitmap head;
    LoadedBitmap cape;
    LoadedBitmap torso;
};

struct GameState {
    MemoryArena world_arena;
    World *world;

    TileMapPosition player_p;
    TileMapPosition camera_p;

    LoadedBitmap backdrop;
    u32 hero_facing_direction;
    HeroBitmaps hero_bitmaps[4];
};

internal void initialize_arena(MemoryArena *arena, size_t size, u8 *base) {
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define push_struct(arena, type) (type *)push_struct_(arena, sizeof(type))
#define push_array(arena, count, type) (type *)push_struct_(arena, (count) * sizeof(type))
internal void *push_struct_(MemoryArena *arena, size_t size) {
    assert((arena->used + size) <= arena->size);
    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

#endif
