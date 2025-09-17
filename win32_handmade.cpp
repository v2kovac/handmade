// Windows Code - compile this file to get a windows app
#include "handmade.h"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>
#include <malloc.h>

struct OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};
struct WindowDimension {
    int width;
    int height;
};
struct SoundOutput {
    int samples_per_second = 48000;
    uint32_t running_sample_index = 0;
    int bytes_per_sample = sizeof(int16_t) * 2;
    DWORD secondary_buffer_size = (DWORD)(samples_per_second * bytes_per_sample);
    DWORD safety_bytes;
    float t_sine = 0;
};
struct DebugTimeMarker {
    DWORD output_play_cursor;
    DWORD output_write_cursor;
    DWORD output_location;
    DWORD output_byte_count;
    DWORD expected_flip_play_cursor;
    DWORD flip_play_cursor;
    DWORD flip_write_cursor;
};
#define WIN32_STATE_FILENAME_COUNT MAX_PATH
struct Win32ReplayBuffer {
    char replay_filename[WIN32_STATE_FILENAME_COUNT];
    void *memory_block;
};
struct Win32State {
    uint64_t total_size;
    void *game_memory_block;
    Win32ReplayBuffer replay_buffers[4];

    HANDLE recording_handle;
    int input_recording_index;

    HANDLE playback_handle;
    int input_playing_index;

    char exe_filename[WIN32_STATE_FILENAME_COUNT];
    char *one_past_last_exe_filename_slash;
};

static bool g_running;
static bool g_pause = false;
static OffscreenBuffer g_backbuffer;
static LPDIRECTSOUNDBUFFER g_secondary_buffer;
static int64_t g_perf_count_frequency;

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

static void win32_load_xinput() {
    HMODULE x_input_library = LoadLibrary("xinput1_4.dll");
    if (x_input_library) {
        XInputGetState = (x_input_get_state *)GetProcAddress(x_input_library, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(x_input_library, "XInputSetState");
    }
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static void init_dsound(HWND window, int32_t samples_per_second, int32_t buffer_size) {
    HMODULE dsound_library = LoadLibrary("dsound.dll");
    if (!dsound_library) return;

    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 2;
    wave_format.nSamplesPerSec = samples_per_second;
    wave_format.wBitsPerSample = 16;
    wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    wave_format.cbSize = 0;

    direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(dsound_library, "DirectSoundCreate");
    LPDIRECTSOUND direct_sound;
    if (!DirectSoundCreate) return;
    if (!SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0))) return;
    if (!SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return;

    LPDIRECTSOUNDBUFFER primary_buffer;
    DSBUFFERDESC buffer_description = {};
    buffer_description.dwSize = sizeof(buffer_description);
    buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;
    if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &primary_buffer, 0))) return;

    primary_buffer->SetFormat(&wave_format);

    // secondary buffer
    buffer_description = {};
    buffer_description.dwSize = sizeof(buffer_description);
    buffer_description.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    buffer_description.lpwfxFormat = &wave_format;
    buffer_description.dwBufferBytes = buffer_size;
    if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &g_secondary_buffer, 0))) return;
}

static WindowDimension get_window_dimension(HWND window) {
    RECT client_rect;
    GetClientRect(window, &client_rect);

    WindowDimension result;
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

static void resize_dib_section(OffscreenBuffer *buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytes_per_pixel = 4;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_size = (buffer->width * buffer->height) * buffer->bytes_per_pixel;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE); 
    buffer->pitch = width * buffer->bytes_per_pixel;
}

