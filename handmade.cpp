// Game Code - Platform Independent
#include "handmade.h"

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

static s32 round_f32_to_s32(f32 float_32) {
    return (s32)(float_32 + 0.5f);
}
static u32 round_f32_to_u32(f32 float_32) {
    return (u32)(float_32 + 0.5f);
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

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;
        game_state->player_x = 20.0;
        game_state->player_y = 20.0;
    }

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
            game_state->player_x += input->dt_for_frame * d_player_x;
            game_state->player_y += input->dt_for_frame * d_player_y;
        }
    }

    u32 tile_map[9][17] = {
        {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1 },
        {0, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1 },
        {0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
        {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0 },
        {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
        {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
        {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
        {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1 },
    };
    f32 upper_left_x = -30;
    f32 upper_left_y = 0;
    f32 tile_width = 60;
    f32 tile_height = 60;

    draw_rectangle(buffer, 0, 0, (f32)buffer->width, (f32)buffer->height, 1.0f, 0.0f, 0.1f);

    for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 17; ++col) {
            u32 tile_id = tile_map[row][col];
            f32 gray = 0.5f;
            if (tile_id == 1) {
                gray = 1.0f;
            }
            f32 min_x = upper_left_x + (f32)col * tile_width;
            f32 min_y = upper_left_y + (f32)row * tile_height;
            f32 max_x = min_x + tile_width;
            f32 max_y = min_y + tile_height;
            draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);
        }
    }
    f32 player_r = 1.0f;
    f32 player_g = 1.0f;
    f32 player_b = 0.0f;
    f32 player_width = 0.75f * tile_width;
    f32 player_height = tile_height;
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
