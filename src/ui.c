#include "ui.h"

#include "input.h"
#include "logger.h"
#include "macros.h"
#include "renderer.h"

#include <assert.h>
#include <string.h>

struct per_ui_mesh_data {
    float2_t position;
    float2_t size;
    float4_t color;
};

static void layout_block_children(ui_state_t *state, ui_element_t *element, float content_width, float content_height);
static void layout_flex_children(ui_state_t *state, ui_element_t *element, float content_width, float content_height);
static void position_block_children(ui_state_t *state, ui_element_t *element);
static void position_flex_children(ui_state_t *state, ui_element_t *element);
static float parse_value(ui_value_t value, float max);
static float parse_spacing_axis(ui_spacing_t spacing, float comparison_value, bool horizontal);
static float parse_spacing(ui_value_t spacing, float comparison_size);

#define UI_HORIZONTAL true
#define UI_VERTICAL false

#define UI_FOREACH_CHILD(NODES, PARENT_IDX, CHILD_VAR) \
    for (int CHILD_VAR = (NODES)[(PARENT_IDX)].first_child; CHILD_VAR != -1; CHILD_VAR = (NODES)[CHILD_VAR].next_sibling)

bool ui_initialize(ui_state_t *state, uint16_t root_width, uint16_t root_height) {
    assert(state && "UI state must be a valid pointer");

    // For now just invalidate all elements by setting their id's to invalid
    memset(state->elements, 0, sizeof(ui_element_t) * UI_MAX_ELEMENTS);
    for (int i = 0; i < UI_MAX_ELEMENTS; ++i) {
        ui_element_t *el = &state->elements[i];
        el->id = ((uint16_t)-1);
        el->parent_id = -1;
        el->first_child_id = -1;
        el->last_child_id = -1;
        el->next_sibling_id = -1;
        el->prev_sibling_id = -1;
    }

    // Add ROOT by default (the size of window)
    ui_element_t *root = &state->elements[UI_ROOT_ID];
    root->id = UI_ROOT_ID;
    root->width = UI_VALUE(root_width, UI_UNIT_PIXEL);
    root->height = UI_VALUE(root_height, UI_UNIT_PIXEL);
    root->computed.layout.width = root_width;
    root->computed.layout.height = root_height;
    root->computed.content.width = root_width;
    root->computed.content.height = root_height;

    return true;
}