static void display_buffer_in_window(OffscreenBuffer *buffer, HDC device_context, int window_width, int window_height) {
    StretchDIBits(device_context,
                  0, 0, buffer->width, buffer->height,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
}

static void clear_sound_buffer(SoundOutput *sound_output) {
    VOID *region1;
    DWORD region1_size;
    VOID *region2;
    DWORD region2_size;
    HRESULT error = g_secondary_buffer->Lock(0, sound_output->secondary_buffer_size, &region1, &region1_size, &region2, &region2_size, 0);
    if (SUCCEEDED(error)) {
        uint8_t *dest_sample = (uint8_t *)region1;
        for (DWORD byte_index = 0; byte_index < region1_size; byte_index++) {
            *dest_sample++ = 0;
        }
        dest_sample = (uint8_t *)region2;
        for (DWORD byte_index = 0; byte_index < region2_size; byte_index++) {
            *dest_sample++ = 0;
        }

        g_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}

static void fill_sound_buffer(SoundOutput *sound_output, DWORD byte_to_lock, DWORD bytes_to_write, GameOutputSoundBuffer *source_buffer) {
    VOID *region1;
    DWORD region1_size;
    VOID *region2;
    DWORD region2_size;
    HRESULT error = g_secondary_buffer->Lock(byte_to_lock, bytes_to_write, &region1, &region1_size, &region2, &region2_size, 0);
    if (SUCCEEDED(error)) {
        int16_t *dest_sample = (int16_t *)region1;
        int16_t *source_sample = source_buffer->samples;
        DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
        for (DWORD sample_index = 0; sample_index < region1_sample_count; sample_index++) {
            *dest_sample++ = *source_sample++;
            *dest_sample++ = *source_sample++;

            sound_output->running_sample_index++;
        }

        dest_sample = (int16_t *)region2;
        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        for (DWORD sample_index = 0; sample_index < region2_sample_count; sample_index++) {
            *dest_sample++ = *source_sample++;
            *dest_sample++ = *source_sample++;

            sound_output->running_sample_index++;
        }

        g_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    } else {
        // TODO
    }
}

LRESULT main_window_callback(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch (message) {
        case WM_SIZE:
        {
        } break;

        case WM_CLOSE:
        case WM_DESTROY:
        {
            g_running = false;
        } break;

        case WM_ACTIVATEAPP:
        {
#if 0
            if (w_param == TRUE) {
                SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
            } else {
                SetLayeredWindowAttributes(window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
        } break;


        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            WindowDimension dim = get_window_dimension(window);
            display_buffer_in_window(&g_backbuffer, device_context, dim.width, dim.height);
            EndPaint(window, &paint);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            assert(!"this shouldn't get processed here");
        } break;
        default:
        {
            result = DefWindowProc(window, message, w_param, l_param);
        } break;
    }

    return result;
}

static void process_xinput_button(DWORD xinput_button_state, DWORD button_bit, GameButtonState *old_state, GameButtonState *new_state) {
    new_state->ended_down = (xinput_button_state & button_bit) == button_bit;
    new_state->half_transition_count = (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

static void process_keyboard_message(GameButtonState *new_state, bool is_down) {
    if (new_state->ended_down != is_down) {
        new_state->ended_down = is_down;
        new_state->half_transition_count++;
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file) {
    DebugReadFileResult result = {};

    HANDLE file_handle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        assert(false);
        exit(1);
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        assert(false);
        exit(1);
    }

    result.contents_size = safe_truncate_uint64(file_size.QuadPart);
    result.contents = VirtualAlloc(NULL, result.contents_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (!result.contents) {
        assert(false);
        exit(1);
    }

    DWORD bytes_read;
    if (ReadFile(file_handle, result.contents, result.contents_size, &bytes_read, NULL) && (result.contents_size == bytes_read)) {
    } else {
        assert(false);
        exit(1);
    }

    CloseHandle(file_handle);

    return result;
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory) {
    if (memory) {
        VirtualFree(memory, NULL, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file) {
    bool result = false;

    HANDLE file_handle = CreateFile(filename, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        assert(false);
        exit(1);
    }

    DWORD bytes_written;
    if (WriteFile(file_handle, memory, memory_size, &bytes_written, NULL)) {
        assert(bytes_written == memory_size);
        result = (bytes_written == memory_size);
    } else {
        assert(false);
        exit(1);
    }


    CloseHandle(file_handle);
    return result;
}

void WriteFileChunked(HANDLE hFile, const void *buffer, size_t totalSize) {
#define CHUNK_SIZE (500 * 1024 * 1024) // 500 mb
    const char *p = (const char *)buffer;
    size_t remaining = totalSize;

    while (remaining > 0) {
        DWORD toWrite = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : (DWORD)remaining;
        DWORD written;
        if (!WriteFile(hFile, p, toWrite, &written, NULL)) {
            DWORD error = GetLastError();
            assert(!"oops");
        }
        p += written;
        remaining -= written;
    }
}

static int string_length(char *string) {
    int count = 0;
    while (*string++) {
        ++count;
    }
    return count;
}

static void cat_strings(size_t source_a_count, char *source_a,
                        size_t source_b_count, char *source_b,
                        size_t dest_count, char *dest) {
    for (int i = 0; i < source_a_count; ++i) {
        *dest++ = *source_a++;
    }
    for (int i = 0; i < source_b_count; ++i) {
        *dest++ = *source_b++;
    }
    *dest++ = 0;
}

static void build_exe_path_filename(Win32State *state, char *filename, int dest_count, char *dest) {
    cat_strings(state->one_past_last_exe_filename_slash - state->exe_filename,
                state->exe_filename, string_length(filename), filename,
                dest_count, dest);
}

static void get_input_file_location(Win32State *state, int slot_index, int dest_count, char *dest) {
    assert(slot_index == 1);
    build_exe_path_filename(state, "loop_edit.hmi", dest_count, dest);
}
static void begin_recording_input(Win32State *state, int input_recording_index) {
    state->input_recording_index = input_recording_index;
    char filename[WIN32_STATE_FILENAME_COUNT];
    get_input_file_location(state, input_recording_index, sizeof(filename), filename);
    state->recording_handle = CreateFile(filename, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);

    // not working? fixed base address? write file not done?
    // looks like shared folders cant handle 1 gig, but they can handle 500 mb
    // so chunk the write
    DWORD bytes_to_write = (DWORD)state->total_size;
    assert(state->total_size == bytes_to_write);
    WriteFileChunked(state->recording_handle, state->game_memory_block, state->total_size);

    // wtf this is slower than the file at least on first time
    // CopyMemory(state->replay_buffers[input_recording_index].memory_block, state->game_memory_block, state->total_size);
}
static void end_recording_input(Win32State *state) {
    CloseHandle(state->recording_handle);
    state->input_recording_index = 0;
}
static void begin_input_playback(Win32State *state, int input_playing_index) {
    state->input_playing_index = input_playing_index;
    char filename[WIN32_STATE_FILENAME_COUNT];
    get_input_file_location(state, input_playing_index, sizeof(filename), filename);
    state->playback_handle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);

    DWORD bytes_to_read = (DWORD)state->total_size;
    assert(state->total_size == bytes_to_read);
    DWORD bytes_read;
    ReadFile(state->playback_handle, state->game_memory_block, bytes_to_read, &bytes_read, NULL);

    // wtf this is slow
    // CopyMemory(state->game_memory_block, state->replay_buffers[input_playing_index].memory_block, state->total_size);
}
static void end_input_playback(Win32State *state) {
    CloseHandle(state->playback_handle);
    state->input_playing_index = 0;
}
static void record_input(Win32State *state, GameInput *new_input) {
    DWORD bytes_written;
    WriteFile(state->recording_handle, new_input, sizeof(*new_input), &bytes_written, NULL);
}
static void playback_input(Win32State *state, GameInput *new_input) {
    DWORD bytes_read;
    if (ReadFile(state->playback_handle, new_input, sizeof(*new_input), &bytes_read, NULL)) {
        if (bytes_read == 0) {
            int playing_index = state->input_playing_index;
            end_input_playback(state);
            begin_input_playback(state, playing_index);
            ReadFile(state->playback_handle, new_input, sizeof(*new_input), &bytes_read, NULL);
        }
    }
}

static void process_pending_messages(Win32State *state, GameControllerInput *keyboard_controller) {
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_QUIT:
            {
                g_running = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32_t vk_code = (uint32_t)message.wParam;
                bool was_down = (message.lParam & (1 << 30)) != 0;
                bool is_down = (message.lParam & (1 << 31)) == 0;
                if (was_down == is_down) break;

                if (vk_code == 'W') {
                    process_keyboard_message(&keyboard_controller->move_up, is_down);
                } else if (vk_code == 'A') {
                    process_keyboard_message(&keyboard_controller->move_left, is_down);
                } else if (vk_code == 'S') {
                    process_keyboard_message(&keyboard_controller->move_down, is_down);
                } else if (vk_code == 'D') {
                    process_keyboard_message(&keyboard_controller->move_right, is_down);
                } else if (vk_code == 'Q') {
                    process_keyboard_message(&keyboard_controller->left_shoulder, is_down);
                } else if (vk_code == 'E') {
                    process_keyboard_message(&keyboard_controller->right_shoulder, is_down);
                } else if (vk_code == VK_UP) {
                    process_keyboard_message(&keyboard_controller->action_up, is_down);
                } else if (vk_code == VK_DOWN) {
                    process_keyboard_message(&keyboard_controller->action_down, is_down);
                } else if (vk_code == VK_LEFT) {
                    process_keyboard_message(&keyboard_controller->action_left, is_down);
                } else if (vk_code == VK_RIGHT) {
                    process_keyboard_message(&keyboard_controller->action_right, is_down);
                } else if (vk_code == VK_SPACE) {
                    process_keyboard_message(&keyboard_controller->start, is_down);
                } else if (vk_code == VK_BACK) {
                    process_keyboard_message(&keyboard_controller->back, is_down);
                } else if (vk_code == VK_ESCAPE) {
                    g_running = false;
                }
#if HANDMADE_INTERNAL
                else if (vk_code == 'P' && is_down) {
                    g_pause = !g_pause;
                }
                else if (vk_code == 'L' && is_down) {
                    if (state->input_recording_index == 0 && state->input_playing_index == 0) {
                        begin_recording_input(state, 1);
                    } else if (state->input_recording_index == 0 && state->input_playing_index > 0) {
                        end_input_playback(state);
                        // this only resets keyboard not gamepad, does gamepad need to be reset?
                        for (int i = 0; i < array_count(keyboard_controller->buttons); i++) {
                            keyboard_controller->buttons[i].ended_down = false;
                        }
                    } else {
                        end_recording_input(state);
                        begin_input_playback(state, 1);
                    }
                }
#endif
            } break;
            default:
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            } break;
        }
    }
}

static float process_xinput_stick_value(SHORT value, SHORT deadzone_threshold) {
    float result = 0;
    if (value < -deadzone_threshold) {
        result = (float)(value + deadzone_threshold) / (32768.0f - deadzone_threshold);
    } else if (value > deadzone_threshold) {
        result = float(value - deadzone_threshold) / (32767.0f - deadzone_threshold);
    }
    return result;
}

static inline LARGE_INTEGER get_wall_clock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

static inline float get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    float result = (float)(end.QuadPart - start.QuadPart) / (float)g_perf_count_frequency;
    return result;
}

#if 0
static void debug_draw_vertical(int x, int top, int bottom, uint32_t color) {
    if (top <= 0) {
        top = 0;
    }
    if (bottom >= g_backbuffer.height) {
        bottom = g_backbuffer.height - 1;
    }
    if (x >= 0 && x < g_backbuffer.width) {
        uint8_t *pixel = (uint8_t *)g_backbuffer.memory +
                         x * g_backbuffer.bytes_per_pixel +
                         top * g_backbuffer.pitch;
        for (int y = top; y < bottom; y++) {
            *(uint32_t *)pixel = color;
            pixel += g_backbuffer.pitch;
        }
    }
}

static inline void draw_sound_buffer_marker(SoundOutput *sound_output,
                                            float c,
                                            int pad_x,
                                            int top,
                                            int bottom,
                                            DWORD value,
                                            uint32_t color) {
    int x = pad_x + (int)(c * (float)value);
    debug_draw_vertical(x, top, bottom, color);
}

static void debug_sync_display(int marker_count,
                               DebugTimeMarker *markers,
                               int current_marker_index,
                               SoundOutput *sound_output,
                               float tartget_seconds_per_frame)
{
    int pad_x = 16;
    int pad_y = 16;
    int line_height = 64;
    float c = (float)(g_backbuffer.width - 2 * pad_x) / (float)sound_output->secondary_buffer_size;
    for (int i = 0; i < marker_count; i++) {
        DebugTimeMarker *this_marker = &markers[i];
        assert(this_marker->output_play_cursor < sound_output->secondary_buffer_size);
        assert(this_marker->output_write_cursor < sound_output->secondary_buffer_size);
        assert(this_marker->output_location < sound_output->secondary_buffer_size);
        assert(this_marker->output_byte_count < sound_output->secondary_buffer_size);
        assert(this_marker->flip_play_cursor < sound_output->secondary_buffer_size);
        assert(this_marker->flip_write_cursor < sound_output->secondary_buffer_size);

        DWORD play_color = 0xFFFFFFFF;
        DWORD write_color = 0xFFFF0000;
        DWORD expected_flip_color = 0xFFFFFF00;
        DWORD play_window_color = 0xFFFF00FF;
        int top = pad_y;
        int bottom = pad_y + line_height;

        if (i == current_marker_index) {
            top += line_height + pad_y;
            bottom += line_height + pad_y;

            int first_top = top;

            draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->output_play_cursor, play_color);
            draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->output_write_cursor, write_color);

            top += line_height + pad_y;
            bottom += line_height + pad_y;

            draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->output_location, play_color);
            draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->output_byte_count + this_marker->output_location, write_color);

            top += line_height + pad_y;
            bottom += line_height + pad_y;

            draw_sound_buffer_marker(sound_output, c, pad_x, first_top, bottom, this_marker->expected_flip_play_cursor, expected_flip_color);
        }

        draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->flip_play_cursor, play_color);
        draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->flip_play_cursor + (480 * sound_output->bytes_per_sample), play_window_color);
        draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->flip_write_cursor, write_color);
    }
}
#endif

