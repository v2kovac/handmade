#ifndef HANDMADE_INTRINSICS_H
#define HANDMADE_INTRINSICS_H

#include "math.h"

inline static s32 round_f32_to_s32(f32 float_32) {
    return (s32)roundf(float_32);
}

inline static u32 round_f32_to_u32(f32 float_32) {
    return (u32)roundf(float_32);
}

inline static s32 truncate_f32_to_s32(f32 float_32) {
    return (s32)float_32;
}

inline static s32 floor_f32_to_s32(f32 float_32) {
    return (s32)floorf(float_32);
}

inline static f32 sin(f32 angle) {
    return sinf(angle);
}

inline static f32 cos(f32 angle) {
    return cosf(angle);
}

inline static f32 atan2(f32 y, f32 x) {
    return atan2f(y, x);
}

inline static u32 find_least_significant_set_bit(u32 value) {
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

#endif
