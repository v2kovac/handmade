#pragma once

union V2 {
    struct {
        f32 x, y;
    };
    f32 E[2];
};

internal V2 v2(f32 x, f32 y) {
    V2 result;
    result.x = x;
    result.y = y;
    return result;
}

internal V2 operator+(V2 a, V2 b) {
    V2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

internal V2 &operator+=(V2 &a, V2 b) {
    a = a + b;
    return a;
}

internal V2 operator-(V2 a, V2 b) {
    V2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

internal V2 operator-(V2 a) {
    V2 result;
    result.x = -a.x;
    result.y = -a.y;
    return result;
}

internal V2 operator*(f32 a, V2 b) {
    V2 result;
    result.x = a * b.x;
    result.y = a * b.y;
    return result;
}

internal V2 operator*(V2 a, f32 b) {
    V2 result = b * a;
    return result;
}

internal V2 &operator*=(V2 &a, f32 b) {
    a = a * b;
    return a;
}

internal f32 square(f32 a) {
    f32 result = a * a;
    return result;
}

internal f32 inner(V2 a, V2 b) {
    f32 result = (a.x * b.x) + (a.y * b.y);
    return result;
}

internal f32 length_sq(V2 a) {
    f32 result = inner(a, a);
    return result;
}

struct Rect2 {
    V2 min;
    V2 max;
};

internal V2 get_min_corner(Rect2 rect) {
    return rect.min;
}

internal V2 get_max_corner(Rect2 rect) {
    return rect.max;
}

internal Rect2 rect_center_half_dim(V2 center, V2 half_dim) {
    return Rect2{center - half_dim, center + half_dim};
}

internal Rect2 rect_center_dim(V2 center, V2 dim) {
    return rect_center_half_dim(center, 0.5f * dim);
}

internal bool is_in_rect(Rect2 rect, V2 test) {
    return test.x >= rect.min.x &&
           test.y >= rect.min.y &&
           test.x < rect.max.x &&
           test.y < rect.max.y;
}
