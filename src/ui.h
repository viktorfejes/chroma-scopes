#pragma once

#include <stdbool.h>
#include <stdint.h>

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

typedef struct ui_rect {
    int32_t x, y;
    uint32_t width, height;
} ui_rect_t;

typedef struct ui_point {
    int32_t x, y;
} ui_point_t;

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
    uint32_t background_color;

    /* Stores the computed position and size of the element. */
    ui_rect_t computed_rect;
} ui_element_t;

#define UI_MAX_ELEMENTS 128

typedef struct ui_state {
    ui_element_t elements[UI_MAX_ELEMENTS];
} ui_state_t;

bool ui_initialize(ui_state_t *state);
ui_element_t ui_create_element(void);
uint16_t ui_insert_element(ui_state_t *state, ui_element_t *element, uint16_t parent_id);
void ui_remove_element(ui_state_t *state, uint16_t id);
void ui_layout(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints, ui_point_t cursor);
void ui_draw(ui_state_t *state, ui_element_t *root);
