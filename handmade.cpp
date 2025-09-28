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

inline static u32 get_tile_value_unchecked(World *world, TileMap *tile_map, s32 tile_x, s32 tile_y) {
    assert(tile_map);
    assert(tile_x >= 0 && tile_x < world->count_x && tile_y >= 0 && tile_y < world->count_y);

    u32 tile_map_value = tile_map->tiles[tile_y * world->count_x + tile_x];
    return tile_map_value;
}

inline static TileMap *get_tile_map(World *world, s32 tile_map_x, s32 tile_map_y) {
    TileMap *tile_map = NULL;
    if (tile_map_x >= 0 && tile_map_y < world->tile_map_count_x &&
        tile_map_y >= 0 && tile_map_y < world->tile_map_count_y)
    {
        tile_map = &world->tile_maps[tile_map_y * world->tile_map_count_x + tile_map_x];
    }
    return tile_map;
}

inline static bool is_tile_map_point_empty(World *world, TileMap *tile_map, s32 test_tile_x, s32 test_tile_y) {
    bool empty = false;

    if (tile_map &&
        test_tile_x >= 0 && test_tile_x < world->count_x &&
        test_tile_y >= 0 && test_tile_y < world->count_y)
    {
        u32 tile_map_value = get_tile_value_unchecked(world, tile_map, test_tile_x, test_tile_y);
        empty = (tile_map_value == 0);
    }

    return empty;
}

inline static CanonicalPosition get_canonical_position(World *world, RawPosition pos) {
    CanonicalPosition result;
    result.tile_map_x = pos.tile_map_x;
    result.tile_map_y = pos.tile_map_y;

    f32 x = pos.x - world->upper_left_x;
    f32 y = pos.y - world->upper_left_y;

    result.tile_x = floor_f32_to_s32(x / world->tile_side_in_pixels);
    result.tile_y = floor_f32_to_s32(y / world->tile_side_in_pixels);

    // this looks wrong to me, dont you have to figure out the actual tile_x/y?
    result.tile_rel_x = x - (result.tile_x * world->tile_side_in_pixels);
    result.tile_rel_y = y - (result.tile_y * world->tile_side_in_pixels);

    assert(result.tile_rel_x >= 0 && result.tile_rel_x < world->tile_side_in_pixels);
    assert(result.tile_rel_y >= 0 && result.tile_rel_y < world->tile_side_in_pixels);

    if (result.tile_x < 0) {
        result.tile_x += world->count_x;
        --result.tile_map_x;
    }
    if (result.tile_y < 0) {
        result.tile_y += world->count_y;
        --result.tile_map_y;
    }
    if (result.tile_x >= world->count_x) {
        result.tile_x -= world->count_x;
        ++result.tile_map_x;
    }
    if (result.tile_y >= world->count_y) {
        result.tile_y -= world->count_y;
        ++result.tile_map_y;
    }

    return result;
}

