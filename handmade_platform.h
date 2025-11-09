#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Compilers
#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#define COMPILER_LLVM 1
#endif
#endif

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;

#define internal static
#define local_global static
#define global static

#define PI32 3.14159265359f
#define array_count(array) (sizeof(array) / sizeof((array)[0]))
#define kilobytes(value) ((value) * 1024LL)
#define megabytes(value) (kilobytes(value) * 1024LL)
#define gigabytes(value) (megabytes(value) * 1024LL)
#define terabytes(value) (gigabytes(value) * 1024LL)

// common helpers
#if HANDMADE_SLOW
#define assert(expr) if (!(expr)) { *(int*)0 = 0; }
#else
#define assert(expr)
#endif

#define INVALID_CODE_PATH assert(!"Invalid Code Path")

typedef struct {
    int placeholder;
} ThreadContext;

#if HANDMADE_INTERNAL

typedef struct {
    u32 contents_size;
    void* contents;
} DebugReadFileResult;
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DebugReadFileResult name(ThreadContext* thread, char* filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file_func);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(ThreadContext* thread, char* filename, u32 memory_size, void* memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file_func);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext* thread, void* memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory_func);

#endif

// Game Structs
typedef struct {
    void* memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
} GameOffscreenBuffer;

typedef struct {
    int samples_per_second;
    int sample_count;
    s16* samples;
} GameOutputSoundBuffer;

typedef struct {
    int half_transition_count;
    bool ended_down;
} GameButtonState;

typedef struct {
    bool is_connected;
    bool is_analog;
    f32 stick_avg_x;
    f32 stick_avg_y;

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
} GameControllerInput;

typedef struct {
    GameButtonState mouse_buttons[5];
    s32 mouse_x, mouse_y, mouse_z;
    f32 dt_for_frame;
    GameControllerInput controllers[5];
} GameInput;

typedef struct {
    bool is_initialized;

    u64 permanent_storage_size;
    void* permanent_storage;

    u64 transient_storage_size;
    void* transient_storage;

    debug_platform_free_file_memory_func* debug_platform_free_file_memory;
    debug_platform_read_entire_file_func* debug_platform_read_entire_file;
    debug_platform_write_entire_file_func* debug_platform_write_entire_file;
} GameMemory;

internal inline u32 safe_truncate_uint64(u64 value) {
    assert(value <= 0xFFFFFF);
    u32 result = (u32)value;
    return result;
}

internal inline GameControllerInput* get_controller(GameInput* input, int controller_index) {
    assert(controller_index < array_count(input->controllers));
    return &input->controllers[controller_index];
}

// game interface
#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext* thread, GameMemory* memory, GameInput* input, GameOffscreenBuffer* buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext* thread, GameMemory* memory, GameOutputSoundBuffer* sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples_func);

#ifdef __cplusplus
}
#endif
