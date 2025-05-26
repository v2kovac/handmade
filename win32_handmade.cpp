#include <windows.h>
#include <stdint.h>

#define BYTES_PER_PIXEL 4

struct OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
};

static bool running;
static OffscreenBuffer g_backbuffer;

static void render_weird_gradient(OffscreenBuffer buffer, int x_offset, int y_offset) {
    uint32_t *arr = (uint32_t *)buffer.memory;
    for (int y = 0; y < buffer.height; y++) {
        for (int x = 0; x < buffer.width; x++) {
            uint8_t blue = x + x_offset;
            uint8_t green = y + y_offset;
            *arr++ = ((green << 8) | blue);
        }
    }
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
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE); 

    render_weird_gradient(*buffer, 0, 0);
}

static void update_window(HDC device_context, RECT client_rect, OffscreenBuffer buffer, int x, int y, int width, int height) {
    int window_width = client_rect.right - client_rect.left;
    int window_height = client_rect.bottom - client_rect.top;
    StretchDIBits(device_context, 0, 0, buffer.width, buffer.height, 0, 0, window_width, window_height, buffer.memory, &buffer.info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT main_window_callback(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch (message) {
        case WM_SIZE:
        {
            RECT client_rect;
            GetClientRect(window, &client_rect);
            int width = client_rect.right - client_rect.left;
            int height = client_rect.bottom - client_rect.top;
            resize_dib_section(&g_backbuffer, width, height);
            OutputDebugString("WM_SIZE\n");
        } break;

        case WM_CLOSE: case WM_DESTROY:
        {
            running = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugString("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;

            RECT client_rect;
            GetClientRect(window, &client_rect);

            update_window(device_context, client_rect, g_backbuffer, x, y, width, height);
            EndPaint(window, &paint);
        } break;

        default:
        {
            result = DefWindowProc(window, message, w_param, l_param);
        } break;
    }

    return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_code) {
    WNDCLASS window_class = {};
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
    if (window == NULL) return -1;
    MSG message;
    running = true;
    int x_offset = 0;
    int y_offset = 0;
    while(running) {
        BOOL message_result = PeekMessage(&message, 0, 0, 0, PM_REMOVE);
        if (message.message == WM_QUIT) {
            running = false;
        }
        if (message_result) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        render_weird_gradient(g_backbuffer, x_offset++, y_offset);

        HDC device_context = GetDC(window);
        RECT client_rect;
        GetClientRect(window, &client_rect);
        int width = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;
        update_window(device_context, client_rect, g_backbuffer, 0, 0, width, height);
        ReleaseDC(window, device_context);
    }

    return 0;
}
