#pragma once

#include "math.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum keycode {
    KEY_UNKNOWN = 0,
    KEY_CTRL,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_P,
    KEY_N,
    KEY_COUNT
} keycode_t;

typedef enum mousebutton {
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_X1,
    MOUSE_BUTTON_X2,
    MOUSE_BUTTON_COUNT
} mousebutton_t;

struct keyboard_state {
    bool keys[KEY_COUNT];
};

struct mouse_state {
    int16_t x, y;
    bool buttons[MOUSE_BUTTON_COUNT];
};

typedef struct input_state {
    struct keyboard_state keyboard_current;
    struct keyboard_state keyboard_previous;

    struct mouse_state mouse_current;
    struct mouse_state mouse_previous;
    int8_t mouse_scroll_delta;
} input_state_t;

bool input_initialize(input_state_t *state);
void input_swap_buffers(input_state_t* state);

void input_process_key(keycode_t key_code, bool pressed);
void input_process_mouse_button(mousebutton_t button, bool pressed);
void input_process_mouse_move(int16_t x, int16_t y);
void input_process_mouse_wheel(int32_t delta);

bool input_is_key_down(keycode_t key_code);
bool input_is_key_up(keycode_t key_code);
bool input_was_key_down(keycode_t key_code);
bool input_was_key_up(keycode_t key_code);
bool input_is_key_pressed(keycode_t key_code);
bool input_is_key_released(keycode_t key_code);

bool input_is_mouse_button_down(mousebutton_t button);
bool input_is_mouse_button_up(mousebutton_t button);
bool input_was_mouse_button_down(mousebutton_t button);
bool input_was_mouse_button_up(mousebutton_t button);
bool input_is_mouse_button_pressed(mousebutton_t button);
bool input_is_mouse_button_released(mousebutton_t button);

int2_t input_mouse_get_pos(void); 
int16_t input_mouse_get_x(void);
int16_t input_mouse_get_y(void);
int16_t input_mouse_get_delta_x(void);
int16_t input_mouse_get_delta_y(void);
int8_t input_mouse_get_wheel(void);
