#include "handmade_tile.h"

#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

internal inline TileChunk* get_tile_chunk(TileMap* tile_map, s32 tile_chunk_x, s32 tile_chunk_y, s32 tile_chunk_z,
                                          MemoryArena* arena = 0)
{
    assert(tile_chunk_x > -TILE_CHUNK_SAFE_MARGIN);
    assert(tile_chunk_y > -TILE_CHUNK_SAFE_MARGIN);
    assert(tile_chunk_z > -TILE_CHUNK_SAFE_MARGIN);
    assert(tile_chunk_x < TILE_CHUNK_SAFE_MARGIN);
    assert(tile_chunk_y < TILE_CHUNK_SAFE_MARGIN);
    assert(tile_chunk_z < TILE_CHUNK_SAFE_MARGIN);

    u32 hash_value = 19*tile_chunk_x + 7*tile_chunk_y + 3*tile_chunk_z;
    u32 hash_slot = hash_value & (array_count(tile_map->tile_chunk_hash) - 1);
    assert(hash_slot < array_count(tile_map->tile_chunk_hash));
    TileChunk* chunk = tile_map->tile_chunk_hash + hash_slot;

    for(;;) {
        // found
        if (tile_chunk_x == chunk->tile_chunk_x &&
            tile_chunk_y == chunk->tile_chunk_y &&
            tile_chunk_z == chunk->tile_chunk_z)
        {
            return chunk;
        }

        if (arena && !chunk->next_in_hash) {
            if (chunk->tile_chunk_x != TILE_CHUNK_UNINITIALIZED) {
                chunk->next_in_hash = push_struct(arena, TileChunk);
                chunk = chunk->next_in_hash;
            }
            u32 tile_count = tile_map->chunk_dim * tile_map->chunk_dim;
            chunk->tiles = push_array(arena, tile_count, u32);
            for (u32 i = 0; i < tile_count; ++i) chunk->tiles[i] = 1;
            chunk->tile_chunk_x = tile_chunk_x;
            chunk->tile_chunk_y = tile_chunk_y;
            chunk->tile_chunk_z = tile_chunk_z;
            chunk->next_in_hash = 0;
            return chunk;
        } else if (!chunk->next_in_hash) {
            return 0;
        }

        chunk = chunk->next_in_hash;
    }
}

internal inline u32 get_tile_value_unchecked(TileMap* tile_map, TileChunk* tile_chunk, s32 tile_x, s32 tile_y) {
    assert(tile_chunk);
    assert(tile_x < tile_map->chunk_dim);
    assert(tile_y < tile_map->chunk_dim);

    u32 tile_chunk_value = tile_chunk->tiles[tile_y * tile_map->chunk_dim + tile_x];
    return tile_chunk_value;
}

internal inline void set_tile_value_unchecked(TileMap* tile_map, TileChunk* tile_chunk, s32 tile_x, s32 tile_y, u32 tile_value) {
    assert(tile_chunk);
    assert(tile_x < tile_map->chunk_dim);
    assert(tile_y < tile_map->chunk_dim);

    tile_chunk->tiles[tile_y * tile_map->chunk_dim + tile_x] = tile_value;
}

internal inline u32 get_tile_value(TileMap* tile_map, TileChunk* tile_chunk, u32 test_tile_x, u32 test_tile_y) {
    u32 tile_chunk_value = 0;

    if (tile_chunk && tile_chunk->tiles) {
        tile_chunk_value = get_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y);
    }

    return tile_chunk_value;
}

internal inline void set_tile_value(TileMap* tile_map, TileChunk* tile_chunk, u32 test_tile_x, u32 test_tile_y, u32 tile_value) {
    if (tile_chunk && tile_chunk->tiles) {
        set_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y, tile_value);
    }
}

internal inline TileChunkPosition get_chunk_position_for(TileMap* tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    TileChunkPosition result;

    result.tile_chunk_x = abs_tile_x >> tile_map->chunk_shift;
    result.tile_chunk_y = abs_tile_y >> tile_map->chunk_shift;
    result.tile_chunk_z = abs_tile_z;
    result.rel_tile_x = abs_tile_x & tile_map->chunk_mask;
    result.rel_tile_y = abs_tile_y & tile_map->chunk_mask;

    return result;
}

