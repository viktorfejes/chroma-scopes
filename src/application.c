#include "application.h"

#include "logger.h"
#include "renderer.h"
#include "window.h"

static renderer_t renderer;
static window_t window;

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
        renderer_draw(&renderer);
        renderer_end_frame(&renderer);
    }

    return true;
}
