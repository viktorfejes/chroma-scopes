#include "ui.h"

#include "logger.h"
#include "macros.h"

#include <assert.h>
#include <string.h>

static void draw_element(ui_state_t *state, ui_element_t *root);
static void layout_block_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints);
static void layout_aligned_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints);
static void layout_flex_row_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints);
static void layout_flex_col_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints);
static int32_t resolve_unit(ui_value_t value, int32_t max);

bool ui_initialize(ui_state_t *state, uint16_t root_width, uint16_t root_height) {
    assert(state && "UI state must be a valid pointer");

    // For now just invalidate all elements by setting their id's to invalid
    memset(state->elements, 0, sizeof(ui_element_t) * UI_MAX_ELEMENTS);
    for (int i = 0; i < UI_MAX_ELEMENTS; ++i) {
        ui_element_t *el = &state->elements[i];
        el->id = ((uint16_t)-1);
        el->parent_id = -1;
        el->first_child_id = -1;
        el->next_sibling_id = -1;
    }

    // Make sure draw list count is 0
    state->draw_list.count = 0;

    // Add ROOT by default (the size of window)
    ui_element_t *root = &state->elements[UI_ROOT_ID];
    root->id = UI_ROOT_ID;
    root->type = UI_ELEMENT_TYPE_BLOCK;
    root->desired_width = UI_VALUE(root_width, UI_UNIT_PIXEL);
    root->desired_height = UI_VALUE(root_height, UI_UNIT_PIXEL);
    root->computed_rect.width = root_width;
    root->computed_rect.height = root_height;

    return true;
}

ui_element_t ui_create_element(void) {
    ui_element_t default_element = {
        .id = ((uint16_t)-1),
        .parent_id = -1,
        .first_child_id = -1,
        .next_sibling_id = -1,
        .type = UI_ELEMENT_TYPE_BLOCK,
        .background_color = (float4_t){1.0f, 1.0f, 1.0f, 1.0f},
    };

    return default_element;
}

uint16_t ui_insert_element(ui_state_t *state, ui_element_t *element, uint16_t parent_id) {
    assert(state && "UI state must be a valid pointer");
    assert(element && "UI element must be a valid pointer");

    // Find new spot with linear search
    ui_element_t *el = NULL;
    for (int i = 0; i < UI_MAX_ELEMENTS; ++i) {
        if (state->elements[i].id == ((uint16_t)-1)) {
            // Found an available slot
            el = &state->elements[i];
            el->id = i;
            break;
        }
    }

    if (!el) {
        LOG("Couldn't find a spot for a new UI element. Maybe there is no more space?");
        el->id = ((uint16_t)-1);
        return ((uint16_t)-1);
    }

    // Copy the element into the slot
    // FIXME: This save-delete-replace is a bit silly for the id
    uint16_t id = el->id;
    *el = *element;
    el->id = id;
    el->parent_id = parent_id;

    // Set up appropriate relationship
    ui_element_t *parent = &state->elements[parent_id];
    if (!parent || parent->id == ((uint16_t)-1)) {
        LOG("Invalid parent id!");
        el->id = ((uint16_t)-1);
        return ((uint16_t)-1);
    }

    // Link with children/siblings
    el->next_sibling_id = parent->first_child_id;
    parent->first_child_id = el->id;

    return el->id;
}

void ui_remove_element(ui_state_t *state, uint16_t id) {
    assert(state && "UI state must be a valid pointer");
    assert(id < UI_MAX_ELEMENTS && "ID must be valid for removal");

    ui_element_t *el = &state->elements[id];
    if (el && el->id != ((uint16_t)-1) && el->parent_id <= 0) {
        // Remove from relationship
        ui_element_t *parent = &state->elements[el->parent_id];
        if (!parent) {
            LOG("Couldn't find parent for element to be removed");
            return;
        }

        // If the parent's first child is this element just remove it from there and replace with sibling
        if (parent->first_child_id == el->id) {
            parent->first_child_id = el->next_sibling_id;
        } else {
            // Otherwise, we need to walk and find
            ui_element_t *current_sibling = &state->elements[parent->first_child_id];
            ui_element_t *prev_sibling = current_sibling;
            while (true) {
                if (current_sibling == el) {
                    prev_sibling->next_sibling_id = el->next_sibling_id;
                    break;
                }

                prev_sibling = current_sibling;
                current_sibling = &state->elements[current_sibling->next_sibling_id];
            }
        }

        // Invalidate slot's id
        el->id = ((uint16_t)-1);
        LOG("Element removed from the layout tree");
    }
}

