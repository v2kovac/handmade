#pragma once
#include "math.h"

internal inline f32 square_root(f32 float_32) {
    f32 result = sqrtf(float_32);
    return result;
}

internal inline f32 absolute_value(f32 float_32) {
    f32 result = fabsf(float_32);
    return result;
}

internal inline s32 round_f32_to_s32(f32 float_32) {
    return (s32)roundf(float_32);
}

internal inline u32 round_f32_to_u32(f32 float_32) {
    return (u32)roundf(float_32);
}

internal inline s32 truncate_f32_to_s32(f32 float_32) {
    return (s32)float_32;
}

internal inline s32 floor_f32_to_s32(f32 float_32) {
    return (s32)floorf(float_32);
}

internal inline s32 ceil_f32_to_s32(f32 float_32) {
    return (s32)ceilf(float_32);
}

internal inline f32 sin(f32 angle) {
    return sinf(angle);
}

internal inline f32 cos(f32 angle) {
    return cosf(angle);
}

internal inline f32 atan2(f32 y, f32 x) {
    return atan2f(y, x);
}

internal inline u32 find_least_significant_set_bit(u32 value) {
#if COMPILER_MSVC
    u32 index;
    bool found = _BitScanForward((unsigned long *)&index, value);
    assert(found);
    return index;
#else
    for (u32 x = 0; x < 32; ++x) {
        if (value & (1 << x)) {
            return x;
        }
    }
    assert(!"not found");
    return 0;
#endif
}
