// Game Code - Platform Independent
#include "handmade.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"
#include "handmade_tile.cpp"
// TODO: remove this when we make our own rand func
#include <stdlib.h>

#define TONE_HZ_START 256

internal void game_output_sound(GameState* game_state, GameOutputSoundBuffer* sound_buffer, int tone_hz) {
    s16 tone_volume = 1000;
    s16* sample_out = sound_buffer->samples;
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

internal void draw_rectangle(GameOffscreenBuffer* buffer,
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

    u8* row = (u8*)buffer->memory + (min_x * buffer->bytes_per_pixel) + (min_y * buffer->pitch);
    for (s32 y = min_y; y < max_y; ++y) {
        u32* pixel = (u32*)row;
        for (s32 x = min_x; x < max_x; ++x) {
            *pixel++ = color;
        }
        row += buffer->pitch;
    }
}

internal void draw_bitmap(GameOffscreenBuffer* buffer, LoadedBitmap* bitmap,
                          f32 real_x, f32 real_y,
                          s32 align_x = 0, s32 align_y = 0,
                          f32 c_alpha = 1.0f) {
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

    u32* source_row = bitmap->pixels + (bitmap->width * (bitmap->height - 1));
    source_row += -source_offset_y * bitmap->width + source_offset_x;
    u8* dest_row = (u8*)buffer->memory + (min_x * buffer->bytes_per_pixel) + (min_y * buffer->pitch);
    for (s32 y = min_y; y < max_y; ++y) {
        u32* dest = (u32*)dest_row;
        u32* source = source_row;
        for (s32 x = min_x; x < max_x; ++x) {
            f32 a = (f32)((*source >> 24) & 0xFF) / 255.0f;
            a *= c_alpha;
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

internal LoadedBitmap debug_load_bmp(ThreadContext* thread, debug_platform_read_entire_file_func* read_entire_file, char* filename) {
    DebugReadFileResult read_result = read_entire_file(thread, filename);
    assert(read_result.contents_size > 0);

    BitmapHeader* header = (BitmapHeader*)read_result.contents;
    u32* pixels = (u32*)((u8*)read_result.contents + header->bitmap_offset);

    assert(header->compression == 3);

    // We want to conver the byte order to 0xAARRGGBB for compatibility with
    // our blit. Bitmap goes from bottom to top.
    u32 alpha_mask = ~(header->red_mask | header->green_mask | header->blue_mask);

    u32 red_shift = find_least_significant_set_bit(header->red_mask);
    u32 green_shift = find_least_significant_set_bit(header->green_mask);
    u32 blue_shift = find_least_significant_set_bit(header->blue_mask);
    u32 alpha_shift = find_least_significant_set_bit(alpha_mask);

    u32* source_dest = pixels;
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

internal void change_entity_residence(GameState* game_state, u32 entity_index, EntityResidence residence) {
    if (residence == ER_HIGH && game_state->entity_residencies[entity_index] != ER_HIGH) {
        HighEntity* high = &game_state->high_entities[entity_index];
        DormantEntity* dormant = &game_state->dormant_entities[entity_index];

        TileMapDifference diff = subtract(game_state->world->tile_map, &dormant->p, &game_state->camera_p);
        high->p = diff.d_xy;
        high->dp = v2{0,0};
        high->abs_tile_z = dormant->p.abs_tile_z;
        high->facing_direction = 0;
    }
    game_state->entity_residencies[entity_index] = residence;
}

internal inline Entity get_entity(GameState* game_state, EntityResidence residence, u32 index) {
    Entity entity = {};

    if (index > 0 && index < game_state->entity_count) {
        if (game_state->entity_residencies[index] < residence) {
            change_entity_residence(game_state, index, residence);
            assert(game_state->entity_residencies[index] >= residence);
        }
        entity.residence = residence;
        entity.dormant = &game_state->dormant_entities[index];
        entity.low = &game_state->low_entities[index];
        entity.high = &game_state->high_entities[index];
    }

    return entity;
}

internal void initialize_player(GameState* game_state, u32 entity_index) {
    Entity entity = get_entity(game_state, ER_DORMANT, entity_index);

    entity.high->facing_direction = 0;
    entity.dormant->p.abs_tile_x = 1;
    entity.dormant->p.abs_tile_y = 3;
    entity.dormant->p.abs_tile_z = 0;
    entity.dormant->p.offset_.x = 0;
    entity.dormant->p.offset_.y = 0;
    entity.dormant->height = 0.5f;
    entity.dormant->width = 1.0f;
    entity.dormant->collides = true;

    change_entity_residence(game_state, entity_index, ER_HIGH);

    if (get_entity(game_state, ER_DORMANT, game_state->camera_following_entity_index).residence == ER_NONEXISTENT) {
        game_state->camera_following_entity_index = entity_index;
    }
}

internal u32 add_entity(GameState* game_state) {
    u32 entity_index = game_state->entity_count++;

    assert(game_state->entity_count < array_count(game_state->dormant_entities));
    assert(game_state->entity_count < array_count(game_state->low_entities));
    assert(game_state->entity_count < array_count(game_state->high_entities));
    assert(game_state->entity_count < array_count(game_state->entity_residencies));

    game_state->entity_residencies[entity_index] = ER_DORMANT;
    game_state->dormant_entities[entity_index] = {};
    game_state->low_entities[entity_index] = {};
    game_state->high_entities[entity_index] = {};

    return entity_index;
}

internal bool test_wall(f32 wall_x, f32 rel_x, f32 rel_y,
                        f32 player_delta_x, f32 player_delta_y,
                        f32* t_min, f32 min_y, f32 max_y)
{
    bool hit = false;;
    f32 t_epsilon = 0.001f;
    if (player_delta_x != 0.0f) {
        f32 t_result = (wall_x - rel_x) / player_delta_x;
        f32 y = rel_y + t_result * player_delta_y;
        if (t_result >= 0.0f && *t_min > t_result && y >= min_y && y <= max_y) {
            hit = true;
            *t_min = max(0.0f, t_result - t_epsilon);
        }
    }

    return hit;
}

internal void move_player(GameState* game_state, Entity entity, f32 dt, v2 ddp) {
    TileMap* tile_map = game_state->world->tile_map;

    f32 ddp_length = length_sq(ddp);
    if (ddp_length > 1.0f) {
        ddp *= (1.0f / square_root(ddp_length));
    }

    f32 player_speed = 50.0f;
    ddp *= player_speed;
    // friction
    ddp += -8.0f * entity.high->dp;

    v2 old_player_p = entity.high->p;
    // p' = (1/2 * a * t^2) + (v * t) + p
    v2 player_delta = (0.5f * ddp * square(dt)) +
                      (entity.high->dp * dt);
    // v' = a * t + v
    entity.high->dp = ddp * dt + entity.high->dp;

    v2 new_player_p = old_player_p + player_delta;

    /*
    u32 min_tile_x = min(old_player_p.abs_tile_x, new_player_p.abs_tile_x);
    u32 min_tile_y = min(old_player_p.abs_tile_y, new_player_p.abs_tile_y);
    u32 max_tile_x = max(old_player_p.abs_tile_x, new_player_p.abs_tile_x);
    u32 max_tile_y = max(old_player_p.abs_tile_y, new_player_p.abs_tile_y);

    u32 entity_tile_width = ceil_f32_to_s32(entity->width / tile_map->tile_side_in_meters);
    u32 entity_tile_height = ceil_f32_to_s32(entity->height / tile_map->tile_side_in_meters);

    min_tile_x -= entity_tile_width;
    min_tile_y -= entity_tile_height;
    max_tile_x += entity_tile_width;
    max_tile_y += entity_tile_height;

    u32 abs_tile_z = entity->p.abs_tile_z;
    */

    f32 t_remaining = 1.0f;
    for (u32 i = 0; i < 4 && t_remaining > 0.0f; ++i) {
        f32 t_min = 1.0f;
        v2 wall_normal = {};
        u32 hit_entity_index = 0;
        for (u32 entity_index = 1; entity_index < game_state->entity_count; ++entity_index) {
            Entity test_entity = get_entity(game_state, ER_HIGH, entity_index);

            if (test_entity.high != entity.high && test_entity.dormant->collides) {
                f32 diameter_w = test_entity.dormant->width + entity.dormant->width;
                f32 diameter_h = test_entity.dormant->height + entity.dormant->height;

                v2 min_corner = -0.5f * v2{diameter_w, diameter_h};
                v2 max_corner = 0.5f * v2{diameter_w, diameter_h};

                v2 rel = entity.high->p - test_entity.high->p;

                if (test_wall(min_corner.x,
                              rel.x, rel.y, player_delta.x, player_delta.y,
                              &t_min, min_corner.y, max_corner.y))
                {
                    wall_normal = v2{-1, 0};
                    hit_entity_index = entity_index;
                }
                if (test_wall(max_corner.x,
                              rel.x, rel.y, player_delta.x, player_delta.y,
                              &t_min, min_corner.y, max_corner.y))
                {
                    wall_normal = v2{1, 0};
                    hit_entity_index = entity_index;
                }
                if (test_wall(min_corner.y,
                              rel.y, rel.x, player_delta.y, player_delta.x,
                              &t_min, min_corner.x, max_corner.x))
                {
                    wall_normal = v2{0, -1};
                    hit_entity_index = entity_index;
                }
                if (test_wall(max_corner.y,
                              rel.y, rel.x, player_delta.y, player_delta.x,
                              &t_min, min_corner.x, max_corner.x))
                {
                    wall_normal = v2{0, 1};
                    hit_entity_index = entity_index;
                }
            }
        }

        entity.high->p += t_min * player_delta;
        if (hit_entity_index) {
            entity.high->dp = entity.high->dp - (1 * inner(entity.high->dp, wall_normal) * wall_normal);
            player_delta = player_delta - (1 * inner(player_delta, wall_normal) * wall_normal);
            t_remaining -= (t_min * t_remaining);

            Entity hit_entity = get_entity(game_state, ER_DORMANT, hit_entity_index);
            entity.high->abs_tile_z += hit_entity.dormant->d_abs_tile_z;
        } else {
            break;
        }
    }

    if (entity.high->dp.x == 0.0f && entity.high->dp.y == 0.0f) {
        // leave facing direction as it was
    } else if (absolute_value(entity.high->dp.x) > absolute_value(entity.high->dp.y)) {
        if (entity.high->dp.x > 0) {
            entity.high->facing_direction = 0;
        } else {
            entity.high->facing_direction = 2;
        }
    } else {
        if (entity.high->dp.y > 0) {
            entity.high->facing_direction = 1;
        } else {
            entity.high->facing_direction = 3;
        }
    }

    entity.dormant->p = map_to_tile_space(game_state->world->tile_map, game_state->camera_p, entity.high->p);
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == array_count(input->controllers[0].buttons));
    assert(sizeof(GameState) <= memory->permanent_storage_size);

    GameState* game_state = (GameState*)memory->permanent_storage;
    if (!memory->is_initialized) {
        // reserve slot 0 as null entity
        add_entity(game_state);

        game_state->backdrop =
            debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_background.bmp");
        game_state->shadow =
            debug_load_bmp(thread, memory->debug_platform_read_entire_file, "test/test_hero_shadow.bmp");

        HeroBitmaps* bitmap = game_state->hero_bitmaps;

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

        game_state->camera_p.abs_tile_x = 17/2;
        game_state->camera_p.abs_tile_y = 9/2;

        initialize_arena(&game_state->world_arena, memory->permanent_storage_size - sizeof(GameState), (u8*)memory->permanent_storage + sizeof(GameState));
        game_state->world = push_struct(&game_state->world_arena, World);
        World* world = game_state->world;
        world->tile_map = push_struct(&game_state->world_arena, TileMap);

        TileMap* tile_map = world->tile_map;

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
        u32 abs_tile_z = 0;
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

    World* world = game_state->world;
    TileMap* tile_map = world->tile_map;

    s32 tile_side_in_pixels = 60;
    f32 meters_to_pixels = (f32)tile_side_in_pixels / tile_map->tile_side_in_meters;

    f32 lower_left_x = -((f32)tile_side_in_pixels / 2);
    f32 lower_left_y = (f32)buffer->height;

    for (int i = 0; i < array_count(input->controllers); i++) {
        GameControllerInput* controller = get_controller(input, i);
        Entity controlling_entity = get_entity(game_state, ER_HIGH, game_state->player_index_for_controller[i]);
        if (controlling_entity.residence != ER_NONEXISTENT) {
            v2 ddp = {};
            if (controller->is_analog) {
                ddp = v2{controller->stick_avg_x, controller->stick_avg_y};
            } else {

                if (controller->move_up.ended_down) {
                    ddp.y = 1.0f;
                }
                if (controller->move_down.ended_down) {
                    ddp.y = -1.0f;
                }
                if (controller->move_left.ended_down) {
                    ddp.x = -1.0f;
                }
                if (controller->move_right.ended_down) {
                    ddp.x = 1.0f;
                }
            }
            if (controller->action_up.ended_down) {
                controlling_entity.high->dz = 3.0f;
            }
            move_player(game_state, controlling_entity, input->dt_for_frame, ddp);
        } else {
            if (controller->start.ended_down) {
                u32 entity_index = add_entity(game_state);
                initialize_player(game_state, entity_index);
                game_state->player_index_for_controller[i] = entity_index;
            }
        }
    }

    v2 entity_offset_for_frame = {};
    Entity camera_following_entity = get_entity(game_state, ER_HIGH, game_state->camera_following_entity_index);
    if (camera_following_entity.residence != ER_NONEXISTENT) {
        TileMapPosition old_camera_p = game_state->camera_p;
        game_state->camera_p.abs_tile_z = camera_following_entity.dormant->p.abs_tile_z;
        if (camera_following_entity.high->p.x > (9.0f * tile_map->tile_side_in_meters)) {
            game_state->camera_p.abs_tile_x += 17;
        } else if (camera_following_entity.high->p.x < -(9.0f * tile_map->tile_side_in_meters)) {
            game_state->camera_p.abs_tile_x -= 17;
        }
        if (camera_following_entity.high->p.y > (5.0f * tile_map->tile_side_in_meters)) {
            game_state->camera_p.abs_tile_y += 9;
        } else if (camera_following_entity.high->p.y < -(5.0f * tile_map->tile_side_in_meters)) {
            game_state->camera_p.abs_tile_y -= 9;
        }

        TileMapDifference d_camera_p = subtract(tile_map, &game_state->camera_p, &old_camera_p);
        entity_offset_for_frame = -d_camera_p.d_xy;
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
                    screen_center_x - (meters_to_pixels * game_state->camera_p.offset_.x) + (f32)relcol * tile_side_in_pixels,
                    screen_center_y + (meters_to_pixels * game_state->camera_p.offset_.y) - (f32)relrow * tile_side_in_pixels
                };
                v2 min = cen - 0.9f * tile_side;
                v2 max = cen + 0.9f * tile_side;
                draw_rectangle(buffer, min, max, gray, gray, gray);
            }
        }
    }

    for (u32 entity_index = 0; entity_index < game_state->entity_count; ++entity_index) {
        if (game_state->entity_residencies[entity_index] == ER_HIGH) {
            HighEntity* high_entity = &game_state->high_entities[entity_index];
            DormantEntity* dormant_entity = &game_state->dormant_entities[entity_index];

            high_entity->p += entity_offset_for_frame;

            f32 dt = input->dt_for_frame;
            f32 ddz = -9.8f;
            high_entity->z = (0.5f * ddz * square(dt)) + (high_entity->dz * dt) + high_entity->z;
            high_entity->dz = (ddz * dt) + high_entity->dz;
            if (high_entity->z < 0) {
                high_entity->z = 0;
            }
            f32 c_alpha = 1.0f - (0.5f * high_entity->z);
            if (c_alpha < 0) {
                c_alpha = 0;
            }

            f32 player_r = 1.0f;
            f32 player_g = 1.0f;
            f32 player_b = 0.0f;
            f32 player_ground_point_x = screen_center_x + meters_to_pixels * high_entity->p.x;
            f32 player_ground_point_y = screen_center_y - meters_to_pixels * high_entity->p.y;
            f32 z = -meters_to_pixels * high_entity->z;
            v2 player_left_top = {
                player_ground_point_x - (0.5f * meters_to_pixels * dormant_entity->width),
                player_ground_point_y - (0.5f * meters_to_pixels * dormant_entity->height)
            };
            v2 entity_width_height = {
                meters_to_pixels * dormant_entity->width,
                meters_to_pixels * dormant_entity->height
            };

#if 0
            draw_rectangle(buffer, player_left_top,
                           player_left_top + entity_width_height,
                           player_r, player_g, player_b);
#endif

            HeroBitmaps* hero_bitmaps = &game_state->hero_bitmaps[high_entity->facing_direction];
            draw_bitmap(buffer, &game_state->shadow, player_ground_point_x, player_ground_point_y, hero_bitmaps->align_x, hero_bitmaps->align_y, c_alpha);
            draw_bitmap(buffer, &hero_bitmaps->torso, player_ground_point_x, player_ground_point_y + z, hero_bitmaps->align_x, hero_bitmaps->align_y);
            draw_bitmap(buffer, &hero_bitmaps->cape, player_ground_point_x, player_ground_point_y + z, hero_bitmaps->align_x, hero_bitmaps->align_y);
            draw_bitmap(buffer, &hero_bitmaps->head, player_ground_point_x, player_ground_point_y + z, hero_bitmaps->align_x, hero_bitmaps->align_y);
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples) {
    GameState* game_state = (GameState*)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer, 400);
}