struct GameCode {
    HMODULE game_code_dll;
    FILETIME last_write_time = {0};
    game_update_and_render_func *update_and_render;
    game_get_sound_samples_func *get_sound_samples;
    bool is_valid = false;
};

static FILETIME get_last_write_time(char *filename) {
    WIN32_FILE_ATTRIBUTE_DATA file_info;
    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &file_info)) {
        assert("missing file");
    }
    return file_info.ftLastWriteTime;
}

static void unload_game_code(GameCode *game_code) {
    if (game_code->game_code_dll) {
        FreeLibrary(game_code->game_code_dll);
        game_code->game_code_dll = NULL;
    }
    game_code->is_valid = false;
    game_code->last_write_time = {0};
    game_code->update_and_render = 0;
    game_code->get_sound_samples = 0;
}

static void reload_game_code(GameCode *game, char *source_dll_name, char *temp_dll_name) {
    FILETIME current_write_time = get_last_write_time(source_dll_name);
    LONG comparison = CompareFileTime(&current_write_time, &game->last_write_time);
    if (game->is_valid && comparison == 0) {
        return;
    }

    unload_game_code(game);
    // windows in parallels fucking sucks, FreeLibrary doesn't immediately free
    // is it better to led the platform loop keep going, or block? right now
    // i'm deleting the dll in the build.bat to get around this

    // if (!CopyFile(source_dll_name, temp_dll_name, FALSE)) {
    //     return;
    // }
    // while(!CopyFile(source_dll_name, temp_dll_name, FALSE));
    // char temp_dll_name[256];
    // sprintf_s(temp_dll_name, "handmade_%x_temp.dll", GetTickCount());
    CopyFile(source_dll_name, temp_dll_name, FALSE);

    game->game_code_dll = LoadLibrary(temp_dll_name);
    if (game->game_code_dll) {
        game->update_and_render = (game_update_and_render_func *)GetProcAddress(game->game_code_dll, "game_update_and_render");
        game->get_sound_samples = (game_get_sound_samples_func *)GetProcAddress(game->game_code_dll, "game_get_sound_samples");
        game->last_write_time = current_write_time;
        game->is_valid = game->update_and_render && game->get_sound_samples;
    }
    if (!game->is_valid) {
        game->update_and_render = 0;
        game->get_sound_samples = 0;
        game->last_write_time = {0};
    }
}

