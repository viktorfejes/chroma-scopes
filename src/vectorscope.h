#pragma once

#include "texture.h"

#include <stdbool.h>

struct renderer;

typedef struct vectorscope {
    texture_t accum_tex;
    texture_t blur_tex;
    texture_t composite_tex;

    ID3D11Buffer *cbuffer;
} vectorscope_t;

bool vectorscope_setup(vectorscope_t *vs, struct renderer *renderer);
void vectorscope_render(vectorscope_t *vs, struct renderer *renderer, texture_t *capture_texture);
texture_t *vectorscope_get_texture(vectorscope_t *vs);
