#pragma once

#include "texture.h"

#include <stdbool.h>

struct renderer;

typedef struct waveform {
    ID3D11Buffer *accum_buffer;
    ID3D11UnorderedAccessView *accum_uav;
    ID3D11ShaderResourceView *accum_srv;

    texture_t blur_tex;
    texture_t composite_tex;

    ID3D11Buffer *cbuffer;
} waveform_t;

bool waveform_setup(waveform_t *wf, struct renderer *renderer);
void waveform_render(waveform_t *wf, struct renderer *renderer, texture_t *capture_texture);
texture_t *waveform_get_texture(waveform_t *wf);
