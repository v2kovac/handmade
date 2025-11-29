#include "handmade_world.h"

#define WORLD_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define WORLD_CHUNK_UNINITIALIZED INT32_MAX
#define TILES_PER_CHUNK 16

internal bool is_canonical(World* world, f32 tile_rel) {
    // TODO fix float point funkiness, this will be replaced dont need it
    return (tile_rel >= (-0.5f * world->chunk_side_in_meters)) &&
           (tile_rel <= (0.5f * world->chunk_side_in_meters));
}

internal bool is_canonical(World* world, V2 offset) {
    return is_canonical(world, offset.x) && is_canonical(world, offset.y);
}

internal bool are_in_same_chunk(World* world, WorldPosition* a, WorldPosition* b) {
    assert(is_canonical(world, a->offset_));
    assert(is_canonical(world, b->offset_));
    bool result = a->chunk_x == b->chunk_x &&
                  a->chunk_y == b->chunk_y &&
                  a->chunk_z == b->chunk_z;
    return result;
}

internal WorldChunk* get_world_chunk(World* world, s32 chunk_x, s32 chunk_y, s32 chunk_z,
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

internal void recanonicalize_coord(World* world, s32* tile, f32* tile_rel) {
    s32 offset = round_f32_to_s32(*tile_rel / world->chunk_side_in_meters);
    *tile += offset;
    *tile_rel -= (offset * world->chunk_side_in_meters);

    assert(is_canonical(world, *tile_rel));
}

internal WorldPosition map_to_chunk_space(World* world, WorldPosition base_pos, V2 offset) {
    WorldPosition result = base_pos;

    result.offset_ += offset;
    recanonicalize_coord(world, &result.chunk_x, &result.offset_.x);
    recanonicalize_coord(world, &result.chunk_y, &result.offset_.y);

    return result;
}

internal void initialize_world(World* world, f32 tile_side_in_meters) {
    world->tile_side_in_meters = tile_side_in_meters;
    world->chunk_side_in_meters = (f32)TILES_PER_CHUNK * tile_side_in_meters;
    world->first_free = 0;

    for (u32 i = 0; i < array_count(world->chunk_hash); ++i) {
        world->chunk_hash[i].chunk_x = WORLD_CHUNK_UNINITIALIZED;
        world->chunk_hash[i].first_block.entity_count = 0;
    }
}

internal WorldPosition chunk_position_from_tile_position(World* world, s32 abs_tile_x, s32 abs_tile_y, s32 abs_tile_z) {
    WorldPosition result = {};

    result.chunk_x = abs_tile_x / TILES_PER_CHUNK;
    result.chunk_y = abs_tile_y / TILES_PER_CHUNK;
    result.chunk_z = abs_tile_z / TILES_PER_CHUNK;

    result.offset_.x = (f32)(abs_tile_x - (result.chunk_x * TILES_PER_CHUNK)) * world->tile_side_in_meters;
    result.offset_.y = (f32)(abs_tile_y - (result.chunk_y * TILES_PER_CHUNK)) * world->tile_side_in_meters;
    // TODO move ot 3d Z

    return result;
}

internal WorldDifference subtract(World* world, WorldPosition* a, WorldPosition* b) {
    WorldDifference result;

    V2 d_tile_xy = { (f32)a->chunk_x - (f32)b->chunk_x,
                     (f32)a->chunk_y - (f32)b->chunk_y };
    f32 d_tile_z = (f32)a->chunk_z - (f32)b->chunk_z;

    result.d_xy = (world->chunk_side_in_meters * d_tile_xy) + (a->offset_ - b->offset_);
    result.d_z = (world->chunk_side_in_meters * d_tile_z);

    return result;
}

internal WorldPosition centered_chunk_point(u32 chunk_x, u32 chunk_y, u32 chunk_z) {
    WorldPosition result = {};
    result.chunk_x = chunk_x;
    result.chunk_y = chunk_y;
    result.chunk_z = chunk_z;
    return result;
}

internal void change_entity_location(MemoryArena* arena, World* world, u32 low_entity_index,
                                            WorldPosition* old_p, WorldPosition* new_p)
{
    if (old_p && are_in_same_chunk(world, old_p, new_p)) return;

    if (old_p) {
        WorldChunk* chunk = get_world_chunk(world, old_p->chunk_x, old_p->chunk_y, old_p->chunk_z);
        assert(chunk);
        if (chunk) {
            WorldEntityBlock* first_block = &chunk->first_block;
            for (WorldEntityBlock* block = first_block; block; block = block->next) {
                for (u32 i = 0; i < block->entity_count; ++i) {
                    if (block->low_entity_index[i] == low_entity_index) {
                        assert(first_block->entity_count > 0);
                        block->low_entity_index[i] = first_block->low_entity_index[--first_block->entity_count];
                        if (first_block->entity_count == 0 && first_block->next) {
                            WorldEntityBlock* next_block = first_block->next;
                            *first_block = *next_block;

                            next_block->next = world->first_free;
                            world->first_free = next_block;
                        }
                        goto end_loops;
                    }
                }
            }
        }
    }

    end_loops:
    WorldChunk* chunk = get_world_chunk(world, new_p->chunk_x, new_p->chunk_y, new_p->chunk_z, arena);
    assert(chunk);

    WorldEntityBlock* block = &chunk->first_block;
    if (block->entity_count == array_count(block->low_entity_index)) {
        WorldEntityBlock* old_block = world->first_free;
        if (old_block) {
            world->first_free = old_block->next;
        } else {
            old_block = push_struct(arena, WorldEntityBlock);
        }
        *old_block = *block;
        block->next = old_block;
        block->entity_count = 0;
    }

    assert(block->entity_count < array_count(block->low_entity_index));
    block->low_entity_index[block->entity_count++] = low_entity_index;
}




