#pragma once

union v2 {
    struct {
        f32 x, y;
    };
    f32 E[2];
};

internal inline v2 operator+(v2 a, v2 b) {
    v2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline v2 &operator+=(v2 &a, v2 b) {
    a = a + b;
    return a;
}


internal inline v2 operator-(v2 a, v2 b) {
    v2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

internal inline v2 operator-(v2 a) {
    v2 result;
    result.x = -a.x;
    result.y = -a.y;
    return result;
}

internal inline v2 operator*(f32 a, v2 b) {
    v2 result;
    result.x = a * b.x;
    result.y = a * b.y;
    return result;
}

internal inline v2 operator*(v2 a, f32 b) {
    v2 result = b * a;
    return result;
}

internal inline v2 &operator*=(v2 &a, f32 b) {
    a = a * b;
    return a;
}
