#pragma once

#include "math.h"
#include <stdbool.h>
#include <stdint.h>

struct texture;
struct renderer;
struct ui_element;

typedef bool (*mouse_event_fn)(struct ui_element *el);
typedef void (*hover_event_fn)(struct ui_element *el, bool is_hovered);

typedef enum ui_element_type {
    UI_ELEMENT_TYPE_BLOCK = 0,
    UI_ELEMENT_TYPE_FLEX,
    UI_ELEMENT_TYPE_ALIGNED,
} ui_element_type_t;

typedef enum ui_unit {
    UI_UNIT_AUTO = 0,
    UI_UNIT_PIXEL,
    UI_UNIT_PERCENT,
} ui_unit_t;

typedef enum ui_flex_direction {
    UI_FLEX_DIRECTION_ROW = 0,
    UI_FLEX_DIRECTION_COL
} ui_flex_direction_t;

typedef enum ui_flex_align {
    UI_FLEX_ALIGN_START = 0,
    UI_FLEX_ALIGN_END,
    UI_FLEX_ALIGN_STRETCH,
    UI_FLEX_ALIGN_CENTER,
    UI_FLEX_ALIGN_SPACE_BETWEEN,
    UI_FLEX_ALIGN_SPACE_EVENLY,
    UI_FLEX_ALIGN_SPACE_AROUND
} ui_flex_align_t;

typedef struct ui_value {
    float value;
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

typedef struct ui_styling {
    float4_t background_color;
    struct texture *background_image;
} ui_styling_t;

typedef struct ui_computed {
    rect_t layout;
    rect_t content;
} ui_computed_t;

typedef struct ui_element {
    uint16_t id;

    /* Relationships */
    int16_t parent_id;
    int16_t first_child_id;
    int16_t last_child_id;
    int16_t next_sibling_id;
    int16_t prev_sibling_id;

    /* Function pointer for mouse events */
    mouse_event_fn handle_mouse;
    hover_event_fn handle_hover_change;

    /* Type of the UI element. Equivalent to CSS `display` */
    ui_element_type_t type;

    /* Attributes when type is flex */
    ui_flex_direction_t flex_direction;
    ui_flex_align_t flex_main_axis_alignment;
    ui_flex_align_t flex_cross_axis_alignment;
    uint8_t flex_grow;
    uint8_t flex_shrink;
    ui_gap_t gap;

    /* Sizing and spacing */
    ui_value_t width;
    ui_value_t height;
    ui_spacing_t margin;
    ui_spacing_t padding;

    /* Decorative styling */
    ui_styling_t base_style;
    ui_styling_t hover_style;

    /* Stores the computed position and size of the element. */
    ui_computed_t computed;
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
    /* All elements in an array forming a tree doubly linked list */
    ui_element_t elements[UI_MAX_ELEMENTS];

    /* Hover state tracking */
    int16_t curr_hovered_element_id;
    int16_t prev_hovered_element_id;

    /* 1px background so something can always be bound avoiding branchin */
    struct texture *default_background_texture;
} ui_state_t;

bool ui_initialize(ui_state_t *state, uint16_t root_width, uint16_t root_height);
ui_element_t ui_create_element(void);
uint16_t ui_insert_element(ui_state_t *state, ui_element_t *element, uint16_t parent_id);
void ui_remove_element(ui_state_t *state, uint16_t id);
void ui_layout_measure(ui_state_t *state, ui_element_t *element, uint16_t min_width, uint16_t max_width, uint16_t min_height, uint16_t max_height);
void ui_layout_position(ui_state_t *state, ui_element_t *element, float origin_x, float origin_y);
void ui_draw(ui_state_t *state, struct renderer *renderer, ui_element_t *root, bool debug_view);
void ui_handle_mouse(ui_state_t *state);
ui_element_t *ui_get_hovered(ui_state_t *state);
