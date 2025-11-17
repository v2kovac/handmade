#include "handmade_world.h"

#define WORLD_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define WORLD_CHUNK_UNINITIALIZED INT32_MAX

internal inline WorldChunk* get_world_chunk(World* world, s32 chunk_x, s32 chunk_y, s32 chunk_z,
                                          MemoryArena* arena = 0)
{
    assert(chunk_x > -WORLD_CHUNK_SAFE_MARGIN);
    assert(chunk_y > -WORLD_CHUNK_SAFE_MARGIN);
    assert(chunk_z > -WORLD_CHUNK_SAFE_MARGIN);
    assert(chunk_x < WORLD_CHUNK_SAFE_MARGIN);
    assert(chunk_y < WORLD_CHUNK_SAFE_MARGIN);
    assert(chunk_z < WORLD_CHUNK_SAFE_MARGIN);

    u32 hash_value = 19*chunk_x + 7*chunk_y + 3*chunk_z;
    u32 hash_slot = hash_value & (array_count(world->chunk_hash) - 1);
    assert(hash_slot < array_count(world->chunk_hash));
    WorldChunk* chunk = world->chunk_hash + hash_slot;

    for(;;) {
        // found
        if (chunk_x == chunk->chunk_x &&
            chunk_y == chunk->chunk_y &&
            chunk_z == chunk->chunk_z)
        {
            return chunk;
        }

        if (arena && !chunk->next_in_hash) {
            if (chunk->chunk_x != WORLD_CHUNK_UNINITIALIZED) {
                chunk->next_in_hash = push_struct(arena, WorldChunk);
                chunk = chunk->next_in_hash;
            }
            u32 tile_count = world->chunk_dim * world->chunk_dim;
            chunk->chunk_x = chunk_x;
            chunk->chunk_y = chunk_y;
            chunk->chunk_z = chunk_z;
            chunk->next_in_hash = 0;
            return chunk;
        } else if (!chunk->next_in_hash) {
            return 0;
        }

        chunk = chunk->next_in_hash;
    }
}

#if 0
internal inline WorldPosition get_chunk_position_for(World* world, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    WorldChunkPosition result;

    result.chunk_x = abs_tile_x >> world->chunk_shift;
    result.chunk_y = abs_tile_y >> world->chunk_shift;
    result.chunk_z = abs_tile_z;
    result.rel_tile_x = abs_tile_x & world->chunk_mask;
    result.rel_tile_y = abs_tile_y & world->chunk_mask;

    return result;
}
#endif

internal inline void recanonicalize_coord(World* world, s32* tile, f32* tile_rel) {
    s32 offset = round_f32_to_s32(*tile_rel / world->tile_side_in_meters);
    *tile += offset;
    *tile_rel -= (offset * world->tile_side_in_meters);

    // TODO fix float point funkiness, this will be replaced dont need it
    assert(*tile_rel >= (-0.5f * world->tile_side_in_meters));
    assert(*tile_rel < (0.5f * world->tile_side_in_meters));
}

internal inline WorldPosition map_to_tile_space(World* world, WorldPosition base_pos, v2 offset) {
    WorldPosition result = base_pos;

    result.offset_ += offset;
    recanonicalize_coord(world, &result.abs_tile_x, &result.offset_.x);
    recanonicalize_coord(world, &result.abs_tile_y, &result.offset_.y);

    return result;
}

internal void initialize_world(World* world, f32 tile_side_in_meters) {
    world->chunk_shift = 4;
    world->chunk_mask = (1 << world->chunk_shift) - 1;
    world->chunk_dim = (1 << world->chunk_shift);
    world->tile_side_in_meters = tile_side_in_meters;

    for (u32 i = 0; i < array_count(world->chunk_hash); ++i) {
        world->chunk_hash[i].chunk_x = WORLD_CHUNK_UNINITIALIZED;
        world->chunk_hash[i].next_in_hash = 0;
    }
}

internal inline bool are_on_same_tile(WorldPosition* a, WorldPosition* b) {
    bool result = a->abs_tile_x == b->abs_tile_x &&
                  a->abs_tile_y == b->abs_tile_y &&
                  a->abs_tile_z == b->abs_tile_z;
    return result;
}

internal inline WorldDifference subtract(World* world, WorldPosition* a, WorldPosition* b) {
    WorldDifference result;

    v2 d_tile_xy = { (f32)a->abs_tile_x - (f32)b->abs_tile_x,
                     (f32)a->abs_tile_y - (f32)b->abs_tile_y };
    f32 d_tile_z = (f32)a->abs_tile_z - (f32)b->abs_tile_z;

    result.d_xy = (world->tile_side_in_meters * d_tile_xy) + (a->offset_ - b->offset_);
    result.d_z = (world->tile_side_in_meters * d_tile_z);

    return result;
}

internal inline WorldPosition centered_tile_point(u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    WorldPosition result = {};
    result.abs_tile_x = abs_tile_x;
    result.abs_tile_y = abs_tile_y;
    result.abs_tile_z = abs_tile_z;
    return result;
}

