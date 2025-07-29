#include "application.h"

#include "input.h"
#include "logger.h"
#include "macros.h"
#include "renderer.h"
#include "texture.h"
#include "ui.h"
#include "vectorscope.h"
#include "window.h"

#define TARGET_FPS 30
#define FIXED_TIMESTEP (1.0 / TARGET_FPS)
#define MAX_FRAME_TIME 0.25 // 250ms max

static renderer_t renderer;
static window_t window;
static ui_state_t ui;
static input_state_t input;

static texture_t spritesheet;

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
static void application_update(double dt);
static bool application_run(void);

static bool interact_close(ui_element_t *el);
static bool interact_minimize(ui_element_t *el);
static bool interact_restore(ui_element_t *el);

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

static bool test_ui_events(ui_element_t *el) {
    if (input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT)) {
        LOG("The vectorscope and only the vectorscope was clicked! (%d)", el->id);
        return true;
    }

    return false;
}

static void test_hover(ui_element_t *el, bool is_hovered) {
    if (is_hovered) {
        el->base_style.background_color = (float4_t){0.8f, 0.0f, 0.0f, 1.0f};
    } else {
        el->base_style.background_color = (float4_t){0.3f, 0.3f, 0.3f, 1.0f};
    }
}

static bool application_initialize(void) {
    LOG("Application is initializing");

    // Create the platform -- there is not a lot for this app for now, but we need it
    if (!platform_initialize()) {
        LOG("Failed to initialize platform layer");
        return false;
    }

    // Create the window (we'll probably only ever have one, unless...)
    if (!window_create("Chroma Scopes", 1200, 710, &window)) {
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

    if (!texture_load(renderer.device, "assets/spritesheet.png", TEXTURE_FORMAT_LDR_SRGB, &spritesheet)) {
        LOG("Failed to load test spritesheet");
    }

    // Get the vectorscope texture
    texture_t *vs_tex = vectorscope_get_texture(&renderer.vectorscope);
    texture_t *wf_tex = waveform_get_texture(&renderer.waveform);

    uint16_t body, header, row1, row2, tl_comp, tr_comp, bl_comp, br_comp, title, buttons,
        minimize, maximize, close;
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
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.flex_direction = UI_FLEX_DIRECTION_ROW;
        el.flex_main_axis_alignment = UI_FLEX_ALIGN_SPACE_BETWEEN;
        el.flex_cross_axis_alignment = UI_FLEX_ALIGN_CENTER;
        el.width = UI_VALUE(100, UI_UNIT_PERCENT);
        el.height = UI_VALUE(32, UI_UNIT_PIXEL);
        el.base_style.background_color = (float4_t){0.16f, 0.16f, 0.16f, 1.0f};
        header = ui_insert_element(&ui, &el, body);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(16, UI_UNIT_PIXEL);
        el.base_style.background_color = (float4_t){0.5f, 0.5f, 0.5f, 1.0f};
        title = ui_insert_element(&ui, &el, header);
    }
    {
        ui_element_t el = ui_create_element();
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.width = UI_VALUE(96, UI_UNIT_PIXEL);
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){0.3f, 0.3f, 0.3f, 1.0f};
        buttons = ui_insert_element(&ui, &el, header);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){0.3f, 0.3f, 0.3f, 1.0f};
        el.base_style.background_image = &spritesheet;
        el.base_style.background_uv = ui_calc_uv_from_pixels(64, 0, 32, 32, 512, 512);
        el.handle_mouse = interact_minimize;
        minimize = ui_insert_element(&ui, &el, buttons);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){0.3f, 0.3f, 0.3f, 1.0f};
        el.base_style.background_image = &spritesheet;
        el.base_style.background_uv = ui_calc_uv_from_pixels(32, 0, 32, 32, 512, 512);
        el.handle_mouse = interact_restore;
        maximize = ui_insert_element(&ui, &el, buttons);
    }
    {
        ui_element_t el = ui_create_element();
        el.flex_grow = 1;
        el.height = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 0.3f, 0.3f, 1.0f};
        el.base_style.background_image = &spritesheet;
        el.base_style.background_uv = ui_calc_uv_from_pixels(0, 0, 32, 32, 512, 512);
        el.handle_hover_change = test_hover;
        el.handle_mouse = interact_close;
        close = ui_insert_element(&ui, &el, buttons);
    }
    {
        ui_element_t el = ui_create_element();
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.flex_direction = UI_FLEX_DIRECTION_ROW;
        el.gap = (ui_gap_t){
            .x = UI_VALUE(2, UI_UNIT_PIXEL),
            .y = UI_VALUE(2, UI_UNIT_PIXEL),
        },
        el.flex_grow = 1;
        el.width = UI_VALUE(100, UI_UNIT_PERCENT);
        el.base_style.background_color = (float4_t){1.0f, 0.0f, 0.0f, 0.0f};
        row1 = ui_insert_element(&ui, &el, body);
    }
    {
        ui_element_t el = ui_create_element();
        el.type = UI_ELEMENT_TYPE_FLEX;
        el.flex_direction = UI_FLEX_DIRECTION_ROW;
        el.gap = (ui_gap_t){
            .x = UI_VALUE(2, UI_UNIT_PIXEL),
            .y = UI_VALUE(2, UI_UNIT_PIXEL),
        },
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
        el.handle_mouse = test_ui_events;
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
    UNUSED(buttons);
    UNUSED(title);
    UNUSED(close);
    UNUSED(maximize);
    UNUSED(minimize);

    ui_layout_measure(&ui, &ui.elements[0], 0.0f, (float)window.width, 0.0f, (float)window.height);
    ui_layout_position(&ui, &ui.elements[0], 0.0f, 0.0f);

    // Also make this the draggable area
    window_set_custom_dragarea(&window, ui.elements[title].computed.layout);

    capture_set_monitor(&renderer.capture, renderer.device, 1);

    return true;
}

