#pragma once

struct TileChunk {
    s32 tile_chunk_x;
    s32 tile_chunk_y;
    s32 tile_chunk_z;

    u32* tiles;

    TileChunk* next_in_hash;
};

struct TileChunkPosition {
    s32 tile_chunk_x;
    s32 tile_chunk_y;
    s32 tile_chunk_z;

    s32 rel_tile_x;
    s32 rel_tile_y;
};

struct TileMap {
    s32 chunk_shift;
    s32 chunk_mask;
    s32 chunk_dim;

    f32 tile_side_in_meters;

    TileChunk tile_chunk_hash[4096];
};

struct TileMapDifference {
    v2 d_xy;
    f32 d_z;
};

struct TileMapPosition {
    // These are fixed point tile locations
    // high bits are tile chunk index
    // low bits are the tile index in the chunk
    s32 abs_tile_x;
    s32 abs_tile_y;
    s32 abs_tile_z;

    // this is relative to the tile center
    v2 offset_;
};
