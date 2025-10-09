// Game Code - Platform Independent
#include "handmade.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"
#include "handmade_tile.cpp"

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

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples) {
    GameState *game_state = (GameState *)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer, 400);
}

static void initialize_arena(MemoryArena *arena, size_t size, u8 *base) {
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define push_struct(arena, type) (type *)push_struct_(arena, sizeof(type))
#define push_array(arena, count, type) (type *)push_struct_(arena, (count) * sizeof(type))
static void *push_struct_(MemoryArena *arena, size_t size) {
    assert((arena->used + size) <= arena->size);
    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

    f32 player_height = 1.4f;
    f32 player_width = 0.75f * player_height;

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        game_state->player_p.abs_tile_x = 1;
        game_state->player_p.abs_tile_y = 3;
        game_state->player_p.tile_rel_x = 5.0f;
        game_state->player_p.tile_rel_y = 5.0f;

        initialize_arena(&game_state->world_arena, memory->permanent_storage_size - sizeof(GameState), (u8 *)memory->permanent_storage + sizeof(GameState));
        game_state->world = push_struct(&game_state->world_arena, World);
        World *world = game_state->world;
        world->tile_map = push_struct(&game_state->world_arena, TileMap);

        TileMap *tile_map = world->tile_map;

        tile_map->chunk_shift = 4;
        tile_map->chunk_mask = (1 << tile_map->chunk_shift) - 1;
        tile_map->chunk_dim = (1 << tile_map->chunk_shift);
        tile_map->tile_chunk_count_x = 128;
        tile_map->tile_chunk_count_y = 128;

        tile_map->tile_chunks = push_array(&game_state->world_arena, tile_map->tile_chunk_count_x * tile_map->tile_chunk_count_y, TileChunk);


        for (u32 y = 0; y < tile_map->tile_chunk_count_y; ++y) {
            for (u32 x = 0; x < tile_map->tile_chunk_count_x; ++x) {
                tile_map->tile_chunks[y * tile_map->tile_chunk_count_x + x].tiles = push_array(&game_state->world_arena, tile_map->chunk_dim * tile_map->chunk_dim, u32);
            }
        }

        tile_map->tile_side_in_meters = 1.4f;
        tile_map->tile_side_in_pixels = 60;
        tile_map->meters_to_pixels = (f32)tile_map->tile_side_in_pixels / tile_map->tile_side_in_meters;

        f32 lower_left_x = -((f32)tile_map->tile_side_in_pixels / 2);
        f32 lower_left_y = (f32)buffer->height;

        u32 tiles_per_width = 17;
        u32 tiles_per_height = 9;
        for (u32 screen_y = 0; screen_y < 32; ++screen_y) {
            for (u32 screen_x = 0; screen_x < 32; ++screen_x) {
                for (u32 tile_y = 0; tile_y < tiles_per_height; ++tile_y) {
                    for (u32 tile_x = 0; tile_x < tiles_per_width; ++tile_x) {
                        u32 abs_tile_x = screen_x * tiles_per_width + tile_x;
                        u32 abs_tile_y = screen_y * tiles_per_height + tile_y;

                        set_tile_value(&game_state->world_arena, world->tile_map, abs_tile_x, abs_tile_y, (tile_x == tile_y) && (tile_x % 2) ? 1 : 0);
                    }
                }
            }
        }

        memory->is_initialized = true;
    }

    World *world = game_state->world;
    TileMap *tile_map = world->tile_map;

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
            f32 player_speed = 2.0f;
            if (controller->action_up.ended_down) {
                player_speed = 10.0f;
            }
            d_player_x *= player_speed;
            d_player_y *= player_speed;

            TileMapPosition new_player_p = game_state->player_p;
            new_player_p.tile_rel_x += input->dt_for_frame * d_player_x;
            new_player_p.tile_rel_y += input->dt_for_frame * d_player_y;
            new_player_p = recanonicalize_position(tile_map, new_player_p);

            TileMapPosition new_player_p_left = new_player_p;
            new_player_p_left.tile_rel_x -= 0.5f * player_width;
            new_player_p_left = recanonicalize_position(tile_map, new_player_p_left);

            TileMapPosition new_player_p_right = new_player_p;
            new_player_p_right.tile_rel_x += 0.5f * player_width;
            new_player_p_right = recanonicalize_position(tile_map, new_player_p_right);

            if (is_tile_map_point_empty(tile_map, new_player_p) &&
                is_tile_map_point_empty(tile_map, new_player_p_left) &&
                is_tile_map_point_empty(tile_map, new_player_p_right))
            {
                game_state->player_p = new_player_p;
            }
        }
    }


    draw_rectangle(buffer, 0, 0, (f32)buffer->width, (f32)buffer->height, 1.0f, 0.0f, 0.1f);

    f32 screen_center_x = 0.5f * (f32)buffer->width;
    f32 screen_center_y = 0.5f * (f32)buffer->height;

    for (s32 relrow = -10; relrow < 10; ++relrow) {
        for (s32 relcol = -20; relcol < 20; ++relcol) {
            u32 col = relcol + game_state->player_p.abs_tile_x;
            u32 row = relrow + game_state->player_p.abs_tile_y;
            u32 tile_id = get_tile_value(tile_map, col, row);
            f32 gray = 0.5f;
            if (tile_id == 1) {
                gray = 1.0f;
            }
            if (row == game_state->player_p.abs_tile_y && col == game_state->player_p.abs_tile_x) {
                gray = 0.0f;
            }
            f32 cen_x = screen_center_x - (tile_map->meters_to_pixels * game_state->player_p.tile_rel_x) + (f32)relcol * tile_map->tile_side_in_pixels;
            f32 cen_y = screen_center_y + (tile_map->meters_to_pixels * game_state->player_p.tile_rel_y) - (f32)relrow * tile_map->tile_side_in_pixels;
            f32 min_x = cen_x - 0.5f * tile_map->tile_side_in_pixels;
            f32 min_y = cen_y - 0.5f * tile_map->tile_side_in_pixels;
            f32 max_x = cen_x + 0.5f * tile_map->tile_side_in_pixels;
            f32 max_y = cen_y + 0.5f * tile_map->tile_side_in_pixels;
            draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);
        }
    }
    f32 player_r = 1.0f;
    f32 player_g = 1.0f;
    f32 player_b = 0.0f;
    f32 player_left = screen_center_x - (0.5f * tile_map->meters_to_pixels * player_width);
    f32 player_top = screen_center_y - (tile_map->meters_to_pixels * player_height);
    draw_rectangle(buffer, player_left, player_top,
                   player_left + (tile_map->meters_to_pixels * player_width),
                   player_top + (tile_map->meters_to_pixels * player_height),
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
