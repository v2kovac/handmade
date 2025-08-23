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

struct GameButtonState {
    int half_transition_count;
    bool ended_down;
};

struct GameControllerInput {
    bool is_analog;

    float start_x;
    float start_y;

    float min_x;
    float min_y;

    float max_x;
    float max_y;

    float end_x;
    float end_y;

    union {
        GameButtonState buttons[6];
        struct {
            GameButtonState up;
            GameButtonState down;
            GameButtonState left;
            GameButtonState right;
            GameButtonState left_shoulder;
            GameButtonState right_shoulder;
        };
    };
};

struct GameInput {
    GameControllerInput controllers[4];
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

static void game_update_and_render(GameInput *input, GameOffscreenBuffer *buffer, GameOutputSoundBuffer *sound_buffer) {
    static int x_offset = 0;
    static int y_offset = 0;
    static int tone_hz = 256;

    GameControllerInput *input0 = &input->controllers[0];
    if (input0->is_analog) {
        tone_hz = 256 + (int)(128.0f * input0->end_y);
        x_offset += (int)(4.0f * input0->end_x);
    } else {
    }

    if (input0->down.ended_down) {
        y_offset += 1;
    }

    game_output_sound(sound_buffer, tone_hz);
    render_weird_gradient(buffer, x_offset, y_offset);
}
