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
    f32 chunk_side_in_meters;

    WorldEntityBlock* first_free;

    WorldChunk chunk_hash[4096];
};

struct WorldDifference {
    v2 d_xy;
    f32 d_z;
};

struct WorldPosition {
    s32 chunk_x;
    s32 chunk_y;
    s32 chunk_z;

    // NOTE this is relative to the chunk center
    v2 offset_;
};