internal inline void recanonicalize_coord(TileMap* tile_map, s32* tile, f32* tile_rel) {
    // tile_map is a taurus

    s32 offset = round_f32_to_s32(*tile_rel / tile_map->tile_side_in_meters);
    *tile += offset;
    *tile_rel -= (offset * tile_map->tile_side_in_meters);

    // TODO fix float point funkiness, this will be replaced dont need it
    assert(*tile_rel >= (-0.5f * tile_map->tile_side_in_meters));
    assert(*tile_rel < (0.5f * tile_map->tile_side_in_meters));
}

internal inline TileMapPosition map_to_tile_space(TileMap* tile_map, TileMapPosition base_pos, v2 offset) {
    TileMapPosition result = base_pos;

    result.offset_ += offset;
    recanonicalize_coord(tile_map, &result.abs_tile_x, &result.offset_.x);
    recanonicalize_coord(tile_map, &result.abs_tile_y, &result.offset_.y);

    return result;
}

internal u32 get_tile_value(TileMap* tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    TileChunkPosition chunk_pos = get_chunk_position_for(tile_map, abs_tile_x, abs_tile_y, abs_tile_z);
    TileChunk* tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y, chunk_pos.tile_chunk_z);
    u32 tile_chunk_value = get_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y);

    return tile_chunk_value;
}

internal u32 get_tile_value(TileMap* tile_map, TileMapPosition pos) {
    u32 tile_chunk_value = get_tile_value(tile_map, pos.abs_tile_x, pos.abs_tile_y, pos.abs_tile_z);
    return tile_chunk_value;
}

internal bool is_tile_value_empty(u32 tile_value) {
    bool result = (tile_value == 1) ||
                  (tile_value == 3) ||
                  (tile_value == 4);
    return result;
}

internal bool is_tile_map_point_empty(TileMap* tile_map, TileMapPosition can_pos) {
    u32 tile_chunk_value = get_tile_value(tile_map, can_pos);
    bool empty = is_tile_value_empty(tile_chunk_value);
    return empty;
}

internal void set_tile_value(MemoryArena* arena, TileMap* tile_map,
                             u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z, u32 tile_value)
{
    TileChunkPosition chunk_pos = get_chunk_position_for(tile_map, abs_tile_x, abs_tile_y, abs_tile_z);
    TileChunk* tile_chunk = get_tile_chunk(tile_map,
                                           chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y, chunk_pos.tile_chunk_z,
                                           arena);
    set_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y, tile_value);
}

internal void initialize_tile_map(TileMap* tile_map, f32 tile_side_in_meters) {
    tile_map->chunk_shift = 4;
    tile_map->chunk_mask = (1 << tile_map->chunk_shift) - 1;
    tile_map->chunk_dim = (1 << tile_map->chunk_shift);
    tile_map->tile_side_in_meters = tile_side_in_meters;

    for (u32 i = 0; i < array_count(tile_map->tile_chunk_hash); ++i) {
        tile_map->tile_chunk_hash[i].tile_chunk_x = TILE_CHUNK_UNINITIALIZED;
        tile_map->tile_chunk_hash[i].next_in_hash = 0;
    }
}

internal inline bool are_on_same_tile(TileMapPosition* a, TileMapPosition* b) {
    bool result = a->abs_tile_x == b->abs_tile_x &&
                  a->abs_tile_y == b->abs_tile_y &&
                  a->abs_tile_z == b->abs_tile_z;
    return result;
}

internal inline TileMapDifference subtract(TileMap* tile_map, TileMapPosition* a, TileMapPosition* b) {
    TileMapDifference result;

    v2 d_tile_xy = { (f32)a->abs_tile_x - (f32)b->abs_tile_x,
                     (f32)a->abs_tile_y - (f32)b->abs_tile_y };
    f32 d_tile_z = (f32)a->abs_tile_z - (f32)b->abs_tile_z;

    result.d_xy = (tile_map->tile_side_in_meters * d_tile_xy) + (a->offset_ - b->offset_);
    result.d_z = (tile_map->tile_side_in_meters * d_tile_z);

    return result;
}

internal inline TileMapPosition centered_tile_point(u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    TileMapPosition result = {};
    result.abs_tile_x = abs_tile_x;
    result.abs_tile_y = abs_tile_y;
    result.abs_tile_z = abs_tile_z;
    return result;
}










