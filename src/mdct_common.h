#pragma once


extern "C" {
#include <obs-module.h>
#include <util/circlebuf.h>
#include <graphics/image-file.h>
}

uint8_t inline sign12(float f) {
    return f < 0.0f ? 1 : 2;
}

float inline translate_int_float(int value, int imin, int imax, float fmin, float fmax) {
    float scale = (fmax - fmin) / (float)(imax - imin);
    float result = scale * (float)(value - imin) + fmin;
    return result;
}

float inline f_abs(float v) {
    if (v >= 0.0f)
        return v;
    return -v;
}

float inline f_min(float a, float b) {
    if (a > b)
        return b;
    return a;
}

float inline f_max(float a, float b) {
    if (a > b)
        return a;
    return b;
}

typedef struct {
    uint32_t x1, x2, y1, y2;
} ZoneCrop;

typedef struct {
    vec2 multiply;
    vec2 offset;
} TexturePosition;

typedef struct {
    float cx, cy, scale;
} TextureCenterPosition;

typedef struct {
    TexturePosition tp;
    bool updated;
} TP_Slot;

typedef TexturePosition texture_position_t;
typedef TextureCenterPosition texture_center_t;
typedef TP_Slot tp_slot_t;

//static texture_position_t tp_slots[10];
extern tp_slot_t tp_slots[10];

void inline tc_scale(texture_center_t *tc, float scale, texture_center_t *result) {
    result->cx = scale * tc->cx;
    result->cy = scale * tc->cy;
    result->scale = scale * tc->scale;
}

void inline tp2tc_limit(texture_position_t *tp, texture_center_t *tc, float scale_max) {
    float scale_max_inv = 1.0f / scale_max;
    float scale = f_min(1.0f, f_max(scale_max_inv, f_max(tp->multiply.x, tp->multiply.y)));
    float cx = tp->offset.x + 0.5f * tp->multiply.x;
    float cy = tp->offset.y + 0.5f * tp->multiply.y;
    float half = 0.5f * scale;
    if (cx < half) {
        cx = half;
    } else if (1.0f - cx < half) {
        cx = 1.0f - half;
    }
    if (cy < half) {
        cy = half;
    } else if (1.0f - cy < half) {
        cy = 1.0f - half;
    }
    tc->cx = cx;
    tc->cy = cy;
    tc->scale = scale;
}

void inline tp2tc(texture_position_t *tp, texture_center_t *tc) {
    tc->cx = tp->offset.x + 0.5f * tp->multiply.x;
    tc->cy = tp->offset.y + 0.5f * tp->multiply.y;
    tc->scale = f_max(tp->multiply.x, tp->multiply.y);
}

void inline tc2tp(texture_center_t *tc, texture_position_t *tp) {
    tp->multiply.x = tc->scale;
    tp->multiply.y = tc->scale;
    tp->offset.x = tc->cx - 0.5f * tc->scale;
    tp->offset.y = tc->cy - 0.5f * tc->scale;
}

void inline tc_limit(texture_center_t *tc, float scale_max_inv) {
    float scale = f_min(1.0f, f_max(scale_max_inv, tc->scale));
    float cx = tc->cx;
    float cy = tc->cy;
    float half = 0.5f * scale;
    if (cx < half) {
        cx = half;
    } else if (1.0f - cx < half) {
        cx = 1.0f - half;
    }
    if (cy < half) {
        cy = half;
    } else if (1.0f - cy < half) {
        cy = 1.0f - half;
    }
    tc->cx = cx;
    tc->cy = cy;
    tc->scale = scale;
}


void inline tc_average(texture_center_t *a, texture_center_t *b, float factor_a, texture_center_t *result) {
    float fb = 1.0f - factor_a;
    result->cx = fb * b->cx + factor_a * a->cx;
    result->cy = fb * b->cy + factor_a * a->cy;
    result->scale = fb * b->scale + factor_a * a->scale;
}

void inline tc_brake(texture_center_t *velocity, texture_center_t *acceleration, float brake) {
    if (!(sign12(velocity->cx) & sign12(acceleration->cx))) {
        velocity->cx *= brake;
    }
    if (!(sign12(velocity->cy) & sign12(acceleration->cy))) {
        velocity->cy *= brake;
    }
    if (!(sign12(velocity->scale) & sign12(acceleration->scale))) {
        velocity->scale *= brake;
    }
}

void inline tc_add(texture_center_t *a, texture_center_t *b, texture_center_t *result) {
    result->cx = a->cx + b->cx;
    result->cy = a->cy + b->cy;
    result->scale = a->scale + b->scale;
}

void inline tc_sub(texture_center_t *a, texture_center_t *b, texture_center_t *result) {
    result->cx = a->cx - b->cx;
    result->cy = a->cy - b->cy;
    result->scale = a->scale - b->scale;
}

void inline tp_reset(texture_position_t *texp) {
    texp->multiply.x = 1.0f;
    texp->multiply.y = 1.0f;
    texp->offset.x = 0.0f;
    texp->offset.y = 0.0f;
}

void inline tc_reset(texture_center_t *tc) {
    tc->scale = 1.0f;
    tc->cx = 0.5f;
    tc->cy = 0.5f;
}

void inline tc_reset0(texture_center_t *tc) {
    tc->scale = 0.0f;
    tc->cx = 0.0f;
    tc->cy = 0.0f;
}

void inline strict_texp(texture_position_t *tp) {
    if (tp->offset.x < 0.0f) {
        tp->offset.x = 0.0f;
    } else {
        float mdiff = tp->offset.x + tp->multiply.x - 1.0f;
        if (mdiff > 0.0f) {
            tp->offset.x -= mdiff;
        }
    }
    if (tp->offset.y < 0.0f) {
        tp->offset.y = 0.0f;
    } else {
        float mdiff = tp->offset.y + tp->multiply.y - 1.0f;
        if (mdiff > 0.0f) {
            tp->offset.y -= mdiff;
        }
    }
}

void inline conform_texp(texture_position_t *tp, float mult_min) {
    if (tp->multiply.x < mult_min) {
        tp->offset.x -= 0.5f * (mult_min - tp->multiply.x);
        tp->multiply.x = mult_min;
    } else if (tp->multiply.x > 1.0f) {
        tp->offset.x = 0.0f;
        tp->multiply.x = 1.0f;
    }
    if (tp->multiply.y < mult_min) {
        tp->offset.y -= 0.5f * (mult_min - tp->multiply.y);
        tp->multiply.y = mult_min;
    } else if (tp->multiply.y > 1.0f) {
        tp->offset.y = 0.0f;
        tp->multiply.y = 1.0f;
    }
    float mdiff = 0.5f * f_abs(tp->multiply.x - tp->multiply.y);
    if (mdiff > 0.0001f) {
        if (tp->multiply.x < tp->multiply.y) {
            tp->offset.x -= mdiff;
            tp->multiply.x = tp->multiply.y;
        } else {
            tp->offset.y -= mdiff;
            tp->multiply.y = tp->multiply.x;
        }
    }
}

void inline conform_bound(float &v, float g) {
    if (v < g) {
        v = g;
    } else if (v > (1.0f - g)) {
        v = 1.0f - g;
    }
}

void inline conform_tc(texture_center_t *tc, float mult_min) {
    tc->scale = f_min(1.0f, f_max(mult_min, tc->scale));
    float gap = 0.5f * tc->scale;
    conform_bound(tc->cx, gap);
    conform_bound(tc->cy, gap);
}

