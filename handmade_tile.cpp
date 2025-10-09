#include "handmade_tile.h"

inline static TileChunk *get_tile_chunk(TileMap *tile_map, u32 tile_chunk_x, u32 tile_chunk_y) {
    TileChunk *tile_chunk = NULL;
    if (tile_chunk_x >= 0 && tile_chunk_x < tile_map->tile_chunk_count_x &&
        tile_chunk_y >= 0 && tile_chunk_y < tile_map->tile_chunk_count_y)
    {
        tile_chunk = &tile_map->tile_chunks[tile_chunk_y * tile_map->tile_chunk_count_x + tile_chunk_x];
    }
    return tile_chunk;
}

inline static u32 get_tile_value_unchecked(TileMap *tile_map, TileChunk *tile_chunk, u32 tile_x, u32 tile_y) {
    assert(tile_chunk);
    assert(tile_x < tile_map->chunk_dim);
    assert(tile_y < tile_map->chunk_dim);

    u32 tile_chunk_value = tile_chunk->tiles[tile_y * tile_map->chunk_dim + tile_x];
    return tile_chunk_value;
}

inline static void set_tile_value_unchecked(TileMap *tile_map, TileChunk *tile_chunk, u32 tile_x, u32 tile_y, u32 tile_value) {
    assert(tile_chunk);
    assert(tile_x < tile_map->chunk_dim);
    assert(tile_y < tile_map->chunk_dim);

    tile_chunk->tiles[tile_y * tile_map->chunk_dim + tile_x] = tile_value;
}

inline static u32 get_tile_value(TileMap *tile_map, TileChunk *tile_chunk, u32 test_tile_x, u32 test_tile_y) {
    u32 tile_chunk_value = 0;

    if (tile_chunk) {
        tile_chunk_value = get_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y);
    }

    return tile_chunk_value;
}

inline static void set_tile_value(TileMap *tile_map, TileChunk *tile_chunk, u32 test_tile_x, u32 test_tile_y, u32 tile_value) {
    if (tile_chunk) {
        set_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y, tile_value);
    }
}

inline static TileChunkPosition get_chunk_position_for(TileMap *tile_map, u32 abs_tile_x, u32 abs_tile_y) {
    TileChunkPosition result;

    result.tile_chunk_x = abs_tile_x >> tile_map->chunk_shift;
    result.tile_chunk_y = abs_tile_y >> tile_map->chunk_shift;
    result.rel_tile_x = abs_tile_x & tile_map->chunk_mask;
    result.rel_tile_y = abs_tile_y & tile_map->chunk_mask;

    return result;
}

inline static void recanonicalize_coord(TileMap *tile_map, u32 *tile, f32 *tile_rel) {
    // tile_map is a taurus

    s32 offset = round_f32_to_s32(*tile_rel / tile_map->tile_side_in_meters);
    *tile += offset;
    *tile_rel -= (offset * tile_map->tile_side_in_meters);

    assert(*tile_rel >= (-0.5f * tile_map->tile_side_in_meters));
    assert(*tile_rel < (0.5f * tile_map->tile_side_in_meters));
}

inline static TileMapPosition recanonicalize_position(TileMap *tile_map, TileMapPosition pos) {
    TileMapPosition result = pos;

    recanonicalize_coord(tile_map, &result.abs_tile_x, &result.tile_rel_x);
    recanonicalize_coord(tile_map, &result.abs_tile_y, &result.tile_rel_y);

    return result;
}

static u32 get_tile_value(TileMap *tile_map, u32 abs_tile_x, u32 abs_tile_y) {
    TileChunkPosition chunk_pos = get_chunk_position_for(tile_map, abs_tile_x, abs_tile_y);
    TileChunk *tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y);
    u32 tile_chunk_value = get_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y);

    return tile_chunk_value;
}

static bool is_tile_map_point_empty(TileMap *tile_map, TileMapPosition can_pos) {
    u32 tile_chunk_value = get_tile_value(tile_map, can_pos.abs_tile_x, can_pos.abs_tile_y);
    bool empty = (tile_chunk_value == 0);

    return empty;
}

static void set_tile_value(MemoryArena *arena, TileMap *tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 tile_value) {
    TileChunkPosition chunk_pos = get_chunk_position_for(tile_map, abs_tile_x, abs_tile_y);
    TileChunk *tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y);

    // TODO: on demand tile chunk creation
    assert(tile_chunk);

    set_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y, tile_value);
}
