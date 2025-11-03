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

internal inline v2 &operator+=(v2 &a, v2 b) {
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

internal inline f32 square(f32 a) {
    f32 result = a * a;
    return result;
}

internal inline f32 inner(v2 a, v2 b) {
    f32 result = (a.x * b.x) + (a.y * b.y);
    return result;
}

internal inline f32 length_sq(v2 a) {
    f32 result = inner(a, a);
    return result;
}

struct Rect2 {
    v2 min;
    v2 max;
};

internal inline Rect2 rect_center_half_dim(v2 center, v2 half_dim) {
    return Rect2{center - half_dim, center + half_dim};
}

internal inline Rect2 rect_center_dim(v2 center, v2 dim) {
    return rect_center_half_dim(center, 0.5f * dim);
}

internal inline bool is_in_rect(Rect2 rect, v2 test) {
    return test.x >= rect.min.x &&
           test.y >= rect.min.y &&
           test.x < rect.max.x &&
           test.y < rect.max.y;
}
