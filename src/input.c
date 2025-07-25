#include "input.h"

#include <assert.h>
#include <string.h>

static input_state_t *state_ptr = NULL;

bool input_initialize(input_state_t *state) {
    assert(state);

    state_ptr = state;
    memset(state_ptr, 0, sizeof(input_state_t));

    return true;
}

void input_swap_buffers(input_state_t *state) {
    assert(state);

    // Copy keyboard and mouse states to previous, and zeroing out mouse delta
    memcpy(&state->keyboard_previous, &state->keyboard_current, sizeof(struct keyboard_state));
    memcpy(&state->mouse_previous, &state->mouse_current, sizeof(struct mouse_state));
    state->mouse_scroll_delta = 0;
}

void input_process_key(keycode_t key_code, bool pressed) {
    if (state_ptr->keyboard_current.keys[key_code] != pressed) {
        state_ptr->keyboard_current.keys[key_code] = pressed;
    }
}

void input_process_mouse_button(mousebutton_t button, bool pressed) {
    if (state_ptr->mouse_current.buttons[button] != pressed) {
        state_ptr->mouse_current.buttons[button] = pressed;
    }
}

void input_process_mouse_move(int16_t x, int16_t y) {
    state_ptr->mouse_current.x = x;
    state_ptr->mouse_current.y = y;
}

void input_process_mouse_wheel(int32_t delta);

bool input_is_key_down(keycode_t key_code) {
    return state_ptr->keyboard_current.keys[key_code] == true;
}

bool input_is_key_up(keycode_t key_code) {
    return state_ptr->keyboard_current.keys[key_code] == false;
}

bool input_was_key_down(keycode_t key_code) {
    return state_ptr->keyboard_previous.keys[key_code] == true;
}

bool input_was_key_up(keycode_t key_code) {
    return state_ptr->keyboard_previous.keys[key_code] == false;
}

bool input_is_key_pressed(keycode_t key_code) {
    return input_was_key_up(key_code) && input_is_key_down(key_code);
}

bool input_is_key_released(keycode_t key_code) {
    return input_was_key_down(key_code) && input_is_key_up(key_code);
}

bool input_is_mouse_button_down(mousebutton_t button) {
    return state_ptr->mouse_current.buttons[button] == true;
}

bool input_is_mouse_button_up(mousebutton_t button) {
    return state_ptr->mouse_current.buttons[button] == false;
}

bool input_was_mouse_button_down(mousebutton_t button) {
    return state_ptr->mouse_previous.buttons[button] == true;
}

bool input_was_mouse_button_up(mousebutton_t button) {
    return state_ptr->mouse_previous.buttons[button] == false;
}

bool input_is_mouse_button_pressed(mousebutton_t button) {
    return input_was_mouse_button_up(button) && input_is_mouse_button_down(button);
}

bool input_is_mouse_button_released(mousebutton_t button) {
    return input_was_mouse_button_down(button) && input_is_mouse_button_up(button);
}

int2_t input_mouse_get_pos(void) {
    return (int2_t){state_ptr->mouse_current.x, state_ptr->mouse_current.y};
}

int16_t input_mouse_get_x(void);
int16_t input_mouse_get_y(void);
int16_t input_mouse_get_delta_x(void);
int16_t input_mouse_get_delta_y(void);
int8_t input_mouse_get_wheel(void);
