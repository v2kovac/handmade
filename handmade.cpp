// Game Code - Platform Independent
#include "handmade.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"
#include "handmade_tile.cpp"
// TODO: remove this when we make our own rand func
#include <stdlib.h>

#define TONE_HZ_START 256

internal void game_output_sound(GameState *game_state, GameOutputSoundBuffer *sound_buffer, int tone_hz) {
    s16 tone_volume = 1000;
    s16 *sample_out = sound_buffer->samples;
    int wave_period = sound_buffer->samples_per_second / tone_hz;

    for (int sample_index = 0; sample_index < sound_buffer->sample_count; sample_index++) {
#if 0
        f32 sine_value = sinf(game_state->t_sine);
        s16 sample_value = (s16)(sine_value * tone_volume);
#else
        s16 sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

#if 0
        game_state->t_sine += 2.0f * PI32 * 1.0f / (f32)wave_period;
        if (game_state->t_sine > (2.0f * PI32)) {
            game_state->t_sine -= (2.0f * PI32);
        }
#endif
    }
}

internal void draw_rectangle(GameOffscreenBuffer *buffer,
                             v2 v_min, v2 v_max,
                             f32 r, f32 g, f32 b) {
    int min_x = round_f32_to_s32(v_min.x);
    int min_y = round_f32_to_s32(v_min.y);
    int max_x = round_f32_to_s32(v_max.x);
    int max_y = round_f32_to_s32(v_max.y);

    if (min_x < 0) {
        min_x = 0;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_x > buffer->width) {
        max_x = buffer->width;
    }
    if (max_y > buffer->height) {
        max_y = buffer->height;
    }

    u32 color = (round_f32_to_s32(r * 255.0f) << 16) |
                (round_f32_to_s32(g * 255.0f) << 8) |
                (round_f32_to_s32(b * 255.0f) << 0);

    u8 *row = (u8 *)buffer->memory + (min_x * buffer->bytes_per_pixel) + (min_y * buffer->pitch);
    for (s32 y = min_y; y < max_y; ++y) {
        u32 *pixel = (u32 *)row;
        for (s32 x = min_x; x < max_x; ++x) {
            *pixel++ = color;
        }
        row += buffer->pitch;
    }
}

internal void draw_bitmap(GameOffscreenBuffer *buffer, LoadedBitmap *bitmap,
                        f32 real_x, f32 real_y,
                        s32 align_x = 0, s32 align_y = 0) {
    real_x -= (f32)align_x;
    real_y -= (f32)align_y;
    s32 min_x = round_f32_to_s32(real_x);
    s32 min_y = round_f32_to_s32(real_y);
    s32 max_x = round_f32_to_s32(real_x + (f32)bitmap->width);
    s32 max_y = round_f32_to_s32(real_y + (f32)bitmap->height);

    s32 source_offset_x = 0;
    if (min_x < 0) {
        source_offset_x = -min_x;
        min_x = 0;
    }
    s32 source_offset_y = 0;
    if (min_y < 0) {
        source_offset_y = -min_y;
        min_y = 0;
    }
    if (max_x > buffer->width) {
        max_x = buffer->width;
    }
    if (max_y > buffer->height) {
        max_y = buffer->height;
    }

    u32 *source_row = bitmap->pixels+ (bitmap->width * (bitmap->height - 1));
    source_row += -source_offset_y * bitmap->width + source_offset_x;
    u8 *dest_row = (u8 *)buffer->memory + (min_x * buffer->bytes_per_pixel) + (min_y * buffer->pitch);
    for (s32 y = min_y; y < max_y; ++y) {
        u32 *dest = (u32 *)dest_row;
        u32 *source = source_row;
        for (s32 x = min_x; x < max_x; ++x) {
            f32 a = (f32)((*source >> 24) & 0xFF) / 255.0f;
            f32 sr = (f32)((*source >> 16) & 0xFF);
            f32 sg = (f32)((*source >> 8) & 0xFF);
            f32 sb = (f32)((*source >> 0) & 0xFF);

            f32 dr = (f32)((*dest >> 16) & 0xFF);
            f32 dg = (f32)((*dest >> 8) & 0xFF);
            f32 db = (f32)((*dest >> 0) & 0xFF);

            f32 r = ((1.0f - a) * dr) + (a * sr);
            f32 g = ((1.0f - a) * dg) + (a * sg);
            f32 b = ((1.0f - a) * db) + (a * sb);

            *dest = ((u32)(r + 0.5f) << 16) |
                    ((u32)(g + 0.5f) << 8) |
                    ((u32)(b + 0.5f) << 0);

            ++dest;
            ++source;
        }
        dest_row += buffer->pitch;
        source_row -= bitmap->width;
    }
}

