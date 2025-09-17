// Game Code - Platform Independent
#include "handmade.h"

#define TONE_HZ_START 256

static void game_output_sound(GameState *game_state, GameOutputSoundBuffer *sound_buffer, int tone_hz) {
    int16_t tone_volume = 1000;
    int16_t *sample_out = sound_buffer->samples;
    int wave_period = sound_buffer->samples_per_second / tone_hz;

    for (int sample_index = 0; sample_index < sound_buffer->sample_count; sample_index++) {
#if 0
        float sine_value = sinf(game_state->t_sine);
        int16_t sample_value = (int16_t)(sine_value * tone_volume);
#else
        int16_t sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

#if 0
        game_state->t_sine += 2.0f * PI32 * 1.0f / (float)wave_period;
        if (game_state->t_sine > (2.0f * PI32)) {
            game_state->t_sine -= (2.0f * PI32);
        }
#endif
    }
}

static int round_float_to_int(float real_32) {
    return (int)(real_32 + 0.5f);
}

static void draw_rectangle(GameOffscreenBuffer *buffer,
                           float real_min_x, float real_min_y,
                           float real_max_x, float real_max_y,
                           uint32_t color) {
    int min_x = round_float_to_int(real_min_x);
    int min_y = round_float_to_int(real_min_y);
    int max_x = round_float_to_int(real_max_x);
    int max_y = round_float_to_int(real_max_y);

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

    uint8_t *end_of_buffer = (uint8_t *)buffer->memory + buffer->pitch * buffer->height;

    uint8_t *row = (uint8_t *)buffer->memory + (min_x * buffer->bytes_per_pixel) + (min_y * buffer->pitch);
    for (int y = min_y; y < max_y; ++y) {
        uint32_t *pixel = (uint32_t *)row;
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
    }

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput *controller = get_controller(input, i);
        if (controller->is_analog) {
        } else {
        }
    }

    draw_rectangle(buffer, 0, 0, (float)buffer->width, (float)buffer->height, 0x00FF00FF);
    draw_rectangle(buffer, 10.0f, 10.0f, 40.0f, 40.0f, 0x0000FFFF);
}

/*
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
*/
