// Game Code - Platform Independent
#include "handmade.h"

#define TONE_HZ_START 256

static void game_output_sound(GameState *game_state, GameOutputSoundBuffer *sound_buffer) {
    int16_t tone_volume = 1000;
    int16_t *sample_out = sound_buffer->samples;
    int wave_period = sound_buffer->samples_per_second / game_state->tone_hz;

    for (int sample_index = 0; sample_index < sound_buffer->sample_count; sample_index++) {
        float sine_value = sinf(game_state->t_sine);
#if 0
        int16_t sample_value = (int16_t)(sine_value * tone_volume);
#else
        int16_t sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        game_state->t_sine += 2.0f * PI32 * 1.0f / (float)wave_period;
        if (game_state->t_sine > (2.0f * PI32)) {
            game_state->t_sine -= (2.0f * PI32);
        }
    }
}

static void render_weird_gradient(GameOffscreenBuffer *buffer, int x_offset, int y_offset) {
    uint32_t *arr = (uint32_t *)buffer->memory;
    for (int y = 0; y < buffer->height; y++) {
        for (int x = 0; x < buffer->width; x++) {
            uint8_t blue = (uint8_t)(x + x_offset);
            uint8_t green = (uint8_t)(y + y_offset);
            *arr++ = ((green << 16) | blue);
        }
    }
}

static void render_player(GameOffscreenBuffer *buffer, int player_x, int player_y) {
    uint8_t *end_of_buffer = (uint8_t *)buffer->memory + buffer->pitch * buffer->height;

    uint32_t color = 0xFFFFFFFF;
    int top = player_y;
    int bottom = player_y + 10;
    for (int x = player_x; x < player_x + 10; x++) {
        uint8_t *pixel = (uint8_t *)buffer->memory + (x * buffer->bytes_per_pixel) + (top * buffer->pitch);
        for (int y = top; y < bottom; y++) {
            if (pixel >= buffer->memory && pixel < end_of_buffer) {
                *(uint32_t *)pixel = color;
            }
            pixel += buffer->pitch;
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples) {
    GameState *game_state = (GameState *)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer);
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        char *filename = __FILE__;
        DebugReadFileResult file = memory->debug_platform_read_entire_file(filename);
        if (file.contents) {
            memory->debug_platform_write_entire_file("test.out", file.contents_size, file.contents);
            memory->debug_platform_free_file_memory(file.contents);
        }

        game_state->tone_hz = TONE_HZ_START;
        game_state->t_sine = 0.0f;
        game_state->player_x = 100;
        game_state->player_y = 100;
        memory->is_initialized = true;
    }

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput *controller = get_controller(input, i);
        if (controller->is_analog) {
            game_state->tone_hz = TONE_HZ_START + (int)(128.0f * controller->stick_avg_y);
            game_state->x_offset += (int)(4.0f * controller->stick_avg_x);
        } else {
            if (controller->move_left.ended_down) {
                game_state->x_offset -= 1;
                game_state->player_x -= 5;
            } else if (controller->move_right.ended_down) {
                game_state->x_offset += 1;
                game_state->player_x += 5;
            }
            if (controller->move_up.ended_down) {
                game_state->player_y -= 5;
            } else if (controller->move_down.ended_down) {
                game_state->player_y += 5;
            }
        }

        if (controller->action_down.ended_down) {
            game_state->y_offset += 1;
            game_state->t_jump = 2.0f;
        } else if (controller->action_up.ended_down) {
            game_state->y_offset -= 1;
        }
        if (game_state->t_jump > 0) {
            game_state->player_y += (int)(10.0f * sinf(PI32 * game_state->t_jump));
        }
        game_state->t_jump -= 0.033f;
    }

    render_weird_gradient(buffer, game_state->x_offset, game_state->y_offset);
    render_player(buffer, game_state->player_x, game_state->player_y);
}
