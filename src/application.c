#include "application.h"

#include "vectorscope.h"
#include "macros.h"
#include "input.h"
#include "logger.h"
#include "renderer.h"
#include "ui.h"
#include "window.h"

#define TARGET_FPS 60
#define FIXED_TIMESTEP (1.0 / TARGET_FPS)
#define MAX_FRAME_TIME 0.25 // 250ms max

static renderer_t renderer;
static window_t window;
static ui_state_t ui;
static input_state_t input;

// TODO: implement
typedef struct app_state {
    /* System states */
    renderer_t *renderer;
    window_t *window;
    ui_state_t *ui;
    input_state_t *input;

    /* Game loop fields */
} app_state_t;

static bool application_initialize(void);
static void application_terminate(void);
static void application_update(void);
static bool application_run(void);

void application_start(void) {
    LOG("Application started");

    if (!application_initialize()) {
        LOG("Couldn't initialize application");
        goto error;
    }

    if (!application_run()) {
        LOG("Couldn't run application");
        goto error;
    }

error:
    application_terminate();
}

static bool application_initialize(void) {
    LOG("Application is initializing");

    // Create the window (we'll probably only ever have one, unless...)
    if (!window_create("Chroma Scopes", 1280, 720, &window)) {
        LOG("Couldn't create window");
        return false;
    }

    // Initialize Input System
    if (!input_initialize(&input)) {
        LOG("Failed to initialize input system");
        return false;
    }

    // Initialize the renderer
    if (!renderer_initialize(&window, &renderer)) {
        LOG("Failed to initialize renderer");
        return false;
    }

    // Initialize the UI system
    if (!ui_initialize(&ui, window.width, window.height)) {
        LOG("Failed to initialize UI system");
        return false;
    }

    // Get the vectorscope texture
    texture_t *vs_tex = vectorscope_get_texture(&renderer.vectorscope);
    texture_t *wf_tex = waveform_get_texture(&renderer.waveform);

    uint16_t body, header, row1, row2, tl_comp, tr_comp, bl_comp, br_comp;
    {
        ui_element_t el = ui_create_element();
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.flex_direction = UI_FLEX_DIRECTION_COL;
        el.gap = (ui_gap_t){
            .x = UI_VALUE(2, UI_UNIT_PIXEL),
            .y = UI_VALUE(2, UI_UNIT_PIXEL),
        },
        el.width = UI_VALUE(100, UI_UNIT_PERCENT);
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){0.0f, 0.0f, 0.0f, 0.0f};
        body = ui_insert_element(&ui, &el, 0);
    }
    {
        ui_element_t el = ui_create_element();
        el.width = UI_VALUE(100, UI_UNIT_PERCENT);
        el.height = UI_VALUE(60, UI_UNIT_PIXEL);
        el.base_style.background_color = (float4_t){0.16f, 0.16f, 0.16f, 1.0f};
        header = ui_insert_element(&ui, &el, body);
    }
    {
        ui_element_t el = ui_create_element();
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.flex_direction = UI_FLEX_DIRECTION_ROW;
        el.flex_grow = 1;
        el.width = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 0.0f, 0.0f, 0.0f};
        row1 = ui_insert_element(&ui, &el, body);
    }
    {
        ui_element_t el = ui_create_element();
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.flex_direction = UI_FLEX_DIRECTION_ROW;
        el.flex_grow = 1;
        el.width = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){0.0f, 0.0f, 0.0f, 0.0f};
        row2 = ui_insert_element(&ui, &el, body);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 1.0f, 1.0f, 1.0f};
        el.base_style.background_image = vs_tex;
        tl_comp = ui_insert_element(&ui, &el, row1);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 1.0f, 1.0f, 1.0f};
        el.base_style.background_image = wf_tex;
        tr_comp = ui_insert_element(&ui, &el, row1);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 1.0f, 1.0f, 1.0f};
        el.base_style.background_image = &renderer.blit_texture;
        bl_comp = ui_insert_element(&ui, &el, row2);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 1.0f, 0.0f, 0.1f};
        br_comp = ui_insert_element(&ui, &el, row2);
    }

    UNUSED(header);
    UNUSED(tl_comp);
    UNUSED(tr_comp);
    UNUSED(bl_comp);
    UNUSED(br_comp);

    ui_layout_measure(&ui, &ui.elements[0], 0.0f, (float)window.width, 0.0f, (float)window.height);
    ui_layout_position(&ui, &ui.elements[0], 0.0f, 0.0f);
    ui_draw(&ui, &ui.elements[0]);

    capture_set_monitor(&renderer.capture, renderer.device, 1);

    return true;
}

static void application_terminate(void) {
    LOG("Application is terminating");
    window_destroy(&window);

    renderer_terminate(&renderer);
}

static void application_update(void) {
    capture_frame(&renderer.capture, (rect_t){0, 0, 500, 500}, renderer.context, &renderer.blit_texture);
}

static bool application_run(void) {
    LOG("Application is running");

    while (!window_should_close(&window)) {
        window_proc_messages();

        application_update();

        renderer_begin_frame(&renderer);
            vectorscope_render(&renderer.vectorscope, &renderer, &renderer.blit_texture);
            waveform_render(&renderer.waveform, &renderer, &renderer.blit_texture);
            renderer_draw_ui(&renderer, &ui.draw_list);
            renderer_draw_composite(&renderer);
        renderer_end_frame(&renderer);

        input_swap_buffers(&input);
    }

    return true;
}