static void get_exe_filename(Win32State *state) {
    DWORD size_of_filename = GetModuleFileName(0, state->exe_filename, sizeof(state->exe_filename));
    state->one_past_last_exe_filename_slash = state->exe_filename;
    for (char *scan = state->exe_filename; *scan; ++scan) {
        if (*scan == '\\') {
            state->one_past_last_exe_filename_slash = scan + 1;
        }
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_code) {
    Win32State win32_state = {};

    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    g_perf_count_frequency = perf_count_frequency_result.QuadPart;

    get_exe_filename(&win32_state);
    char source_game_code_dll_full_path[WIN32_STATE_FILENAME_COUNT];
    build_exe_path_filename(&win32_state, "handmade.dll",
                            sizeof(source_game_code_dll_full_path), source_game_code_dll_full_path);
    char temp_game_code_dll_full_path[WIN32_STATE_FILENAME_COUNT];
    build_exe_path_filename(&win32_state, "handmade_temp.dll",
                            sizeof(temp_game_code_dll_full_path), temp_game_code_dll_full_path);

    UINT desired_scheduler_ms = 1;
    bool sleep_is_granular = timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR;

    win32_load_xinput();

    WNDCLASS window_class = {};

    resize_dib_section(&g_backbuffer, 960, 540);

    window_class.style = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = main_window_callback;
    window_class.hInstance = instance;
    //window_class.hIcon;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    ATOM register_result = RegisterClass(&window_class);
    if (register_result == 0) return 1;
    HWND window = CreateWindowEx(
        0, //WS_EX_TOPMOST|WS_EX_LAYERED,
        window_class.lpszClassName,
        "HandmadeHero",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        instance,
        0
    );
    if (window == NULL) return 1;

    HDC refresh_dc = GetDC(window);
    int monitor_refresh_hz = 60;
    int win32_refresh_rate = GetDeviceCaps(refresh_dc, VREFRESH);
    if (win32_refresh_rate > 1) {
        monitor_refresh_hz = win32_refresh_rate;
    }
    // i'm getting inconsistent mohitor_refresh_hz so i'm just hardcoding this for now
    float game_update_hz = 30.0f; // monitor_refresh_hz / 4.0f;
    float target_seconds_per_frame = 1.0f / (float)game_update_hz;
    ReleaseDC(window, refresh_dc);

    SoundOutput sound_output = {};
    sound_output.safety_bytes = (int)((((float)sound_output.samples_per_second * (float)sound_output.bytes_per_sample) / game_update_hz) / 3.0f);
    init_dsound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
    clear_sound_buffer(&sound_output);
    g_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

    g_running = true;
    int16_t *samples = (int16_t *)VirtualAlloc(0, sound_output.secondary_buffer_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
    LPVOID base_address = (LPVOID)terabytes(2);
#else
    LPVOID base_address = 0;
#endif
    GameMemory game_memory = {};
    game_memory.permanent_storage_size = megabytes(64);
    game_memory.transient_storage_size = gigabytes(1);
    game_memory.debug_platform_read_entire_file = debug_platform_read_entire_file;
    game_memory.debug_platform_write_entire_file = debug_platform_write_entire_file;
    game_memory.debug_platform_free_file_memory = debug_platform_free_file_memory;

    win32_state.total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
    win32_state.game_memory_block = VirtualAlloc(base_address, win32_state.total_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    game_memory.permanent_storage = win32_state.game_memory_block;
    game_memory.transient_storage = (uint8_t *)game_memory.permanent_storage + game_memory.permanent_storage_size;

    // skipping this since the replay_buffer memcpy is slow AF Ep 25
    // for (int i = 0; i < array_count(win32_state.replay_buffers); ++i) {
    //     win32_state.replay_buffers[i].memory_block = VirtualAlloc(0, win32_state.total_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    //     assert(win32_state.replay_buffers[i].memory_block);
    // }

    GameInput inputs[2] = {};
    GameInput *new_input = &inputs[0];
    GameInput *old_input = &inputs[1];
    new_input->seconds_to_advance_over_update = target_seconds_per_frame;

    int debug_time_marker_index = 0;
    DebugTimeMarker debug_time_markers[30] = {};

    DWORD audio_latency_bytes = 0;
    float audio_latency_seconds = 0;
    bool sound_is_valid = false;

    GameCode game = {};
    reload_game_code(&game, source_game_code_dll_full_path, temp_game_code_dll_full_path);
    int load_counter = 0;

    LARGE_INTEGER last_counter = get_wall_clock();
    LARGE_INTEGER flip_wall_clock = get_wall_clock();
    uint64_t last_cycle_count = __rdtsc();
    while (g_running) {
        reload_game_code(&game, source_game_code_dll_full_path, temp_game_code_dll_full_path);

        GameControllerInput *new_keyboard_controller = get_controller(new_input, 0);
        GameControllerInput *old_keyboard_controller = get_controller(old_input, 0);
        *new_keyboard_controller = {};
        for (int i = 0; i < array_count(new_keyboard_controller->buttons); i++) {
            new_keyboard_controller->buttons[i].ended_down = old_keyboard_controller->buttons[i].ended_down;
        }

        process_pending_messages(&win32_state, new_keyboard_controller);

        if (g_pause) continue;

        POINT mouse_p;
        GetCursorPos(&mouse_p);
        ScreenToClient(window, &mouse_p);
        new_input->mouse_x = mouse_p.x;
        new_input->mouse_y = mouse_p.y;
        new_input->mouse_z = 0;
        process_keyboard_message(&new_input->mouse_buttons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
        process_keyboard_message(&new_input->mouse_buttons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
        process_keyboard_message(&new_input->mouse_buttons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
        process_keyboard_message(&new_input->mouse_buttons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
        process_keyboard_message(&new_input->mouse_buttons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

        DWORD max_controller_count = XUSER_MAX_COUNT + 1; // plus 1 for keyboard
        if (max_controller_count > array_count(new_input->controllers)) {
            max_controller_count = array_count(new_input->controllers);
        }
        for (DWORD i = 1; i < max_controller_count; i++) {
            GameControllerInput *old_controller = get_controller(old_input, i);
            GameControllerInput *new_controller = get_controller(new_input, i);

            XINPUT_STATE controller_state;
            DWORD x_input_get_state_result = XInputGetState(i, &controller_state);
            if (x_input_get_state_result == ERROR_SUCCESS) {
                // controller is plugged in
                XINPUT_GAMEPAD *pad = &controller_state.Gamepad;
                new_controller->is_connected = true;
                new_controller->is_analog = old_controller->is_analog;

                new_controller->stick_avg_x = process_xinput_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                new_controller->stick_avg_y = process_xinput_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                if ((new_controller->stick_avg_x != 0.0f) || (new_controller->stick_avg_y != 0.0f)) {
                    new_controller->is_analog = true;
                }

                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                    new_controller->stick_avg_y = 1.0f;
                    new_controller->is_analog = false;
                } else if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                    new_controller->stick_avg_y = -1.0f;
                    new_controller->is_analog = false;
                } else if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                    new_controller->stick_avg_x = -1.0f;
                    new_controller->is_analog = false;
                } else if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                    new_controller->stick_avg_x = 1.0f;
                    new_controller->is_analog = false;
                } 


                float threshold = 0.5f;
                process_xinput_button((new_controller->stick_avg_x < -threshold) ? 1 : 0, 1, &old_controller->move_left, &new_controller->move_left);
                process_xinput_button((new_controller->stick_avg_x > threshold) ? 1 : 0, 1, &old_controller->move_right, &new_controller->move_right);
                process_xinput_button((new_controller->stick_avg_y < -threshold) ? 1 : 0, 1, &old_controller->move_down, &new_controller->move_down);
                process_xinput_button((new_controller->stick_avg_y > threshold) ? 1 : 0, 1, &old_controller->move_up, &new_controller->move_up);

                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_A, &old_controller->action_down, &new_controller->action_down);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_B, &old_controller->action_right, &new_controller->action_right);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_X, &old_controller->action_left, &new_controller->action_left);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_Y, &old_controller->action_right, &new_controller->action_right);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER, &old_controller->left_shoulder, &new_controller->left_shoulder);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER, &old_controller->right_shoulder, &new_controller->right_shoulder);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_START, &old_controller->start, &new_controller->start);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_BACK, &old_controller->back, &new_controller->back);
            }
        }

        ThreadContext thread = {};

        GameOffscreenBuffer go_buffer = {};
        go_buffer.width = g_backbuffer.width;
        go_buffer.height = g_backbuffer.height;
        go_buffer.memory = g_backbuffer.memory;
        go_buffer.pitch = g_backbuffer.pitch;
        go_buffer.bytes_per_pixel = g_backbuffer.bytes_per_pixel;

        if (win32_state.input_recording_index) {
            record_input(&win32_state, new_input);
        }
        if (win32_state.input_playing_index) {
            playback_input(&win32_state, new_input);
        }
        if (game.update_and_render) {
            game.update_and_render(&thread, &game_memory, new_input, &go_buffer);
        }

        LARGE_INTEGER audio_wall_clock = get_wall_clock();
        float from_begin_to_audio_seconds = get_seconds_elapsed(flip_wall_clock, audio_wall_clock);

        /* NOTE:

           Here is how sound output computation works.

           We define a safety value that is the number of samples we
           think our game update loop may vary by (let's say up to 2ms).

           When we wake up to write audio we will look and see what the
           play cursor position is and we will forecast ahead where we
           think the play cursor will be on the next frame boundary.

           We will then look to see if the write cursor is before that
           by at least our safety value. If it is, the target fill
           position is that frame boundary plus one frame. This gives
           us perfect audio sync in the case of a card that has low enough latency.

           If the write cursor is _after_ that safety margin, then we
           assume we can never sync the audio perfectly, so we will
           write one frame's worth of audio plus the safety margin's
           worth of guard samples.
        */
        DWORD play_cursor;
        DWORD write_cursor;
        if (g_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor) == DS_OK) {
            if (!sound_is_valid) {
                sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
                sound_is_valid = true;
            }

            DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;

            DWORD expected_sound_bytes_per_frame = (int)((float)(sound_output.samples_per_second * sound_output.bytes_per_sample) / game_update_hz);

            float seconds_left_until_flip = target_seconds_per_frame - from_begin_to_audio_seconds;
            DWORD expected_bytes_until_flip = (DWORD)((seconds_left_until_flip / target_seconds_per_frame) * (float)expected_sound_bytes_per_frame);

            DWORD expected_frame_boundary_byte = play_cursor + expected_bytes_until_flip;

            DWORD safe_write_cursor = write_cursor;
            if (safe_write_cursor < play_cursor) {
                safe_write_cursor += sound_output.secondary_buffer_size;
            }
            assert(safe_write_cursor >= play_cursor);
            safe_write_cursor += sound_output.safety_bytes;

            bool audio_card_is_low_latency = safe_write_cursor < expected_frame_boundary_byte;

            DWORD target_cursor = 0;
            if (audio_card_is_low_latency) {
                target_cursor = expected_frame_boundary_byte + expected_sound_bytes_per_frame;
            } else {
                target_cursor = write_cursor + expected_sound_bytes_per_frame + sound_output.safety_bytes;
            }
            target_cursor = target_cursor % sound_output.secondary_buffer_size;

            DWORD bytes_to_write = 0;
            if (byte_to_lock > target_cursor) {
                bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock + target_cursor;
            } else {
                bytes_to_write = target_cursor - byte_to_lock;
            }

            GameOutputSoundBuffer sound_buffer = {};
            sound_buffer.samples_per_second = sound_output.samples_per_second;
            sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
            sound_buffer.samples = samples;
            if (game.get_sound_samples) {
                game.get_sound_samples(&thread, &game_memory, &sound_buffer);
            }

#if HANDMADE_INTERNAL
            DebugTimeMarker *marker = &debug_time_markers[debug_time_marker_index];
            marker->output_play_cursor = play_cursor;
            marker->output_write_cursor = write_cursor;
            marker->output_location = byte_to_lock;
            marker->output_byte_count = bytes_to_write;
            marker->expected_flip_play_cursor = expected_frame_boundary_byte;

            if (write_cursor >= play_cursor) {
                audio_latency_bytes = write_cursor - play_cursor;
            } else {
                audio_latency_bytes = write_cursor + sound_output.secondary_buffer_size - play_cursor;
            }
            audio_latency_seconds = ((float)audio_latency_bytes / (float)sound_output.bytes_per_sample) / (float)sound_output.samples_per_second;

#if 0
            char text_buffer[256];
            sprintf_s(text_buffer, sizeof(text_buffer),
                    "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n",
                    byte_to_lock, target_cursor, bytes_to_write, play_cursor, write_cursor, audio_latency_bytes, audio_latency_seconds);
            OutputDebugString(text_buffer);
#endif
#endif

            fill_sound_buffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
        } else {
            assert(!"bad audio");
            sound_is_valid = false;
        }

        LARGE_INTEGER work_counter = get_wall_clock();
        float work_seconds_elapsed = get_seconds_elapsed(last_counter, work_counter);
        float seconds_elapsed_for_frame = work_seconds_elapsed;

        if (seconds_elapsed_for_frame < target_seconds_per_frame) {
            if (sleep_is_granular) {
                DWORD sleep_ms = (DWORD)(1000.0f * (target_seconds_per_frame - seconds_elapsed_for_frame));
                if (sleep_ms > 5) {
                    Sleep(sleep_ms - 5);
                }
            }
            float testSleep = get_seconds_elapsed(last_counter, get_wall_clock());
            if (testSleep >= target_seconds_per_frame) {
                //TODO logging
            }
            while (seconds_elapsed_for_frame < target_seconds_per_frame) {
                seconds_elapsed_for_frame = get_seconds_elapsed(last_counter, get_wall_clock());
            }
        } else {
            // missed frame rate
            // logging
        }

        LARGE_INTEGER end_counter = get_wall_clock();
        float ms_per_frame = 1000.0f * get_seconds_elapsed(last_counter, end_counter);
        last_counter = end_counter;

        WindowDimension dim = get_window_dimension(window);
        HDC device_context = GetDC(window);
        display_buffer_in_window(&g_backbuffer, device_context, dim.width, dim.height);
        ReleaseDC(window, device_context);


        flip_wall_clock = get_wall_clock();
#if HANDMADE_INTERNAL
        {
            DWORD play_cursor_test;
            DWORD write_cursor_test;
            if (g_secondary_buffer->GetCurrentPosition(&play_cursor_test, &write_cursor_test) == DS_OK) {
                assert(debug_time_marker_index < array_count(debug_time_markers));
                DebugTimeMarker *marker = &debug_time_markers[debug_time_marker_index];
                marker->flip_play_cursor = play_cursor_test;
                marker->flip_write_cursor = write_cursor_test;
            }
            debug_time_marker_index++;
            if (debug_time_marker_index >= array_count(debug_time_markers)) {
                debug_time_marker_index = 0;
            }
        }
#endif

        GameInput *temp = new_input;
        new_input = old_input;
        old_input = temp;

#if 0
        uint64_t end_cycle_count = __rdtsc();
        uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
        last_cycle_count = end_cycle_count;

        double fps = 0.0f;
        double mcpf = (double)cycles_elapsed / (1000.0f * 1000.0f);

        char buffer[256];
        sprintf_s(buffer, "%.02fms/f, %.02ff/s, %.02fmc/f\n", ms_per_frame, fps, mcpf);
        OutputDebugString(buffer);
#endif
    }

    return 0;
}
