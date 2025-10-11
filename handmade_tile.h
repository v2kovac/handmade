#ifndef HANDMADE_TILE_H
#define HANDMADE_TILE_H

struct TileChunk {
    u32 *tiles;
};

struct TileChunkPosition {
    u32 tile_chunk_x;
    u32 tile_chunk_y;
    u32 tile_chunk_z;

    u32 rel_tile_x;
    u32 rel_tile_y;
};

struct TileMap {
    u32 chunk_shift;
    u32 chunk_mask;
    u32 chunk_dim;

    f32 tile_side_in_meters;

    u32 tile_chunk_count_x;
    u32 tile_chunk_count_y;
    u32 tile_chunk_count_z;

    TileChunk *tile_chunks;
};

struct TileMapPosition {
    u32 abs_tile_x;
    u32 abs_tile_y;
    u32 abs_tile_z;

    // this is relative to the tile
    f32 tile_rel_x;
    f32 tile_rel_y;
};

#endif
