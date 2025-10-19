#include "handmade_tile.h"

inline internal TileChunk *get_tile_chunk(TileMap *tile_map, u32 tile_chunk_x, u32 tile_chunk_y, u32 tile_chunk_z) {
    TileChunk *tile_chunk = NULL;
    if (tile_chunk_x >= 0 && tile_chunk_x < tile_map->tile_chunk_count_x &&
        tile_chunk_y >= 0 && tile_chunk_y < tile_map->tile_chunk_count_y &&
        tile_chunk_z >= 0 && tile_chunk_z < tile_map->tile_chunk_count_z)
    {
        tile_chunk = &tile_map->tile_chunks[
            tile_chunk_z * tile_map->tile_chunk_count_x * tile_map->tile_chunk_count_y +
            tile_chunk_y * tile_map->tile_chunk_count_x +
            tile_chunk_x];
    }
    return tile_chunk;
}

inline internal u32 get_tile_value_unchecked(TileMap *tile_map, TileChunk *tile_chunk, u32 tile_x, u32 tile_y) {
    assert(tile_chunk);
    assert(tile_x < tile_map->chunk_dim);
    assert(tile_y < tile_map->chunk_dim);

    u32 tile_chunk_value = tile_chunk->tiles[tile_y * tile_map->chunk_dim + tile_x];
    return tile_chunk_value;
}

inline internal void set_tile_value_unchecked(TileMap *tile_map, TileChunk *tile_chunk, u32 tile_x, u32 tile_y, u32 tile_value) {
    assert(tile_chunk);
    assert(tile_x < tile_map->chunk_dim);
    assert(tile_y < tile_map->chunk_dim);

    tile_chunk->tiles[tile_y * tile_map->chunk_dim + tile_x] = tile_value;
}

inline internal u32 get_tile_value(TileMap *tile_map, TileChunk *tile_chunk, u32 test_tile_x, u32 test_tile_y) {
    u32 tile_chunk_value = 0;

    if (tile_chunk && tile_chunk->tiles) {
        tile_chunk_value = get_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y);
    }

    return tile_chunk_value;
}

inline internal void set_tile_value(TileMap *tile_map, TileChunk *tile_chunk, u32 test_tile_x, u32 test_tile_y, u32 tile_value) {
    if (tile_chunk && tile_chunk->tiles) {
        set_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y, tile_value);
    }
}

inline internal TileChunkPosition get_chunk_position_for(TileMap *tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    TileChunkPosition result;

    result.tile_chunk_x = abs_tile_x >> tile_map->chunk_shift;
    result.tile_chunk_y = abs_tile_y >> tile_map->chunk_shift;
    result.tile_chunk_z = abs_tile_z;
    result.rel_tile_x = abs_tile_x & tile_map->chunk_mask;
    result.rel_tile_y = abs_tile_y & tile_map->chunk_mask;

    return result;
}

inline internal void recanonicalize_coord(TileMap *tile_map, u32 *tile, f32 *tile_rel) {
    // tile_map is a taurus

    s32 offset = round_f32_to_s32(*tile_rel / tile_map->tile_side_in_meters);
    *tile += offset;
    *tile_rel -= (offset * tile_map->tile_side_in_meters);

    assert(*tile_rel >= (-0.5f * tile_map->tile_side_in_meters));
    assert(*tile_rel < (0.5f * tile_map->tile_side_in_meters));
}

inline internal TileMapPosition recanonicalize_position(TileMap *tile_map, TileMapPosition pos) {
    TileMapPosition result = pos;

    recanonicalize_coord(tile_map, &result.abs_tile_x, &result.offset_x);
    recanonicalize_coord(tile_map, &result.abs_tile_y, &result.offset_y);

    return result;
}

internal u32 get_tile_value(TileMap *tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z) {
    TileChunkPosition chunk_pos = get_chunk_position_for(tile_map, abs_tile_x, abs_tile_y, abs_tile_z);
    TileChunk *tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y, chunk_pos.tile_chunk_z);
    u32 tile_chunk_value = get_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y);

    return tile_chunk_value;
}

internal u32 get_tile_value(TileMap *tile_map, TileMapPosition pos) {
    u32 tile_chunk_value = get_tile_value(tile_map, pos.abs_tile_x, pos.abs_tile_y, pos.abs_tile_z);
    return tile_chunk_value;
}

internal bool is_tile_map_point_empty(TileMap *tile_map, TileMapPosition can_pos) {
    u32 tile_chunk_value = get_tile_value(tile_map, can_pos);
    bool empty = (tile_chunk_value == 1) ||
                 (tile_chunk_value == 3) ||
                 (tile_chunk_value == 4);

    return empty;
}

internal void set_tile_value(MemoryArena *arena, TileMap *tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z, u32 tile_value) {
    TileChunkPosition chunk_pos = get_chunk_position_for(tile_map, abs_tile_x, abs_tile_y, abs_tile_z);
    TileChunk *tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y, chunk_pos.tile_chunk_z);

    assert(tile_chunk);

    if (!tile_chunk->tiles) {
        u32 tile_count = tile_map->chunk_dim * tile_map->chunk_dim;
        tile_chunk->tiles = push_array(arena, tile_count, u32);
        for (u32 i = 0; i < tile_count; ++i) {
            tile_chunk->tiles[i] = 1;
        }
    }

    set_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y, tile_value);
}

inline internal bool are_on_same_tile(TileMapPosition *a, TileMapPosition *b) {
    bool result = a->abs_tile_x == b->abs_tile_x &&
                  a->abs_tile_y == b->abs_tile_y &&
                  a->abs_tile_z == b->abs_tile_z;
    return result;
}

inline internal TileMapDifference subtract(TileMap *tile_map, TileMapPosition *a, TileMapPosition *b) {
    TileMapDifference result;

    f32 d_tile_x = (f32)a->abs_tile_x - (f32)b->abs_tile_x;
    f32 d_tile_y = (f32)a->abs_tile_y - (f32)b->abs_tile_y;
    f32 d_tile_z = (f32)a->abs_tile_z - (f32)b->abs_tile_z;

    result.d_x = (tile_map->tile_side_in_meters * d_tile_x) + (a->offset_x - b->offset_x);
    result.d_y = (tile_map->tile_side_in_meters * d_tile_y) + (a->offset_y - b->offset_y);
    result.d_z = (tile_map->tile_side_in_meters * d_tile_z);

    return result;
}

