#ifndef HANDMADE_INTRINSICS_H
#define HANDMADE_INTRINSICS_H

#include "math.h"

inline static s32 round_f32_to_s32(f32 float_32) {
    return (s32)(float_32 + 0.5f);
}

inline static u32 round_f32_to_u32(f32 float_32) {
    return (u32)(float_32 + 0.5f);
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

#endif