static void application_terminate(void) {
    LOG("Application is terminating");
    window_destroy(&window);

    renderer_terminate(&renderer);
}

static void application_update(double dt) {
    UNUSED(dt);
    // TEMP: just to test...
    static bool on_top = false;
    if (input_is_key_down(KEY_CTRL) && input_is_key_pressed(KEY_P)) {
        window_set_always_on_top(&window, (on_top = !on_top));
    }

    if (input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT)) {
        if (ui.curr_hovered_element_id != -1) {
            ui_element_t *el = &ui.elements[ui.curr_hovered_element_id];
            int2_t mouse_pos = input_mouse_get_pos();

            LOG("Element (%d): Position (%.2f, %.2f), Size (%.2f, %.2f)",
                el->id,
                el->computed.layout.x, el->computed.layout.y,
                el->computed.layout.width, el->computed.layout.height);
            LOG("Mouse Position: (%d, %d)", mouse_pos.x, mouse_pos.y);
        }
    }

    capture_frame(&renderer.capture, (rect_t){0, 0, 500, 500}, renderer.context, &renderer.blit_texture);
}

static bool application_run(void) {
    LOG("Application is running");
    double last_time = platform_get_seconds();
    double accumulator = 0.0;

    while (!window_should_close(&window)) {
        window_proc_messages();

        double current_time = platform_get_seconds();
        double elapsed = current_time - last_time;
        last_time = current_time;

        if (elapsed > MAX_FRAME_TIME) {
            elapsed = MAX_FRAME_TIME;
        }

        accumulator += elapsed;

        while (accumulator >= FIXED_TIMESTEP) {
            ui_handle_mouse(&ui);

            application_update(FIXED_TIMESTEP);
            input_swap_buffers(&input);

            accumulator -= FIXED_TIMESTEP;
        }

        // application_render...
        // could add interpolation for rendering as well
        renderer_begin_frame(&renderer);
        vectorscope_render(&renderer.vectorscope, &renderer, &renderer.blit_texture);
        waveform_render(&renderer.waveform, &renderer, &renderer.blit_texture);
        renderer_draw_ui(&renderer, &ui, &ui.elements[0], false);
        renderer_draw_composite(&renderer);
        renderer_end_frame(&renderer);

        // Limit FPS
        double frame_time = platform_get_seconds() - current_time;
        if (frame_time < FIXED_TIMESTEP) {
            platform_sleep(FIXED_TIMESTEP - frame_time);
        }
    }

    return true;
}

static bool interact_close(ui_element_t *el) {
    UNUSED(el);
    if (input_is_mouse_button_down(MOUSE_BUTTON_LEFT)) {
        window_post_close(&window);
        return true;
    }
    return false;
}

static bool interact_minimize(ui_element_t *el) {
    UNUSED(el);
    if (input_is_mouse_button_down(MOUSE_BUTTON_LEFT)) {
        window_minimize(&window);
        return true;
    }
    return false;
}

static bool interact_restore(ui_element_t *el) {
    UNUSED(el);
    if (input_is_mouse_button_down(MOUSE_BUTTON_LEFT)) {
        window_maximize_restore(&window);
        return true;
    }
    return false;
}
