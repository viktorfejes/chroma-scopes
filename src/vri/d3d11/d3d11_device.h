#pragma once

#include "../vri.h"

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct IDXGIAdapter;

typedef struct {
    vri_device_t base;

    struct ID3D11Device5 *device;
    struct ID3D11DeviceContext4 *immediate_context;
    struct IDXGIAdapter *adapter;
} vri_d3d11_device_t;