ui_element_t ui_create_element(void) {
    ui_element_t default_element = {
        .id = ((uint16_t)-1),
        .parent_id = -1,
        .first_child_id = -1,
        .last_child_id = -1,
        .next_sibling_id = -1,
        .prev_sibling_id = -1,
        .base_style.background_color = (float4_t){1.0f, 1.0f, 1.0f, 1.0f},
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

    // Set up appropriate relationship
    el->parent_id = parent_id;
    el->next_sibling_id = -1;
    el->prev_sibling_id = -1;

    ui_element_t *parent = &state->elements[parent_id];
    if (!parent || parent->id == ((uint16_t)-1)) {
        LOG("Invalid parent id!");
        el->id = ((uint16_t)-1);
        return ((uint16_t)-1);
    }

    if (parent->first_child_id == -1) {
        // First and only child
        parent->first_child_id = el->id;
        parent->last_child_id = el->id;
    } else {
        // Append to end of sibling list
        ui_element_t *last = &state->elements[parent->last_child_id];
        last->next_sibling_id = el->id;
        el->prev_sibling_id = last->id;
        parent->last_child_id = el->id;
    }

    return el->id;
}

void ui_remove_element(ui_state_t *state, uint16_t id) {
    assert(state && "UI state must be a valid pointer");
    assert(id < UI_MAX_ELEMENTS && "ID must be valid for removal");

    ui_element_t *el = &state->elements[id];
    if (!el || el->id == ((uint16_t)-1)) {
        LOG("Element already invalid");
        return;
    }

    int16_t parent_id = el->parent_id;
    if (parent_id != -1) {
        ui_element_t *parent = &state->elements[parent_id];
        if (parent && parent->id != ((uint16_t)-1)) {
            // Unlink from siblings
            if (el->prev_sibling_id != -1) {
                state->elements[el->prev_sibling_id].next_sibling_id = el->next_sibling_id;
            } else {
                // el was first child
                parent->first_child_id = el->next_sibling_id;
            }

            if (el->next_sibling_id != -1) {
                state->elements[el->next_sibling_id].prev_sibling_id = el->prev_sibling_id;
            } else {
                // el was last child
                parent->last_child_id = el->prev_sibling_id;
            }
        } else {
            LOG("Couldn't find valid parent for element");
        }
    }

    // Recursively remove all children
    int16_t child = el->first_child_id;
    while (child != -1) {
        int16_t next = state->elements[child].next_sibling_id;
        ui_remove_element(state, child);
        child = next;
    }

    // Invalidate and clear the element
    *el = (ui_element_t){0};
    el->id = (uint16_t)-1;
    el->parent_id = -1;
    el->next_sibling_id = -1;
    el->prev_sibling_id = -1;

    LOG("Element removed from layout tree");
}

void ui_layout_measure(ui_state_t *state, ui_element_t *element, uint16_t min_width, uint16_t max_width, uint16_t min_height, uint16_t max_height) {
    assert(state && "UI state must be a valid pointer");
    assert(element && "UI element must be a valid pointer");

    float available_width = max_width - parse_spacing_axis(element->margin, max_width, UI_HORIZONTAL);
    float available_height = max_height - parse_spacing_axis(element->margin, max_height, UI_VERTICAL);

    float width = parse_value(element->width, available_width);
    float height = parse_value(element->height, available_height);

    float px = parse_spacing_axis(element->padding, max_width, UI_HORIZONTAL);
    float py = parse_spacing_axis(element->padding, max_height, UI_VERTICAL);

    if (element->first_child_id > -1) {
        float content_width = width - px;
        float content_height = height - py;

        switch (element->type) {
            case UI_ELEMENT_TYPE_FLEX:
                layout_flex_children(state, element, content_width, content_height);
                break;
            case UI_ELEMENT_TYPE_BLOCK:
                layout_block_children(state, element, content_width, content_height);
                break;
            case UI_ELEMENT_TYPE_ALIGNED:
                LOG("Aligned element type has not been implemented yet!");
                return;
            default:
                LOG("Unknown element type");
                return;
        }

        // Calculate intrinsic size
        if (element->width.unit == UI_UNIT_AUTO) {
            // TODO:
        }
        if (element->height.unit == UI_UNIT_AUTO) {
            // TODO:
        }
    }

    // Save out the computed box size -- this is the full size of the box
    element->computed.layout.width = CLAMP(width, min_width, available_width);
    element->computed.layout.height = CLAMP(height, min_height, available_height);

    // Save the computed content size -- this is the usable area for children
    element->computed.content.width = element->computed.layout.width - px;
    element->computed.content.height = element->computed.layout.height - py;
}

void ui_layout_position(ui_state_t *state, ui_element_t *element, float origin_x, float origin_y) {
    // Grab the parent for percentage calculations where it applies (margin, padding...etc)
    ui_element_t *parent = element->parent_id > -1 ? &state->elements[element->parent_id] : &state->elements[0];

    element->computed.layout.x = origin_x + parse_spacing(element->margin.left, parent->computed.layout.width);
    element->computed.layout.y = origin_y + parse_spacing(element->margin.top, parent->computed.layout.width);

    element->computed.content.x = element->computed.layout.x + parse_spacing(element->padding.left, parent->computed.layout.width);
    element->computed.content.y = element->computed.layout.y + parse_spacing(element->padding.top, parent->computed.layout.width);

    if (element->first_child_id > -1) {
        switch (element->type) {
            case UI_ELEMENT_TYPE_FLEX:
                position_flex_children(state, element);
                break;
            case UI_ELEMENT_TYPE_BLOCK:
                position_block_children(state, element);
                break;
            case UI_ELEMENT_TYPE_ALIGNED:
                LOG("Aligned element type has not been implemented yet!");
                return;
            default:
                LOG("Unknown element type");
                return;
        }
    }
}

void ui_draw(ui_state_t *state, struct renderer *renderer, ui_element_t *root) {
    assert(state && "UI state must be a valid pointer");

    ID3D11DeviceContext1 *context = renderer->context;

    // Bind the per object data
    D3D11_MAPPED_SUBRESOURCE mapped;
    context->lpVtbl->Map(context, (ID3D11Resource *)renderer->per_ui_mesh_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    struct per_ui_mesh_data *per_mesh_buffer = mapped.pData;
    per_mesh_buffer->position = rect_to_position(root->computed.layout);
    per_mesh_buffer->size = rect_to_size(root->computed.layout);
    per_mesh_buffer->color = root->base_style.background_color;

    context->lpVtbl->Unmap(context, (ID3D11Resource *)renderer->per_ui_mesh_buffer, 0);
    context->lpVtbl->VSSetConstantBuffers(context, 1, 1, &renderer->per_ui_mesh_buffer);
    context->lpVtbl->PSSetConstantBuffers(context, 1, 1, &renderer->per_ui_mesh_buffer);

    // Set texture if any is present
    if (root->base_style.background_image) {
        context->lpVtbl->PSSetShaderResources(context, 0, 1, &root->base_style.background_image->srv);
    } else {
        context->lpVtbl->PSSetShaderResources(context, 0, 1, &renderer->default_white_px.srv);
    }

    // Draw the UI mesh
    context->lpVtbl->Draw(context, 6, 0);

    // Repeat for the children recursively
    for (int16_t child = root->first_child_id; child != -1; child = state->elements[child].next_sibling_id) {
        ui_draw(state, renderer, &state->elements[child]);
    }
}

bool ui_handle_mouse_event(ui_state_t *state, ui_element_t *element) {
    // Check bounds
    int2_t mouse_pos = input_mouse_get_pos();
    if (!rect_contains(element->computed.layout, (float2_t){mouse_pos.x, mouse_pos.y})) {
        return false;
    }

    // Process children first (front-to-back)
    for (int16_t child = element->last_child_id; child != -1; child = state->elements[child].prev_sibling_id) {
        if (ui_handle_mouse_event(state, &state->elements[child])) {
            return true; // Child consumed it, we are done!
        }
    }

    // Save out topmost hover
    state->curr_hovered_element_id = element->id;

    // No child handled it, try current element
    if (element->handle_mouse) {
        return element->handle_mouse(element);
    }

    return false;
}

void ui_update_hover_states(ui_state_t *state) {
    // Clear prev hover
    if (state->prev_hovered_element_id != -1 && state->prev_hovered_element_id != state->curr_hovered_element_id) {
        ui_element_t *prev_elem = &state->elements[state->prev_hovered_element_id];
        if (prev_elem->handle_hover_change) {
            prev_elem->handle_hover_change(prev_elem, false);
        }
    }

    // Set new hover
    if (state->curr_hovered_element_id != -1) {
        ui_element_t *elem = &state->elements[state->curr_hovered_element_id];
        if (elem->handle_hover_change) {
            elem->handle_hover_change(elem, true);
        }
    }

    state->prev_hovered_element_id = state->curr_hovered_element_id;
}

static void layout_block_children(ui_state_t *state, ui_element_t *element, float content_width, float content_height) {
    int32_t child_idx = element->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];

        float child_max_w = parse_value(child->width, content_width);
        float child_max_h = parse_value(child->height, content_height);
        ui_layout_measure(state, child, 0, child_max_w, 0, child_max_h);

        child_idx = child->next_sibling_id;
    }
}

