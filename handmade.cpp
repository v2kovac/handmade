// Game Code - Platform Independent
#include "handmade.h"
#include "handmade_intrinsics.h"

#define TONE_HZ_START 256

static void game_output_sound(GameState *game_state, GameOutputSoundBuffer *sound_buffer, int tone_hz) {
    s16 tone_volume = 1000;
    s16 *sample_out = sound_buffer->samples;
    int wave_period = sound_buffer->samples_per_second / tone_hz;

    for (int sample_index = 0; sample_index < sound_buffer->sample_count; sample_index++) {
#if 0
        f32 sine_value = sinf(game_state->t_sine);
        s16 sample_value = (s16)(sine_value * tone_volume);
#else
        s16 sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

#if 0
        game_state->t_sine += 2.0f * PI32 * 1.0f / (f32)wave_period;
        if (game_state->t_sine > (2.0f * PI32)) {
            game_state->t_sine -= (2.0f * PI32);
        }
#endif
    }
}

static void draw_rectangle(GameOffscreenBuffer *buffer,
                           f32 real_min_x, f32 real_min_y,
                           f32 real_max_x, f32 real_max_y,
                           f32 r, f32 g, f32 b) {
    int min_x = round_f32_to_s32(real_min_x);
    int min_y = round_f32_to_s32(real_min_y);
    int max_x = round_f32_to_s32(real_max_x);
    int max_y = round_f32_to_s32(real_max_y);

    if (min_x < 0) {
        min_x = 0;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_x > buffer->width) {
        max_x = buffer->width;
    }
    if (max_y > buffer->height) {
        max_y = buffer->height;
    }

    u32 color = (round_f32_to_s32(r * 255.0f) << 16) |
                (round_f32_to_s32(g * 255.0f) << 8) |
                (round_f32_to_s32(b * 255.0f) << 0);
    u8 *end_of_buffer = (u8 *)buffer->memory + buffer->pitch * buffer->height;

    u8 *row = (u8 *)buffer->memory + (min_x * buffer->bytes_per_pixel) + (min_y * buffer->pitch);
    for (int y = min_y; y < max_y; ++y) {
        u32 *pixel = (u32 *)row;
        for (int x = min_x; x < max_x; ++x) {
            *pixel++ = color;
        }
        row += buffer->pitch;
    }
}

inline static u32 get_tile_value_unchecked(World *world, TileChunk *tile_chunk, u32 tile_x, u32 tile_y) {
    assert(tile_chunk);
    assert(tile_x < world->chunk_dim);
    assert(tile_y < world->chunk_dim);

    u32 tile_chunk_value = tile_chunk->tiles[tile_y * world->chunk_dim + tile_x];
    return tile_chunk_value;
}

inline static TileChunk *get_tile_chunk(World *world, s32 tile_chunk_x, s32 tile_chunk_y) {
    TileChunk *tile_chunk = NULL;
    if (tile_chunk_x >= 0 && tile_chunk_x < world->tile_chunk_count_x &&
        tile_chunk_y >= 0 && tile_chunk_y < world->tile_chunk_count_y)
    {
        tile_chunk = &world->tile_chunks[tile_chunk_y * world->tile_chunk_count_x + tile_chunk_x];
    }
    return tile_chunk;
}

inline static u32 get_tile_value(World *world, TileChunk *tile_chunk, u32 test_tile_x, u32 test_tile_y) {
    u32 tile_chunk_value = 0;

    if (tile_chunk) {
        tile_chunk_value = get_tile_value_unchecked(world, tile_chunk, test_tile_x, test_tile_y);
    }

    return tile_chunk_value;
}

inline static void recanonicalize_coord(World *world, u32 *tile, f32 *tile_rel) {
    // world is a taurus

    s32 offset = floor_f32_to_s32(*tile_rel / world->tile_side_in_meters);
    *tile += offset;
    *tile_rel -= (offset * world->tile_side_in_meters);

    assert(*tile_rel >= 0);
    assert(*tile_rel < world->tile_side_in_meters);
}

inline static WorldPosition recanonicalize_position(World *world, WorldPosition pos) {
    WorldPosition result = pos;

    recanonicalize_coord(world, &result.abs_tile_x, &result.tile_rel_x);
    recanonicalize_coord(world, &result.abs_tile_y, &result.tile_rel_y);

    return result;
}

inline static TileChunkPosition get_chunk_position_for(World *world, u32 abs_tile_x, u32 abs_tile_y) {
    TileChunkPosition result;

    result.tile_chunk_x = abs_tile_x >> world->chunk_shift;
    result.tile_chunk_y = abs_tile_y >> world->chunk_shift;
    result.rel_tile_x = abs_tile_x & world->chunk_mask;
    result.rel_tile_y = abs_tile_y & world->chunk_mask;

    return result;
}

inline static u32 get_tile_value(World *world, u32 abs_tile_x, u32 abs_tile_y) {
    TileChunkPosition chunk_pos = get_chunk_position_for(world, abs_tile_x, abs_tile_y);
    TileChunk *tile_chunk = get_tile_chunk(world, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y);
    u32 tile_chunk_value = get_tile_value(world, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y);

    return tile_chunk_value;
}

