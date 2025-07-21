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

void input_process_key(keycode_t key_code, bool pressed);
void input_process_mouse_button(mousebutton_t button, bool pressed);

void input_process_mouse_move(int16_t x, int16_t y) {
    state_ptr->mouse_current.x = x;
    state_ptr->mouse_current.y = y;
}

void input_process_mouse_wheel(int32_t delta);

int16_t input_mouse_get_x(void);
int16_t input_mouse_get_y(void);
int16_t input_mouse_get_delta_x(void);
int16_t input_mouse_get_delta_y(void);
int8_t input_mouse_get_wheel(void);
