// Game Code - Platform Independent

struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
};

struct GameOutputSoundBuffer {
    int samples_per_second;
    int sample_count;
    int16_t *samples;
};

static void game_output_sound(GameOutputSoundBuffer *sound_buffer, int tone_hz) {
    static float t_sine;
    int16_t tone_volume = 1000;
    int16_t *sample_out = sound_buffer->samples;
    int wave_period = sound_buffer->samples_per_second / tone_hz;

    for (int sample_index = 0; sample_index < sound_buffer->sample_count; sample_index++) {
        float sine_value = sinf(t_sine);
        int16_t sample_value = (int16_t)(sine_value * tone_volume);
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        t_sine += 2.0f * PI32 * 1.0f / (float)wave_period;
    }
}

static void render_weird_gradient(GameOffscreenBuffer *buffer, int x_offset, int y_offset) {
    uint32_t *arr = (uint32_t *)buffer->memory;
    for (int y = 0; y < buffer->height; y++) {
        for (int x = 0; x < buffer->width; x++) {
            uint8_t blue = x + x_offset;
            uint8_t green = y + y_offset;
            *arr++ = ((green << 8) | blue);
        }
    }
}

static void game_update_and_render(GameOffscreenBuffer *buffer, int x_offset, int y_offset, GameOutputSoundBuffer *sound_buffer, int tone_hz) {
    game_output_sound(sound_buffer, tone_hz);
    render_weird_gradient(buffer, x_offset, y_offset);
}