static bool is_world_point_empty(World *world, WorldPosition can_pos) {
    u32 tile_chunk_value = get_tile_value(world, can_pos.abs_tile_x, can_pos.abs_tile_y);
    bool empty = (tile_chunk_value == 0);

    return empty;
}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples) {
    GameState *game_state = (GameState *)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer, 400);
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

#define TILE_MAP_COUNT_X 256
#define TILE_MAP_COUNT_Y 256
    u32 temp_tiles[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    };

    World world;
    // 256 x 256 tile chunks
    world.chunk_shift = 8;
    world.chunk_mask = (1 << world.chunk_shift) - 1;
    world.chunk_dim = 256;

    world.tile_chunk_count_x = 1;
    world.tile_chunk_count_y = 1;

    TileChunk tile_chunk;
    tile_chunk.tiles = (u32 *)temp_tiles;
    world.tile_chunks = &tile_chunk;

    world.tile_side_in_meters = 1.4f;
    world.tile_side_in_pixels = 60;
    world.meters_to_pixels = (f32)world.tile_side_in_pixels / world.tile_side_in_meters;

    f32 lower_left_x = -((f32)world.tile_side_in_pixels / 2);
    f32 lower_left_y = (f32)buffer->height;

    f32 player_height = 1.4f;
    f32 player_width = 0.75f * player_height;


    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        game_state->player_p.abs_tile_x = 3;
        game_state->player_p.abs_tile_y = 3;
        game_state->player_p.tile_rel_x = 5.0f;
        game_state->player_p.tile_rel_y = 5.0f;

        memory->is_initialized = true;
    }

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput *controller = get_controller(input, i);
        if (controller->is_analog) {
        } else {
            f32 d_player_x = 0.0f;
            f32 d_player_y = 0.0f;
            if (controller->move_up.ended_down) {
                d_player_y = 1.0f;
            }
            if (controller->move_down.ended_down) {
                d_player_y = -1.0f;
            }
            if (controller->move_left.ended_down) {
                d_player_x = -1.0f;
            }
            if (controller->move_right.ended_down) {
                d_player_x = 1.0f;
            }
            d_player_x *= 2.0f;
            d_player_y *= 2.0f;

            WorldPosition new_player_p = game_state->player_p;
            new_player_p.tile_rel_x += input->dt_for_frame * d_player_x;
            new_player_p.tile_rel_y += input->dt_for_frame * d_player_y;
            new_player_p = recanonicalize_position(&world, new_player_p);

            WorldPosition new_player_p_left = new_player_p;
            new_player_p_left.tile_rel_x -= 0.5f * player_width;
            new_player_p_left = recanonicalize_position(&world, new_player_p_left);

            WorldPosition new_player_p_right = new_player_p;
            new_player_p_right.tile_rel_x += 0.5f * player_width;
            new_player_p_right = recanonicalize_position(&world, new_player_p_right);

            if (is_world_point_empty(&world, new_player_p) &&
                is_world_point_empty(&world, new_player_p_left) &&
                is_world_point_empty(&world, new_player_p_right))
            {
                game_state->player_p = new_player_p;
            }
        }
    }


    draw_rectangle(buffer, 0, 0, (f32)buffer->width, (f32)buffer->height, 1.0f, 0.0f, 0.1f);

    f32 center_x = 0.5f * (f32)buffer->width;
    f32 center_y = 0.5f * (f32)buffer->height;

    for (s32 relrow = -10; relrow < 10; ++relrow) {
        for (s32 relcol = -20; relcol < 20; ++relcol) {
            u32 col = relcol + game_state->player_p.abs_tile_x;
            u32 row = relrow + game_state->player_p.abs_tile_y;
            u32 tile_id = get_tile_value(&world, col, row);
            f32 gray = 0.5f;
            if (tile_id == 1) {
                gray = 1.0f;
            }
            if (row == game_state->player_p.abs_tile_y && col == game_state->player_p.abs_tile_x) {
                gray = 0.0f;
            }
            f32 min_x = center_x + (f32)relcol * world.tile_side_in_pixels;
            f32 min_y = center_y - (f32)relrow * world.tile_side_in_pixels;
            f32 max_x = min_x + world.tile_side_in_pixels;
            f32 max_y = min_y - world.tile_side_in_pixels;
            draw_rectangle(buffer, min_x, max_y, max_x, min_y, gray, gray, gray);
        }
    }
    f32 player_r = 1.0f;
    f32 player_g = 1.0f;
    f32 player_b = 0.0f;
    f32 player_left = center_x + (world.meters_to_pixels * game_state->player_p.tile_rel_x) -
                      (0.5f * world.meters_to_pixels * player_width);
    f32 player_top = center_y - (world.meters_to_pixels * game_state->player_p.tile_rel_y) -
                     (world.meters_to_pixels * player_height);
    draw_rectangle(buffer, player_left, player_top,
                   player_left + (world.meters_to_pixels * player_width),
                   player_top + (world.meters_to_pixels * player_height),
                   player_r, player_g, player_b);
}

/*
static void render_weird_gradient(GameOffscreenBuffer *buffer, int x_offset, int y_offset) {
    u32 *arr = (u32 *)buffer->memory;
    for (int y = 0; y < buffer->height; y++) {
        for (int x = 0; x < buffer->width; x++) {
            u8 blue = (u8)(x + x_offset);
            u8 green = (u8)(y + y_offset);
            *arr++ = ((green << 16) | blue);
        }
    }
}
*/
