#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-websocket-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-canvas-preview", "en-US")

static void handle_set_preview_enabled(obs_data_t *request_data, obs_data_t *response_data)
{
    bool enable = obs_data_get_bool(request_data, "enabled");

    obs_frontend_preview_program_enable(enable);

    // Add response field
    obs_data_set_bool(response_data, "currentPreviewEnabled", obs_frontend_preview_program_enabled());
}

bool obs_module_load(void)
{
    blog(LOG_INFO, "[canvas-preview] Registering WebSocket request...");

    // Register request: SetCanvasPreviewEnabled
    obs_websocket_register_request(
        "SetCanvasPreviewEnabled",
        handle_set_preview_enabled
    );

    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[canvas-preview] Unloaded");
}