void ui_layout(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints, float2_t cursor) {
    assert(state && "UI state must be a valid pointer");
    assert(element && "UI element must be a valid pointer");

    // Get the desired dimensions of the element
    int32_t dw = resolve_unit(element->desired_width, constraints.max_width);
    int32_t dh = resolve_unit(element->desired_height, constraints.max_height);

    // Apply constraints to desired dimensions
    int32_t w = CLAMP(dw, constraints.min_width, constraints.max_width);
    int32_t h = CLAMP(dh, constraints.min_height, constraints.max_height);

    // Resolve padding
    int32_t pt = resolve_unit(element->padding.top, h);
    int32_t pr = resolve_unit(element->padding.right, w);
    int32_t pb = resolve_unit(element->padding.bottom, h);
    int32_t pl = resolve_unit(element->padding.left, w);

    // Resolve margin
    int32_t mt = resolve_unit(element->margin.top, h);
    int32_t ml = resolve_unit(element->margin.left, w);

    // Compute absolute position
    int32_t x = cursor.x + ml;
    int32_t y = cursor.y + mt;

    // Save computed size of element
    element->computed_rect.x = x;
    element->computed_rect.y = y;
    element->computed_rect.width = w;
    element->computed_rect.height = h;

    // Calculate the constraints to pass on
    ui_constraints_t inner_constraints = {
        .min_width = 0,
        .min_height = 0,
        .max_width = w - (pl + pr),
        .max_height = h - (pt + pb)};

    switch (element->type) {
        case UI_ELEMENT_TYPE_BLOCK:
            layout_block_children(state, element, inner_constraints);
            break;
        case UI_ELEMENT_TYPE_ALIGNED:
            layout_aligned_children(state, element, inner_constraints);
            break;
        case UI_ELEMENT_TYPE_FLEX_ROW:
            layout_flex_row_children(state, element, inner_constraints);
            break;
        case UI_ELEMENT_TYPE_FLEX_COL:
            layout_flex_col_children(state, element, inner_constraints);
            break;

        default: {
            LOG("Unknown UI element type");
            return;
        }
    }
}

void ui_draw(ui_state_t *state, ui_element_t *root) {
    assert(state && "UI state must be a valid pointer");

    draw_element(state, root);

    int32_t child_idx = root->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];
        ui_draw(state, child);
        child_idx = child->next_sibling_id;
    }
}

static void draw_element(ui_state_t *state, ui_element_t *root) {
    ui_draw_command_t *command = &state->draw_list.commands[state->draw_list.count];

    command->position = rect_to_position(root->computed_rect);
    command->size = rect_to_size(root->computed_rect);
    command->background_color = root->background_color;
    command->background_image = root->background_image;

    state->draw_list.count++;
}

static void layout_block_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints) {
    int32_t parent_padding_top = resolve_unit(element->padding.top, element->computed_rect.height);
    int32_t parent_padding_left = resolve_unit(element->padding.left, element->computed_rect.width);

    // Initialize local cursor using parent's padding
    float2_t cursor = {
        .x = element->computed_rect.x + parent_padding_left,
        .y = element->computed_rect.y + parent_padding_top,
    };

    // Traverse children
    int32_t child_idx = element->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];

        // Measure size of child
        ui_layout(state, child, constraints, cursor);

        // Advance local cursor by the computed height of the element and the top and bottom margins
        // FIXME: I suspect a bug here with the margins...
        cursor.x += child->computed_rect.height +
                    resolve_unit(child->margin.bottom, child->computed_rect.height) +
                    resolve_unit(child->margin.top, child->computed_rect.height);

        child_idx = child->next_sibling_id;
    }
}