static bool is_world_point_empty(World *world, RawPosition test_pos) {
    bool empty = false;

    CanonicalPosition can_pos = get_canonical_position(world, test_pos);
    TileMap *tile_map = get_tile_map(world, can_pos.tile_map_x, can_pos.tile_map_y);
    empty = is_tile_map_point_empty(world, tile_map, can_pos.tile_x, can_pos.tile_y);

    return empty;
}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples) {
    GameState *game_state = (GameState *)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer, 400);
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9
    u32 tiles_00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 },
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1 },
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0 },
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1 },
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1 },
        {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1 },
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 },
    };
    u32 tiles_01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 },
    };
    u32 tiles_10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 },
    };
    u32 tiles_11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 },
    };

    TileMap tile_maps[2][2];
    tile_maps[0][0].tiles = (u32 *)tiles_00;
    tile_maps[0][1].tiles = (u32 *)tiles_01;
    tile_maps[1][0].tiles = (u32 *)tiles_10;
    tile_maps[1][1].tiles = (u32 *)tiles_11;

    World world;
    world.tile_map_count_x = 2;
    world.tile_map_count_y = 2;
    world.tile_maps = (TileMap *)tile_maps;

    world.count_x = TILE_MAP_COUNT_X;
    world.count_y = TILE_MAP_COUNT_Y;
    world.tile_side_in_meters = 1.4f;
    world.tile_side_in_pixels = 60;
    world.upper_left_x = -((f32)world.tile_side_in_pixels / 2);
    world.upper_left_y = 0;

    f32 player_width = 0.75f * world.tile_side_in_pixels;
    f32 player_height = (f32)world.tile_side_in_pixels;


    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;
        game_state->player_x = 150.0;
        game_state->player_y = 150.0;
        game_state->player_tile_map_x = 0;
        game_state->player_tile_map_y = 0;
    }

    TileMap *tile_map = get_tile_map(&world, game_state->player_tile_map_x, game_state->player_tile_map_y);
    assert(tile_map);

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput *controller = get_controller(input, i);
        if (controller->is_analog) {
        } else {
            f32 d_player_x = 0.0f;
            f32 d_player_y = 0.0f;
            if (controller->move_up.ended_down) {
                d_player_y = -1.0f;
            }
            if (controller->move_down.ended_down) {
                d_player_y = 1.0f;
            }
            if (controller->move_left.ended_down) {
                d_player_x = -1.0f;
            }
            if (controller->move_right.ended_down) {
                d_player_x = 1.0f;
            }
            d_player_x *= 64.0f;
            d_player_y *= 64.0f;

            f32 new_player_x = game_state->player_x + input->dt_for_frame * d_player_x;
            f32 new_player_y = game_state->player_y + input->dt_for_frame * d_player_y;

            RawPosition player_pos = {};
            player_pos.tile_map_x = game_state->player_tile_map_x;
            player_pos.tile_map_y = game_state->player_tile_map_y;
            player_pos.x = new_player_x;
            player_pos.y = new_player_y;
            RawPosition player_left = player_pos;
            player_left.x -= 0.5f * player_width;
            RawPosition player_right = player_pos;
            player_right.x += 0.5f * player_width;

            if (is_world_point_empty(&world, player_pos) &&
                is_world_point_empty(&world, player_left) &&
                is_world_point_empty(&world, player_right))
            {
                CanonicalPosition can_pos = get_canonical_position(&world, player_pos);
                game_state->player_tile_map_x = can_pos.tile_map_x;
                game_state->player_tile_map_y = can_pos.tile_map_y;
                game_state->player_x = world.upper_left_x + world.tile_side_in_pixels * can_pos.tile_x + can_pos.tile_rel_x;
                game_state->player_y = world.upper_left_y + world.tile_side_in_pixels * can_pos.tile_y + can_pos.tile_rel_y;
            }
        }
    }


    draw_rectangle(buffer, 0, 0, (f32)buffer->width, (f32)buffer->height, 1.0f, 0.0f, 0.1f);

    for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 17; ++col) {
            u32 tile_id = get_tile_value_unchecked(&world, tile_map, col, row);
            f32 gray = 0.5f;
            if (tile_id == 1) {
                gray = 1.0f;
            }
            f32 min_x = world.upper_left_x + (f32)col * world.tile_side_in_pixels;
            f32 min_y = world.upper_left_y + (f32)row * world.tile_side_in_pixels;
            f32 max_x = min_x + world.tile_side_in_pixels;
            f32 max_y = min_y + world.tile_side_in_pixels;
            draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);
        }
    }
    f32 player_r = 1.0f;
    f32 player_g = 1.0f;
    f32 player_b = 0.0f;
    f32 player_left = game_state->player_x - 0.5f * player_width;
    f32 player_top = game_state->player_y - player_height;
    draw_rectangle(buffer, player_left, player_top,
                   player_left + player_width,
                   player_top + player_height,
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
