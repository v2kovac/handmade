// Game Code - Platform Independent
#if HANDMADE_SLOW
#define assert(expr) if (!(expr)) { *(int *)0 = 0; }
#else
#define assert(expr)
#endif

#define PI32 3.14159265359f
#define array_count(array) (sizeof(array) / sizeof((array)[0]))
#define kilobytes(value) ((value) * 1024LL)
#define megabytes(value) (kilobytes(value) * 1024LL)
#define gigabytes(value) (megabytes(value) * 1024LL)
#define terabytes(value) (gigabytes(value) * 1024LL)


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
    bool is_connected = false;
    bool is_analog = false;
    float stick_avg_x;
    float stick_avg_y;

    union {
        GameButtonState buttons[12];
        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;

            GameButtonState action_up;
            GameButtonState action_down;
            GameButtonState action_left;
            GameButtonState action_right;

            GameButtonState left_shoulder;
            GameButtonState right_shoulder;

            GameButtonState back;
            GameButtonState start;

            // keep this element last for bounds checking
            GameButtonState terminator;
        };
    };
};

struct GameInput {
    GameControllerInput controllers[5];
};
static inline GameControllerInput *get_controller(GameInput *input, int controller_index) {
    assert(controller_index < array_count(input->controllers));
    return &input->controllers[controller_index];
}

struct GameMemory {
    bool is_initialized;

    uint64_t permanent_storage_size;
    void *permanent_storage;

    uint64_t transient_storage_size;
    void *transient_storage;
};

struct GameState {
    int tone_hz;
    int x_offset;
    int y_offset;
};

static inline uint32_t safe_truncate_uint64(uint64_t value) {
    assert(value <= 0xFFFFFF);
    uint32_t result = (uint32_t)value;
    return result;
}

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
            uint8_t blue = (uint8_t)(x + x_offset);
            uint8_t green = (uint8_t)(y + y_offset);
            *arr++ = ((green << 8) | blue);
        }
    }
}

static void game_update_and_render(GameMemory *memory,
                                   GameInput *input,
                                   GameOffscreenBuffer *buffer,
                                   GameOutputSoundBuffer *sound_buffer) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        char *filename = __FILE__;
        DebugReadFileResult file = debug_platform_read_entire_file(filename);
        if (file.contents) {
            debug_platform_write_entire_file("test.out", file.contents_size, file.contents);
            debug_platform_free_file_memory(file.contents);
        }

        game_state->tone_hz = 256;
        memory->is_initialized = true;
    }

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput *controller = get_controller(input, i);
        if (controller->is_analog) {
            game_state->tone_hz = 256 + (int)(128.0f * controller->stick_avg_y);
            game_state->x_offset += (int)(4.0f * controller->stick_avg_x);
        } else {
            if (controller->move_left.ended_down) {
                game_state->x_offset -= 1;
            } else if (controller->move_right.ended_down) {
                game_state->x_offset += 1;
            }
        }

        if (controller->action_down.ended_down) {
            game_state->y_offset += 1;
        }
    }

    game_output_sound(sound_buffer, game_state->tone_hz);
    render_weird_gradient(buffer, game_state->x_offset, game_state->y_offset);
}