static void layout_aligned_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints) {
    int32_t parent_padding_top = resolve_unit(element->padding.top, element->computed_rect.height);
    int32_t parent_padding_left = resolve_unit(element->padding.left, element->computed_rect.width);

    // ALIGNED elements can only have a SINGLE child
    int32_t child_idx = element->first_child_id;
    if (child_idx == -1) return;

    ui_element_t *child = &state->elements[child_idx];

    // NOTE: Passing in dummy cursor as we will overwrite it later
    ui_layout(state, child, constraints, (float2_t){0, 0});

    // Resolve margins
    int32_t mt = resolve_unit(element->margin.top, child->computed_rect.height);
    int32_t mr = resolve_unit(element->margin.right, child->computed_rect.width);
    int32_t mb = resolve_unit(element->margin.bottom, child->computed_rect.height);
    int32_t ml = resolve_unit(element->margin.left, child->computed_rect.width);

    // TODO: Expand this to other alignments, for now it's CENTER only
    uint16_t aw = constraints.max_width - (ml + mr);
    uint16_t ah = constraints.max_height - (mt + mb);

    // Recompute position
    child->computed_rect.x += parent_padding_left + ml + (aw - child->computed_rect.width) / 2;
    child->computed_rect.y += parent_padding_top + mt + (ah - child->computed_rect.height) / 2;
}

static void layout_flex_row_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints) {
    int32_t parent_padding_top = resolve_unit(element->padding.top, element->computed_rect.height);
    int32_t parent_padding_left = resolve_unit(element->padding.left, element->computed_rect.width);

    float2_t gap = {
        .x = resolve_unit(element->gap.x, element->computed_rect.width),
        .y = resolve_unit(element->gap.y, element->computed_rect.height),
    };

    // Initialize local cursor using parent's padding
    float2_t cursor = {
        .x = element->computed_rect.x + parent_padding_left,
        .y = element->computed_rect.y + parent_padding_top,
    };

    // 1st Pass: Gather children and do some calculations
    struct child_info {
        uint16_t idx;
        uint16_t dw, dh;
    };

    struct child_info children_info[16];
    int children_info_count = 0;
    int32_t child_idx = element->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];

        children_info[children_info_count].idx = child_idx;
        children_info[children_info_count].dw = resolve_unit(child->desired_width, constraints.max_width);
        children_info[children_info_count].dh = resolve_unit(child->desired_height, constraints.max_height);
        children_info_count++;

        child_idx = child->next_sibling_id;
    }

    // 2nd Pass: Distribute sizes and positions
    int32_t available_width = constraints.max_width - (gap.x * (children_info_count - 1));
    int32_t parent_center_y = constraints.max_height / 2;

    for (int i = 0; i < children_info_count; ++i) {
        ui_element_t *child = &state->elements[children_info[i].idx];

        ui_constraints_t c = {
            .min_width = available_width / children_info_count,
            .min_height = 0,
            .max_width = available_width / children_info_count,
            .max_height = constraints.max_height,
        };

        // Measure the size
        ui_layout(state, child, c, (float2_t){0, 0});

        // Correct the layout if needed based on the alignment
        child->computed_rect.y += parent_center_y - child->computed_rect.height / 2;

        // We call layout again to recalculate with new position
        ui_layout(state, child, c, (float2_t){cursor.x, child->computed_rect.y});

        cursor.x += child->computed_rect.width + gap.x;
    }
}

static void layout_flex_col_children(ui_state_t *state, ui_element_t *element, ui_constraints_t constraints) {
    UNUSED(state);
    UNUSED(element);
    UNUSED(constraints);
}

static int32_t resolve_unit(ui_value_t value, int32_t max) {
    switch (value.unit) {
        case UI_UNIT_PIXEL:
            return value.value;
        case UI_UNIT_PERCENT:
            return (value.value * 0.01) * max;
        case UI_UNIT_FLEX:
            return value.value;
    }
}
