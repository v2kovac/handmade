// Windows Code - compile this file to get a windows app
#include <stdint.h>
#include <math.h>

#define BYTES_PER_PIXEL 4
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


struct OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
};
struct WindowDimension {
    int width;
    int height;
};
struct SoundOutput {
    int samples_per_second = 48000;
    uint32_t running_sample_index = 0;
    int bytes_per_sample = sizeof(int16_t) * 2;
    int secondary_buffer_size = samples_per_second * bytes_per_sample;
    int latency_sample_count = samples_per_second / 15;
    float t_sine = 0;
};

static bool g_running;
static OffscreenBuffer g_backbuffer;
static LPDIRECTSOUNDBUFFER g_secondary_buffer;

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
    buffer_description.dwFlags = 0;
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

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_size = (buffer->width * buffer->height) * BYTES_PER_PIXEL;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE); 
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
            case WM_QUIT: {
                g_running = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                uint32_t vk_code = (uint32_t)message.wParam;
                bool was_down = (message.lParam & (1 << 30)) != 0;
                bool is_down = (message.lParam & (1 << 31)) == 0;
                if (was_down == is_down) break;

                if (vk_code == 'W') {
                } else if (vk_code == 'A') {
                } else if (vk_code == 'S') {
                } else if (vk_code == 'D') {
                } else if (vk_code == 'Q') {
                    process_keyboard_message(&keyboard_controller->left_shoulder, is_down);
                } else if (vk_code == 'E') {
                    process_keyboard_message(&keyboard_controller->right_shoulder, is_down);
                } else if (vk_code == VK_UP) {
                    process_keyboard_message(&keyboard_controller->up, is_down);
                } else if (vk_code == VK_DOWN) {
                    process_keyboard_message(&keyboard_controller->down, is_down);
                } else if (vk_code == VK_LEFT) {
                    process_keyboard_message(&keyboard_controller->left, is_down);
                } else if (vk_code == VK_RIGHT) {
                    process_keyboard_message(&keyboard_controller->right, is_down);
                } else if (vk_code == VK_SPACE) {
                } else if (vk_code == VK_ESCAPE) {
                    g_running = false;
                }
            } break;
            default: {
                TranslateMessage(&message);
                DispatchMessage(&message);
            } break;
        }
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_code) {
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    int64_t perf_count_frequency = perf_count_frequency_result.QuadPart;

    WNDCLASS window_class = {};

    resize_dib_section(&g_backbuffer, 1280, 720);

    window_class.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = main_window_callback;
    window_class.hInstance = instance;
    //window_class.hIcon;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    ATOM register_result = RegisterClass(&window_class);
    if (register_result == 0) return -1;
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
    g_running = true;
    HDC device_context = GetDC(window);

    SoundOutput sound_output = {};
    init_dsound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
    clear_sound_buffer(&sound_output);
    g_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

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

    win32_load_xinput();
    GameInput inputs[2] = {};
    GameInput *new_input = &inputs[0];
    GameInput *old_input = &inputs[1];

    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);
    uint64_t last_cycle_count = __rdtsc();
    while (g_running) {
        GameControllerInput *keyboard_controller = &new_input->controllers[0];
        GameControllerInput zero_controller = {};
        *keyboard_controller = zero_controller;

        process_pending_messages(keyboard_controller);


        DWORD max_controller_count = XUSER_MAX_COUNT;
        if (max_controller_count > array_count(new_input->controllers)) {
            max_controller_count = array_count(new_input->controllers);
        }
        for (DWORD i = 0; i < max_controller_count; i++) {
            GameControllerInput *old_controller = &old_input->controllers[i];
            GameControllerInput *new_controller = &new_input->controllers[i];

            XINPUT_STATE controller_state;
            DWORD x_input_get_state_result = XInputGetState(i, &controller_state);
            if (x_input_get_state_result == ERROR_SUCCESS) {
                // controller is plugged in
                XINPUT_GAMEPAD *pad = &controller_state.Gamepad;

                // todo
                bool dpad_up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                bool dpad_down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                bool dpad_left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                bool dpad_right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
                bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);

                new_controller->is_analog = true;
                new_controller->start_x = old_controller->end_x;
                new_controller->start_y = old_controller->end_y;

                float x;
                if (pad->sThumbLX < 0) {
                    x = (float)pad->sThumbLX / 32768.0f;
                } else {
                    x = (float)pad->sThumbLX / 32767.0f;
                }
                new_controller->min_x = new_controller->max_x = new_controller->end_x = x;
                float y;
                if (pad->sThumbLY < 0) {
                    y = (float)pad->sThumbLY / 32768.0f;
                } else {
                    y = (float)pad->sThumbLY / 32767.0f;
                }
                new_controller->min_y = new_controller->max_y = new_controller->end_y = y;

                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_A, &old_controller->down, &new_controller->down);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_B, &old_controller->right, &new_controller->right);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_X, &old_controller->left, &new_controller->left);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_Y, &old_controller->right, &new_controller->right);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER, &old_controller->left_shoulder, &new_controller->left_shoulder);
                process_xinput_button(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER, &old_controller->right_shoulder, &new_controller->right_shoulder);
            }
        }
        // sound stuff
        DWORD play_cursor;
        DWORD write_cursor;
        if (!SUCCEEDED(g_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor))) return 2;

        DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;
        DWORD target_cursor = (play_cursor + (sound_output.latency_sample_count * sound_output.bytes_per_sample)) % sound_output.secondary_buffer_size;
        DWORD bytes_to_write;
        if (byte_to_lock > target_cursor) {
            bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock + target_cursor;
        } else {
            bytes_to_write = target_cursor - byte_to_lock;
        }

        int16_t samples[48000 * 2];
        GameOutputSoundBuffer sound_buffer = {};
        sound_buffer.samples_per_second = sound_output.samples_per_second;
        sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
        sound_buffer.samples = samples;

        GameOffscreenBuffer game_offscreen_buffer = {};
        game_offscreen_buffer.width = g_backbuffer.width;
        game_offscreen_buffer.height = g_backbuffer.height;
        game_offscreen_buffer.memory = g_backbuffer.memory;

        game_update_and_render(&game_memory, new_input, &game_offscreen_buffer, &sound_buffer);
        fill_sound_buffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);

        WindowDimension dim = get_window_dimension(window);
        display_buffer_in_window(&g_backbuffer, device_context, dim.width, dim.height);

        // fps stuff
        LARGE_INTEGER end_counter;
        QueryPerformanceCounter(&end_counter);
        int64_t counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
        int32_t ms_per_frame = (int32_t)((1000 * counter_elapsed) / perf_count_frequency);
        int32_t fps = (int32_t)(perf_count_frequency / counter_elapsed);

        uint64_t end_cycle_count = __rdtsc();
        uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
        int32_t mcpf = (int32_t)(cycles_elapsed / (1000 * 1000));

        // char buffer[256];
        // wsprintf(buffer, "Milliseconds/frame: %dms / %dFPS / %dmc/f\n", ms_per_frame, fps, mcpf);
        // OutputDebugString(buffer);
        last_counter = end_counter;
        last_cycle_count = end_cycle_count;

        GameInput *temp = new_input;
        new_input = old_input;
        old_input = temp;
    }

    return 0;
}