#pragma pack(push, 1)
struct BitmapHeader {
    u16 file_type;
    u32 file_size;
    u16 reserved_1;
    u16 reserved_2;
    u32 bitmap_offset;
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 size_of_bitmap;
    s32 horz_resolution;
    s32 vert_resolution;
    u32 colors_used;
    u32 colors_important;

    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
};
#pragma pack(pop)

internal LoadedBitmap debug_load_bmp(ThreadContext *thread, debug_platform_read_entire_file_func *read_entire_file, char *filename) {
    DebugReadFileResult read_result = read_entire_file(thread, filename);
    assert(read_result.contents_size > 0);

    BitmapHeader *header = (BitmapHeader *)read_result.contents;
    u32 *pixels = (u32 *)((u8 *)read_result.contents + header->bitmap_offset);

    assert(header->compression == 3);

    // We want to conver the byte order to 0xAARRGGBB for compatibility with
    // our blit. Bitmap goes from bottom to top.
    u32 alpha_mask = ~(header->red_mask | header->green_mask | header->blue_mask);

    u32 red_shift = find_least_significant_set_bit(header->red_mask);
    u32 green_shift = find_least_significant_set_bit(header->green_mask);
    u32 blue_shift = find_least_significant_set_bit(header->blue_mask);
    u32 alpha_shift = find_least_significant_set_bit(alpha_mask);

    u32 *source_dest = pixels;
    for (s32 y = 0; y < header->height; ++y) {
        for (s32 x = 0; x < header->width; ++x) {
            u32 c = *source_dest;
            *source_dest++ = (((c >> alpha_shift) & 0xFF) << 24) |
                             (((c >> red_shift) & 0xFF) << 16) |
                             (((c >> green_shift) & 0xFF) << 8) |
                             (((c >> blue_shift) & 0xFF) << 0);
        }
    }

    LoadedBitmap bitmap = {};
    bitmap.pixels = pixels;
    bitmap.height = header->height;
    bitmap.width = header->width;
    return bitmap;
}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples) {
    GameState *game_state = (GameState *)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer, 400);
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

    f32 player_height = 1.4f;
    f32 player_width = 0.75f * player_height;

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized) {
        game_state->backdrop =
            debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_background.bmp");

        HeroBitmaps *bitmap = game_state->hero_bitmaps;

        bitmap->head = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_right_head.bmp");
        bitmap->cape = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_right_cape.bmp");
        bitmap->torso = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_right_torso.bmp");
        bitmap->align_x = 72;
        bitmap->align_y = 182;
        bitmap++;

        bitmap->head = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_back_head.bmp");
        bitmap->cape = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_back_cape.bmp");
        bitmap->torso = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_back_torso.bmp");
        bitmap->align_x = 72;
        bitmap->align_y = 182;
        bitmap++;

        bitmap->head = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_left_head.bmp");
        bitmap->cape = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_left_cape.bmp");
        bitmap->torso = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_left_torso.bmp");
        bitmap->align_x = 72;
        bitmap->align_y = 182;
        bitmap++;

        bitmap->head = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_front_head.bmp");
        bitmap->cape = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_front_cape.bmp");
        bitmap->torso = debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_front_torso.bmp");
        bitmap->align_x = 72;
        bitmap->align_y = 182;

        game_state->hero_facing_direction = 0;

        game_state->camera_p.abs_tile_x = 17/2;
        game_state->camera_p.abs_tile_y = 9/2;

        game_state->player_p.abs_tile_x = 1;
        game_state->player_p.abs_tile_y = 3;
        game_state->player_p.abs_tile_z = 0;
        game_state->player_p.offset.x = 5.0f;
        game_state->player_p.offset.y = 5.0f;

        initialize_arena(&game_state->world_arena, memory->permanent_storage_size - sizeof(GameState), (u8 *)memory->permanent_storage + sizeof(GameState));
        game_state->world = push_struct(&game_state->world_arena, World);
        World *world = game_state->world;
        world->tile_map = push_struct(&game_state->world_arena, TileMap);

        TileMap *tile_map = world->tile_map;

        tile_map->chunk_shift = 4;
        tile_map->chunk_mask = (1 << tile_map->chunk_shift) - 1;
        tile_map->chunk_dim = (1 << tile_map->chunk_shift);
        tile_map->tile_chunk_count_x = 128;
        tile_map->tile_chunk_count_y = 128;
        tile_map->tile_chunk_count_z = 2;

        tile_map->tile_chunks = push_array(&game_state->world_arena,
                                           tile_map->tile_chunk_count_x *
                                           tile_map->tile_chunk_count_y *
                                           tile_map->tile_chunk_count_z,
                                           TileChunk);

        tile_map->tile_side_in_meters = 1.4f;

        u32 tiles_per_width = 17;
        u32 tiles_per_height = 9;
        u32 screen_y = 0;
        u32 screen_x = 0;
        u32 abs_tile_z = game_state->player_p.abs_tile_z;
        bool door_right = false;
        bool door_left = false;
        bool door_top = false;
        bool door_bottom = false;
        bool door_up = false;
        bool door_down = false;
        for (u32 screen_index = 0; screen_index < 100; ++screen_index) {
            u32 random_choice;
            // avoid up -> down -> up  or vice versa
            if (door_up || door_down) {
                random_choice = rand() % 2;
            } else {
                random_choice = rand() % 3;
            }
            if (random_choice == 0) {
                door_top = true;
            } else if (random_choice == 1) {
                door_right = true;
            } else if (random_choice == 2) {
                if (abs_tile_z == 0) {
                    door_up = true;
                } else {
                    door_down = true;
                }
            }
            for (u32 tile_y = 0; tile_y < tiles_per_height; ++tile_y) {
                for (u32 tile_x = 0; tile_x < tiles_per_width; ++tile_x) {
                    u32 abs_tile_x = screen_x * tiles_per_width + tile_x;
                    u32 abs_tile_y = screen_y * tiles_per_height + tile_y;

                    u32 tile_value = 1;

                    // left side
                    if (tile_x == 0 && !(door_left && tile_y == tiles_per_height / 2)) {
                        tile_value = 2;
                    }
                    // right side
                    if ((tile_x == tiles_per_width - 1) && !(door_right && tile_y == tiles_per_height / 2)) {
                        tile_value = 2;
                    }
                    // bottom side
                    if (tile_y == 0 && !(door_bottom && tile_x == tiles_per_width / 2)) {
                        tile_value = 2;
                    }
                    // top side
                    if ((tile_y == tiles_per_height - 1) && !(door_top && tile_x == tiles_per_width / 2)) {
                        tile_value = 2;
                    }

                    if (tile_x == 10 && tile_y == 6) {
                        if (door_up) {
                            tile_value = 3;
                        } else if (door_down) {
                            tile_value = 4;
                        }
                    }

                    set_tile_value(&game_state->world_arena, world->tile_map,
                                   abs_tile_x, abs_tile_y, abs_tile_z,
                                   tile_value);
                }
            }
            if (random_choice == 0) {
                screen_y += 1;
            } else if (random_choice == 1) {
                screen_x += 1;
            } else if (random_choice == 2) {
                if (abs_tile_z == 0) {
                    abs_tile_z = 1;
                } else {
                    abs_tile_z = 0;
                }
            }
            door_left = door_right;
            door_bottom = door_top;
            door_top = false;
            door_right = false;
            if (random_choice == 2) {
                door_up = !door_up;
                door_down = !door_down;
            } else {
                door_down = false;
                door_up = false;
            }
        }

        memory->is_initialized = true;
    }

    World *world = game_state->world;
    TileMap *tile_map = world->tile_map;

    s32 tile_side_in_pixels = 60;
    f32 meters_to_pixels = (f32)tile_side_in_pixels / tile_map->tile_side_in_meters;

    f32 lower_left_x = -((f32)tile_side_in_pixels / 2);
    f32 lower_left_y = (f32)buffer->height;

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput *controller = get_controller(input, i);
        if (controller->is_analog) {
        } else {
            v2 d_player = {};
            if (controller->move_up.ended_down) {
                game_state->hero_facing_direction = 1;
                d_player.y = 1.0f;
            }
            if (controller->move_down.ended_down) {
                game_state->hero_facing_direction = 3;
                d_player.y = -1.0f;
            }
            if (controller->move_left.ended_down) {
                game_state->hero_facing_direction = 2;
                d_player.x = -1.0f;
            }
            if (controller->move_right.ended_down) {
                game_state->hero_facing_direction = 0;
                d_player.x = 1.0f;
            }
            f32 player_speed = 2.0f;
            if (controller->action_up.ended_down) {
                player_speed = 10.0f;
            }
            d_player *= player_speed;

            if (d_player.x != 0.0f && d_player.y != 0.0f) {
                d_player *= 0.707106781187;
            }

            TileMapPosition new_player_p = game_state->player_p;
            new_player_p.offset += input->dt_for_frame * d_player;
            new_player_p = recanonicalize_position(tile_map, new_player_p);

            TileMapPosition new_player_p_left = new_player_p;
            new_player_p_left.offset.x -= 0.5f * player_width;
            new_player_p_left = recanonicalize_position(tile_map, new_player_p_left);

            TileMapPosition new_player_p_right = new_player_p;
            new_player_p_right.offset.x += 0.5f * player_width;
            new_player_p_right = recanonicalize_position(tile_map, new_player_p_right);

            if (is_tile_map_point_empty(tile_map, new_player_p) &&
                is_tile_map_point_empty(tile_map, new_player_p_left) &&
                is_tile_map_point_empty(tile_map, new_player_p_right))
            {
                if (!are_on_same_tile(&game_state->player_p, &new_player_p)) {
                    u32 new_tile_value = get_tile_value(tile_map, new_player_p);
                    if (new_tile_value == 3) {
                        ++new_player_p.abs_tile_z;
                    } else if (new_tile_value == 4) {
                        --new_player_p.abs_tile_z;
                    }
                }
                game_state->player_p = new_player_p;
            }

            game_state->camera_p.abs_tile_z = game_state->player_p.abs_tile_z;
            TileMapDifference diff = subtract(tile_map, &game_state->player_p, &game_state->camera_p);
            if (diff.d_xy.x > (9.0f * tile_map->tile_side_in_meters)) {
                game_state->camera_p.abs_tile_x += 17;
            } else if (diff.d_xy.x < -(9.0f * tile_map->tile_side_in_meters)) {
                game_state->camera_p.abs_tile_x -= 17;
            }
            if (diff.d_xy.y > (5.0f * tile_map->tile_side_in_meters)) {
                game_state->camera_p.abs_tile_y += 9;
            } else if (diff.d_xy.y < -(5.0f * tile_map->tile_side_in_meters)) {
                game_state->camera_p.abs_tile_y -= 9;
            }
        }
    }

    draw_bitmap(buffer, &game_state->backdrop, 0, 0);

    f32 screen_center_x = 0.5f * (f32)buffer->width;
    f32 screen_center_y = 0.5f * (f32)buffer->height;

    for (s32 relrow = -10; relrow < 10; ++relrow) {
        for (s32 relcol = -20; relcol < 20; ++relcol) {
            u32 col = relcol + game_state->camera_p.abs_tile_x;
            u32 row = relrow + game_state->camera_p.abs_tile_y;
            u32 tile_id = get_tile_value(tile_map, col, row, game_state->camera_p.abs_tile_z);
            f32 gray = 0.5f;
            if (tile_id > 1) {
                if (tile_id == 2) {
                    gray = 1.0f;
                }
                if (tile_id > 2) {
                    gray = 0.25f;
                }
                if (row == game_state->camera_p.abs_tile_y && col == game_state->camera_p.abs_tile_x) {
                    gray = 0.0f;
                }
                v2 tile_side = {
                    0.5f * tile_side_in_pixels,
                    0.5f * tile_side_in_pixels
                };
                v2 cen = {
                    screen_center_x - (meters_to_pixels * game_state->camera_p.offset.x) + (f32)relcol * tile_side_in_pixels,
                    screen_center_y + (meters_to_pixels * game_state->camera_p.offset.y) - (f32)relrow * tile_side_in_pixels
                };
                v2 min = cen - tile_side;
                v2 max = cen + tile_side;
                draw_rectangle(buffer, min, max, gray, gray, gray);
            }
        }
    }
    TileMapDifference diff = subtract(tile_map, &game_state->player_p, &game_state->camera_p);
    f32 player_r = 1.0f;
    f32 player_g = 1.0f;
    f32 player_b = 0.0f;
    f32 player_ground_point_x = screen_center_x + meters_to_pixels * diff.d_xy.x;
    f32 player_ground_point_y = screen_center_y - meters_to_pixels * diff.d_xy.y;
    v2 player_left_top = {
        player_ground_point_x - (0.5f * meters_to_pixels * player_width),
        player_ground_point_y - (meters_to_pixels * player_height)
    };
    v2 player_width_height = {
        meters_to_pixels * player_width,
        meters_to_pixels * player_height
    };

    draw_rectangle(buffer, player_left_top,
                   player_left_top + player_width_height,
                   player_r, player_g, player_b);

    HeroBitmaps *hero_bitmaps = &game_state->hero_bitmaps[game_state->hero_facing_direction];
    draw_bitmap(buffer, &hero_bitmaps->torso, player_ground_point_x, player_ground_point_y, hero_bitmaps->align_x, hero_bitmaps->align_y);
    draw_bitmap(buffer, &hero_bitmaps->cape, player_ground_point_x, player_ground_point_y, hero_bitmaps->align_x, hero_bitmaps->align_y);
    draw_bitmap(buffer, &hero_bitmaps->head, player_ground_point_x, player_ground_point_y, hero_bitmaps->align_x, hero_bitmaps->align_y);
}

/*
internal void render_weird_gradient(GameOffscreenBuffer *buffer, int x_offset, int y_offset) {
    u32 *arr = (u32 *)buffer->memory;
    for (int y = 0; y < buffer->height; y++) {
        for (int x = 0; x < buffer->width; x++) {
            u8 blue = (u8)(x + x_offset);
            u8 green = (u8)(y + y_offset);
            *arr++ = ((green << 16) | blue);
        }
    }
}
*/
