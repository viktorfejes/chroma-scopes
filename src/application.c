#include "application.h"

#include "logger.h"
#include "renderer.h"
#include "ui.h"
#include "window.h"

static renderer_t renderer;
static window_t window;
static ui_state_t ui;

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

    // TEST: Insert a new element
    ui_element_t el = ui_create_element();
    el.type = UI_ELEMENT_TYPE_FLEX;
    el.flex_direction = UI_FLEX_DIRECTION_COL;
    // el.flex_main_axis_alignment = UI_FLEX_ALIGN_SPACE_EVENLY;
    // el.flex_cross_axis_alignment = UI_FLEX_ALIGN_CENTER;
    el.width = UI_VALUE(100, UI_UNIT_PERCENT);
    el.height = UI_VALUE(100, UI_UNIT_PERCENT);
    el.base_style.background_color = (float4_t){1.0f, 1.0f, 1.0f, 0.0f};
    // el.background_image = &renderer.vectorscope_texture;
    el.gap = (ui_gap_t){UI_VALUE(5, UI_UNIT_PIXEL), UI_VALUE(5, UI_UNIT_PIXEL)};
    el.padding = (ui_spacing_t){
        UI_VALUE(10, UI_UNIT_PIXEL),
        UI_VALUE(10, UI_UNIT_PIXEL),
        UI_VALUE(10, UI_UNIT_PIXEL),
        UI_VALUE(10, UI_UNIT_PIXEL),
    };
    uint16_t body = ui_insert_element(&ui, &el, 0);

    el.type = UI_ELEMENT_TYPE_BLOCK;
    el.width = UI_VALUE(400, UI_UNIT_AUTO);
    el.height = UI_VALUE(100, UI_UNIT_PIXEL);
    el.base_style.background_color = (float4_t){0.5f, 0.5f, 0.0f, 1.0f};
    ui_insert_element(&ui, &el, body);

    el.type = UI_ELEMENT_TYPE_FLEX;
    el.width = UI_VALUE(100, UI_UNIT_PERCENT);
    el.height = UI_VALUE(100, UI_UNIT_AUTO);
    el.flex_direction = UI_FLEX_DIRECTION_ROW;
    el.flex_grow = 1;
    el.base_style.background_color = (float4_t){1.0f, 1.0f, 1.0f, 1.0f};
    el.gap = (ui_gap_t){UI_VALUE(5, UI_UNIT_PIXEL), UI_VALUE(5, UI_UNIT_PIXEL)};
    el.padding = (ui_spacing_t){
        UI_VALUE(10, UI_UNIT_PIXEL),
        UI_VALUE(10, UI_UNIT_PIXEL),
        UI_VALUE(10, UI_UNIT_PIXEL),
        UI_VALUE(10, UI_UNIT_PIXEL),
    };
    uint16_t main = ui_insert_element(&ui, &el, body);

    el.type = UI_ELEMENT_TYPE_BLOCK;
    el.width = UI_VALUE(400, UI_UNIT_AUTO);
    el.height = UI_VALUE(100, UI_UNIT_PERCENT);
    el.flex_grow = 1;
    el.base_style.background_color = (float4_t){1.0f, 0.0f, 1.0f, 1.0f};
    ui_insert_element(&ui, &el, main);

    el.type = UI_ELEMENT_TYPE_FLEX;
    el.flex_direction = UI_FLEX_DIRECTION_COL;
    el.flex_main_axis_alignment = UI_FLEX_ALIGN_SPACE_BETWEEN;
    el.flex_cross_axis_alignment = UI_FLEX_ALIGN_STRETCH;
    el.width = UI_VALUE(400, UI_UNIT_PIXEL);
    el.height = UI_VALUE(100, UI_UNIT_PERCENT);
    el.flex_grow = 0;
    el.base_style.background_color = (float4_t){0.0f, 1.0f, 0.0f, 1.0f};
    uint16_t left = ui_insert_element(&ui, &el, main);

    el.type = UI_ELEMENT_TYPE_BLOCK;
    el.width = UI_VALUE(100, UI_UNIT_PERCENT);
    el.height = UI_VALUE(100, UI_UNIT_AUTO);
    el.flex_grow = 1;
    el.base_style.background_color = (float4_t){1.0f, 1.0f, 0.0f, 1.0f};
    ui_insert_element(&ui, &el, left);

    el.type = UI_ELEMENT_TYPE_BLOCK;
    el.width = UI_VALUE(200, UI_UNIT_PIXEL);
    el.height = UI_VALUE(100, UI_UNIT_AUTO);
    el.flex_grow = 1;
    el.base_style.background_color = (float4_t){1.0f, 0.0f, 0.0f, 1.0f};
    ui_insert_element(&ui, &el, left);

    el.type = UI_ELEMENT_TYPE_BLOCK;
    el.width = UI_VALUE(200, UI_UNIT_PIXEL);
    el.height = UI_VALUE(100, UI_UNIT_AUTO);
    el.flex_grow = 1;
    el.base_style.background_color = (float4_t){0.0f, 0.0f, 1.0f, 1.0f};
    ui_insert_element(&ui, &el, left);

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
}

static bool application_run(void) {
    LOG("Application is running");

    while (!window_should_close(&window)) {
        window_proc_messages();

        application_update();

        renderer_begin_frame(&renderer);
        renderer_draw_scopes(&renderer);

        renderer_draw_ui(&renderer, &ui.draw_list);

        renderer_draw_composite(&renderer);
        renderer_end_frame(&renderer);
    }

    return true;
}
