#ifndef HANDMADE_H
#define HANDMADE_H

#include <stdint.h>
#include <math.h>

#define PI32 3.14159265359f
#define array_count(array) (sizeof(array) / sizeof((array)[0]))
#define kilobytes(value) ((value) * 1024LL)
#define megabytes(value) (kilobytes(value) * 1024LL)
#define gigabytes(value) (megabytes(value) * 1024LL)
#define terabytes(value) (gigabytes(value) * 1024LL)

struct ThreadContext {
    int placeholder;
};

#if HANDMADE_INTERNAL

struct DebugReadFileResult {
    uint32_t contents_size;
    void *contents;
};
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DebugReadFileResult name(ThreadContext *thread, char *filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file_func);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(ThreadContext *thread, char *filename, uint32_t memory_size, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file_func);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext *thread, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory_func);

#endif

// Game Structs
struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
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
    GameButtonState mouse_buttons[5];
    int32_t mouse_x, mouse_y, mouse_z;
    GameControllerInput controllers[5];
};

struct GameMemory {
    bool is_initialized;

    uint64_t permanent_storage_size;
    void *permanent_storage;

    uint64_t transient_storage_size;
    void *transient_storage;

    debug_platform_free_file_memory_func *debug_platform_free_file_memory;
    debug_platform_read_entire_file_func *debug_platform_read_entire_file;
    debug_platform_write_entire_file_func *debug_platform_write_entire_file;
};

struct GameState {
    int tone_hz;
    int x_offset;
    int y_offset;
    float t_sine = 0.0f;

    int player_x;
    int player_y;
    float t_jump;
};


// game interface
#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext *thread, GameMemory *memory, GameInput *input, GameOffscreenBuffer *buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext *thread, GameMemory *memory, GameOutputSoundBuffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples_func);


// common helpers
#if HANDMADE_SLOW
#define assert(expr) if (!(expr)) { *(int *)0 = 0; }
#else
#define assert(expr)
#endif


static inline uint32_t safe_truncate_uint64(uint64_t value) {
    assert(value <= 0xFFFFFF);
    uint32_t result = (uint32_t)value;
    return result;
}

static inline GameControllerInput *get_controller(GameInput *input, int controller_index) {
    assert(controller_index < array_count(input->controllers));
    return &input->controllers[controller_index];
}

#endif
