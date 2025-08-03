// Game Code - Platform Independent

struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
};


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

static void game_update_and_render(GameOffscreenBuffer *buffer, int x_offset, int y_offset) {
    render_weird_gradient(buffer, x_offset, y_offset);
}
