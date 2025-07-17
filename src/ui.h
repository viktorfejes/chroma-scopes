#pragma once

#include "math.h"
#include <stdbool.h>
#include <stdint.h>

struct texture;

typedef enum ui_element_type {
    UI_ELEMENT_TYPE_INVALID = 0,
    UI_ELEMENT_TYPE_BLOCK,
    UI_ELEMENT_TYPE_ALIGNED,
    UI_ELEMENT_TYPE_FLEX_ROW,
    UI_ELEMENT_TYPE_FLEX_COL,
} ui_element_type_t;

typedef enum ui_unit {
    UI_UNIT_PIXEL = 0,
    UI_UNIT_PERCENT,
    UI_UNIT_FLEX,
} ui_unit_t;

typedef struct ui_value {
    int32_t value;
    ui_unit_t unit;
} ui_value_t;

typedef struct ui_spacing {
    ui_value_t top;
    ui_value_t right;
    ui_value_t bottom;
    ui_value_t left;
} ui_spacing_t;

typedef struct ui_gap {
    ui_value_t x;
    ui_value_t y;
} ui_gap_t;

typedef struct ui_constraints {
    uint16_t min_width;
    uint16_t min_height;
    uint16_t max_width;
    uint16_t max_height;
} ui_constraints_t;

typedef struct ui_element {
    uint16_t id;

    /* Relationships */
    int16_t parent_id;
    int16_t first_child_id;
    int16_t next_sibling_id;

    /* Type of the UI element. Equivalent to CSS `display` */
    ui_element_type_t type;

    /* Sizing and spacing */
    ui_value_t desired_width;
    ui_value_t desired_height;
    ui_spacing_t margin;
    ui_spacing_t padding;
    ui_gap_t gap;

    /* Decorative styling (most likely temporary for now) */
    float4_t background_color;
    struct texture *background_image;

    /* Stores the computed position and size of the element. */
    rect_t computed_rect;
} ui_element_t;

#define UI_VALUE(v, u) \
    (ui_value_t) { (v), (u) }

#define UI_MAX_ELEMENTS 128
#define UI_ROOT_ID 0

typedef struct ui_draw_command {
    float2_t position;
    float2_t size;
    float4_t background_color;
    struct texture *background_image;
} ui_draw_command_t;

typedef struct ui_draw_list {
    ui_draw_command_t commands[UI_MAX_ELEMENTS];
    uint32_t count;
} ui_draw_list_t;

typedef struct ui_state {
    ui_element_t elements[UI_MAX_ELEMENTS];
    ui_draw_list_t draw_list;
    struct texture *default_background_texture;
} ui_state_t;

bool ui_initialize(ui_state_t *state, uint16_t root_width, uint16_t root_height);
ui_element_t ui_create_element(void);
uint16_t ui_insert_element(ui_state_t *state, ui_element_t *element, uint16_t parent_id);
void ui_remove_element(ui_state_t *state, uint16_t id);
void ui_layout(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints, float2_t cursor);
void ui_draw(ui_state_t *state, ui_element_t *root);
