// Windows Code - compile this file to get a windows app
#include <stdint.h>
#include <math.h>

#if HANDMADE_INTERNAL
struct DebugReadFileResult {
    uint32_t contents_size;
    void *contents;
};
static DebugReadFileResult debug_platform_read_entire_file(char *filename);
static void debug_platform_free_file_memory(void *memory);
static bool debug_platform_write_entire_file(char *filename, uint32_t memory_size, void *memory);
#endif

#include "handmade.cpp"

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
    int latency_sample_count;
    float t_sine = 0;
};
struct DebugTimeMarker {
    DWORD play_cursor;
    DWORD write_cursor;
};

static bool g_running;
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
                  0, 0, window_width, window_height,
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
            OutputDebugString("WM_ACTIVATEAPP\n");
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
    assert(new_state->ended_down != is_down);
    new_state->ended_down = is_down;
    new_state->half_transition_count++;
}

static DebugReadFileResult debug_platform_read_entire_file(char *filename) {
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

static void debug_platform_free_file_memory(void *memory) {
    if (memory) {
        VirtualFree(memory, NULL, MEM_RELEASE);
    }
}

static bool debug_platform_write_entire_file(char *filename, uint32_t memory_size, void *memory) {
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

static void process_pending_messages(GameControllerInput *keyboard_controller) {
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

static void debug_draw_vertical(int x, int top, int bottom, uint32_t color) {
    uint8_t *pixel = (uint8_t *)g_backbuffer.memory +
                     x * g_backbuffer.bytes_per_pixel +
                     top * g_backbuffer.pitch;
    for (int y = top; y < bottom; y++) {
        *(uint32_t *)pixel = color;
        pixel += g_backbuffer.pitch;
    }
}

static inline void draw_sound_buffer_marker(SoundOutput *sound_output,
                                            float c,
                                            int pad_x,
                                            int top,
                                            int bottom,
                                            DWORD value,
                                            uint32_t color) {
    assert(value < sound_output->secondary_buffer_size);
    int x = pad_x + (int)(c * (float)value);
    debug_draw_vertical(x, top, bottom, color);
}

static void debug_sync_display(int marker_count,
                               DebugTimeMarker *markers,
                               SoundOutput *sound_output,
                               float tartget_seconds_per_frame)
{
    int pad_x = 16;
    int pad_y = 16;
    int top = pad_y;
    int bottom = g_backbuffer.height - pad_y;
    float c = (float)(g_backbuffer.width - 2 * pad_x) / (float)sound_output->secondary_buffer_size;
    for (int i = 0; i < marker_count; i++) {
        DebugTimeMarker *this_marker = &markers[i];
        draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->play_cursor, 0xFFFFFFFF);
        draw_sound_buffer_marker(sound_output, c, pad_x, top, bottom, this_marker->write_cursor, 0xFFFF0000);
    }
}


int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_code) {
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    g_perf_count_frequency = perf_count_frequency_result.QuadPart;

    UINT desired_scheduler_ms = 1;
    bool sleep_is_granular = timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR;

    win32_load_xinput();

    WNDCLASS window_class = {};

    resize_dib_section(&g_backbuffer, 1280, 720);

    window_class.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = main_window_callback;
    window_class.hInstance = instance;
    //window_class.hIcon;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

#define frames_of_audio_latency 3
#define monitor_refresh_hz 60
#define game_update_hz (monitor_refresh_hz / 2)
    float target_seconds_per_frame = 1.0f / float(game_update_hz);

    ATOM register_result = RegisterClass(&window_class);
    if (register_result == 0) return 1;
    HWND window = CreateWindowEx(
        0,
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
    HDC device_context = GetDC(window);

    SoundOutput sound_output = {};
    sound_output.latency_sample_count = frames_of_audio_latency * (sound_output.samples_per_second / game_update_hz);
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
    game_memory.transient_storage_size = gigabytes(4);
    uint64_t total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;

    game_memory.permanent_storage = VirtualAlloc(base_address, total_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    game_memory.transient_storage = (uint8_t *)game_memory.permanent_storage + game_memory.permanent_storage_size;

    GameInput inputs[2] = {};
    GameInput *new_input = &inputs[0];
    GameInput *old_input = &inputs[1];

    int debug_time_marker_index = 0;
    DebugTimeMarker debug_time_markers[game_update_hz / 2] = {};

    DWORD last_play_cursor = 0;
    bool sound_is_valid = false;

    LARGE_INTEGER last_counter = get_wall_clock();
    uint64_t last_cycle_count = __rdtsc();
    while (g_running) {
        GameControllerInput *new_keyboard_controller = get_controller(new_input, 0);
        GameControllerInput *old_keyboard_controller = get_controller(old_input, 0);
        *new_keyboard_controller = {};
        new_keyboard_controller->is_connected = true;
        for (int i = 0; i < array_count(new_keyboard_controller->buttons); i++) {
            new_keyboard_controller->buttons[i].ended_down = old_keyboard_controller->buttons[i].ended_down;
        }

        process_pending_messages(new_keyboard_controller);

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
        // sound stuff
        DWORD bytes_to_write = 0;
        DWORD byte_to_lock = 0;
        DWORD target_cursor = 0;
        if (sound_is_valid) {
            byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;
            target_cursor = (last_play_cursor + (sound_output.latency_sample_count * sound_output.bytes_per_sample)) % sound_output.secondary_buffer_size;
            if (byte_to_lock > target_cursor) {
                bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock + target_cursor;
            } else {
                bytes_to_write = target_cursor - byte_to_lock;
            }
        }

        GameOutputSoundBuffer sound_buffer = {};
        sound_buffer.samples_per_second = sound_output.samples_per_second;
        sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
        sound_buffer.samples = samples;

        GameOffscreenBuffer game_offscreen_buffer = {};
        game_offscreen_buffer.width = g_backbuffer.width;
        game_offscreen_buffer.height = g_backbuffer.height;
        game_offscreen_buffer.memory = g_backbuffer.memory;

        game_update_and_render(&game_memory, new_input, &game_offscreen_buffer, &sound_buffer);
        if (sound_is_valid) {
#if HANDMADE_INTERNAL
            DWORD play_cursor;
            DWORD write_cursor;
            g_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);
            char text_buffer[256];
            sprintf_s(text_buffer, sizeof(text_buffer), "LPC:%u BTL:%u TC:%u BTW:%u - PC:%u WC:%u\n", last_play_cursor, byte_to_lock, target_cursor, bytes_to_write, play_cursor, write_cursor);
            OutputDebugString(text_buffer);
#endif
            fill_sound_buffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
        }


        // fps stuff

        LARGE_INTEGER work_counter = get_wall_clock();
        float work_seconds_elapsed = get_seconds_elapsed(last_counter, work_counter);
        float seconds_elapsed_for_frame = work_seconds_elapsed;

        if (seconds_elapsed_for_frame < target_seconds_per_frame) {
            if (sleep_is_granular) {
                DWORD sleep_ms = (DWORD)(1000.0f * (target_seconds_per_frame - seconds_elapsed_for_frame));
                if (sleep_ms > 3) {
                    Sleep(sleep_ms - 3);
                }
            }
            float testSleep = get_seconds_elapsed(last_counter, get_wall_clock());
            assert(testSleep < target_seconds_per_frame);
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
#if HANDMADE_INTERNAL
        debug_sync_display(array_count(debug_time_markers), debug_time_markers, &sound_output, target_seconds_per_frame);
#endif
        display_buffer_in_window(&g_backbuffer, device_context, dim.width, dim.height);

        DWORD play_cursor;
        DWORD write_cursor;
        if (g_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor) == DS_OK) {
            last_play_cursor = play_cursor;
            if (!sound_is_valid) {
                sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
                sound_is_valid = true;
            }
        } else {
            sound_is_valid = false;
            assert(sound_is_valid);
        }
        // debug code
#if HANDMADE_INTERNAL
        {
            DebugTimeMarker *marker = &debug_time_markers[debug_time_marker_index++];
            if (debug_time_marker_index > array_count(debug_time_markers)) {
                debug_time_marker_index = 0;
            }
            marker->play_cursor = play_cursor;
            marker->write_cursor = write_cursor;
        }
#endif

        GameInput *temp = new_input;
        new_input = old_input;
        old_input = temp;

        uint64_t end_cycle_count = __rdtsc();
        uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
        last_cycle_count = end_cycle_count;

        double fps = 0.0f;
        double mcpf = (double)cycles_elapsed / (1000.0f * 1000.0f);

        char buffer[256];
        sprintf_s(buffer, "%.02fms/f, %.02ff/s, %.02fmc/f\n", ms_per_frame, fps, mcpf);
        OutputDebugString(buffer);
    }

    return 0;
}