static void layout_flex_children(ui_state_t *state, ui_element_t *element, float content_width, float content_height) {
    // HACK: Using a simple fixed array for gather for now
    // Find better method -- maybe even a dynamic array (spk_array)
    ui_element_t *flex_children[16];
    uint8_t flex_children_count = 0;

    uint16_t total_flex_amount = 0;
    float total_fixed_size = 0.0f;
    uint16_t total_children_count = 0;

    // First pass: measure and gather children
    int32_t child_idx = element->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];

        // Count the child into total
        total_children_count++;

        if (child->flex_grow > 0) {
            total_flex_amount += child->flex_grow;
            flex_children[flex_children_count++] = child;
        } else {
            if (element->flex_direction == UI_FLEX_DIRECTION_ROW) {
                float child_max_w = parse_value(child->width, content_width);
                float child_min_h = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_height : 0;
                float child_max_h = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_height : parse_value(child->height, content_height);
                ui_layout_measure(state, child, 0, child_max_w, child_min_h, child_max_h);
                total_fixed_size += child->computed.layout.width + parse_spacing_axis(child->margin, content_width, UI_HORIZONTAL);
            } else {
                float child_min_w = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_width : 0;
                float child_max_w = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_width : parse_value(child->width, content_width);
                float child_max_h = parse_value(child->height, content_height);
                ui_layout_measure(state, child, child_min_w, child_max_w, 0, child_max_h);
                total_fixed_size += child->computed.layout.height + parse_spacing_axis(child->margin, content_height, UI_VERTICAL);
            }
        }
        child_idx = child->next_sibling_id;
    }

    // NOTE: Gap size when defined in percentage should be based on the width and height of the element it is
    // set on, which our case is the `element`, meaning we can use its computed with and height to parsing the value
    float total_gap_x = MAX(0, (total_children_count - 1) * parse_spacing(element->gap.x, element->computed.layout.width));
    float total_gap_y = MAX(0, (total_children_count - 1) * parse_spacing(element->gap.y, element->computed.layout.height));

    // Calculate the remaining space -- how much we still have where we can distribute the flexible children
    float main_axis_content_size = element->flex_direction == UI_FLEX_DIRECTION_ROW ? content_width : content_height;
    float remaining_space_x = MAX(0, main_axis_content_size - total_fixed_size - total_gap_x);
    float remaining_space_y = MAX(0, main_axis_content_size - total_fixed_size - total_gap_y);

    // Second pass: distribute flexible children
    for (uint16_t i = 0; i < flex_children_count; ++i) {
        ui_element_t *child = flex_children[i];

        if (element->flex_direction == UI_FLEX_DIRECTION_ROW) {
            float child_width = remaining_space_x * (child->flex_grow / (float)total_flex_amount);
            float child_min_h = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_height : 0;
            float child_max_h = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_height : parse_value(child->height, content_height);
            ui_layout_measure(state, child, 0, child_width, child_min_h, child_max_h);
        } else {
            float child_max_w = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_width : parse_value(child->width, content_width);
            float child_min_w = element->flex_cross_axis_alignment == UI_FLEX_ALIGN_STRETCH ? content_width : 0;
            float child_height = remaining_space_y * (child->flex_grow / (float)total_flex_amount);
            ui_layout_measure(state, child, child_min_w, child_max_w, 0, child_height);
        }
    }
}

