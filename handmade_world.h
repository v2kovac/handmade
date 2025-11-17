#pragma once

struct WorldEntityBlock {
    u32 entity_count;
    u32 low_entity_index[16];
    WorldEntityBlock* next;
};

struct WorldChunk {
    s32 chunk_x;
    s32 chunk_y;
    s32 chunk_z;

    WorldEntityBlock first_block;

    WorldChunk* next_in_hash;
};

struct World {
    f32 tile_side_in_meters;

    s32 chunk_shift;
    s32 chunk_mask;
    s32 chunk_dim;
    WorldChunk chunk_hash[4096];
};

struct WorldDifference {
    v2 d_xy;
    f32 d_z;
};

struct WorldPosition {
    // These are fixed point tile locations
    // high bits are tile chunk index
    // low bits are the tile index in the chunk
    s32 abs_tile_x;
    s32 abs_tile_y;
    s32 abs_tile_z;

    // this is relative to the tile center
    v2 offset_;
};
