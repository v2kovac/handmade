#pragma once

#include "handmade_platform.h"
#include "handmade_math.h"
#include "handmade_world.h"

#define min(a, b) ((a < b) ? (a) : (b))
#define max(a, b) ((a > b) ? (a) : (b))

struct MemoryArena {
    size_t size;
    u8* base;
    size_t used;
};

struct LoadedBitmap {
    s32 width;
    s32 height;
    u32* pixels;
};

struct HeroBitmaps {
    s32 align_x;
    s32 align_y;
    LoadedBitmap head;
    LoadedBitmap cape;
    LoadedBitmap torso;
};

enum EntityType {
    ET_NULL,
    ET_HERO,
    ET_WALL,
};

struct HighEntity {
    v2 p; // relative to camera
    v2 dp;
    u32 facing_direction;
    u32 chunk_z;

    f32 z;
    f32 dz;

    u32 low_entity_index;
};

struct LowEntity {
    EntityType type;
    WorldPosition p;
    f32 width, height;

    // this is for stairs
    bool collides;
    s32 d_abs_tile_z;

    u32 high_entity_index;
};

struct Entity {
    u32 low_index;
    HighEntity* high;
    LowEntity* low;
};

struct LowEntityChunkReference {
    WorldChunk* tile_chunk;
    u32 entity_index_in_chunk;
};

struct GameState {
    MemoryArena world_arena;
    World* world;

    u32 camera_following_entity_index;
    WorldPosition camera_p;

    u32 player_index_for_controller[array_count(((GameInput*)0)->controllers)];

    u32 high_entity_count;
    HighEntity high_entities_[256];

    u32 low_entity_count;
    LowEntity low_entities[100000];

    LoadedBitmap backdrop;
    LoadedBitmap shadow;
    HeroBitmaps hero_bitmaps[4];
};

internal void initialize_arena(MemoryArena* arena, size_t size, u8* base) {
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define push_struct(arena, type) (type*)push_struct_(arena, sizeof(type))
#define push_array(arena, count, type) (type*)push_struct_(arena, (count) * sizeof(type))
internal void* push_struct_(MemoryArena* arena, size_t size) {
    assert((arena->used + size) <= arena->size);
    void* result = arena->base + arena->used;
    arena->used += size;
    return result;
}