static void position_block_children(ui_state_t *state, ui_element_t *element) {
    float cursor_y = element->computed.content.y;

    int32_t child_idx = element->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];

        ui_layout_position(state, child, element->computed.content.x, cursor_y);
        cursor_y += child->computed.layout.height + parse_spacing_axis(child->margin, element->computed.content.width, UI_VERTICAL);

        child_idx = child->next_sibling_id;
    }
}

static void position_flex_children(ui_state_t *state, ui_element_t *element) {
    bool is_row = element->flex_direction == UI_FLEX_DIRECTION_ROW;

    ui_element_t *children[16];
    uint16_t children_count = 0;
    float children_size = 0;

    // Gather up the children again so we can calculate the size
    int32_t child_idx = element->first_child_id;
    while (child_idx != -1) {
        ui_element_t *child = &state->elements[child_idx];

        children_size += is_row ? child->computed.layout.width + parse_spacing_axis(child->margin, element->computed.content.width, UI_HORIZONTAL) : child->computed.layout.height + parse_spacing_axis(child->margin, element->computed.content.width, UI_VERTICAL);
        children[children_count++] = child;

        child_idx = child->next_sibling_id;
    }

    float total_gap_x = MAX(0, (children_count - 1) * parse_spacing(element->gap.x, element->computed.layout.width));
    float total_gap_y = MAX(0, (children_count - 1) * parse_spacing(element->gap.y, element->computed.layout.height));
    float total_gap = is_row ? total_gap_x : total_gap_y;

    float main_axis_size = is_row ? element->computed.content.width : element->computed.content.height;
    float cross_axis_size = is_row ? element->computed.content.height : element->computed.content.width;

    float free_space = main_axis_size - children_size - total_gap;
    float cursor = is_row ? element->computed.content.x : element->computed.content.y;
    float spacing = 0.0f;

    switch (element->flex_main_axis_alignment) {
        case UI_FLEX_ALIGN_CENTER:
            cursor += free_space * 0.5f;
            break;
        case UI_FLEX_ALIGN_END:
            cursor += free_space;
            break;
        case UI_FLEX_ALIGN_SPACE_BETWEEN:
            spacing = children_count > 1 ? free_space / (children_count - 1) : 0.0f;
            break;
        case UI_FLEX_ALIGN_SPACE_AROUND:
            spacing = free_space / children_count;
            cursor += spacing * 0.5f;
            break;
        case UI_FLEX_ALIGN_SPACE_EVENLY:
            spacing = free_space / (children_count + 1);
            cursor += spacing;
            break;
        default:
            break;
    }

    for (uint16_t i = 0; i < children_count; ++i) {
        ui_element_t *child = children[i];

        float cross_start = is_row ? element->computed.content.y : element->computed.content.x;
        float child_cross_size = is_row ? (child->computed.layout.height + parse_spacing_axis(child->margin, element->computed.layout.width, UI_VERTICAL)) : (child->computed.layout.width + parse_spacing_axis(child->margin, element->computed.layout.width, UI_HORIZONTAL));

        float child_cross_pos = 0.0f;

        switch (element->flex_cross_axis_alignment) {
            case UI_FLEX_ALIGN_CENTER:
                child_cross_pos = cross_start + (cross_axis_size - child_cross_size) * 0.5f;
                break;
            case UI_FLEX_ALIGN_END:
                child_cross_pos = cross_start + cross_axis_size - child_cross_size;
                break;
            default:
                child_cross_pos = cross_start;
                break;
        }

        if (is_row) {
            ui_layout_position(state, child, cursor, child_cross_pos);
            cursor += child->computed.layout.width +
                      parse_spacing_axis(child->margin, element->computed.layout.width, UI_HORIZONTAL) +
                      spacing;

            if (i < children_count - 1) {
                cursor += parse_spacing(element->gap.x, element->computed.layout.width);
            }
        } else {
            ui_layout_position(state, child, child_cross_pos, cursor);
            cursor += child->computed.layout.height +
                      parse_spacing_axis(child->margin, element->computed.layout.width, UI_VERTICAL) +
                      spacing;

            if (i < children_count - 1) {
                cursor += parse_spacing(element->gap.y, element->computed.layout.height);
            }
        }
    }
}

static float parse_value(ui_value_t value, float max) {
    switch (value.unit) {
        case UI_UNIT_PIXEL:
            return value.value;
        case UI_UNIT_PERCENT:
            return (value.value * 0.01f) * max;
        case UI_UNIT_AUTO:
            return max;
    }
}

static float parse_spacing(ui_value_t spacing, float comparison_size) {
    switch (spacing.unit) {
        case UI_UNIT_PIXEL:
            return spacing.value;
        case UI_UNIT_PERCENT:
            return (spacing.value * 0.01f) * comparison_size;
        case UI_UNIT_AUTO:
            return 0;
    }
}

static float parse_spacing_axis(ui_spacing_t spacing, float comparison_value, bool horizontal) {
    float s1 = parse_spacing(horizontal ? spacing.left : spacing.top, comparison_value);
    float s2 = parse_spacing(horizontal ? spacing.right : spacing.bottom, comparison_value);
    return s1 + s2;
}
