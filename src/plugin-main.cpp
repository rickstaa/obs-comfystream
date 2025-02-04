#include <obs-module.h>
#include <plugin-support.h>
#include "comfystream_client.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

struct filter_data {
    obs_source_t *source;
    ComfyStreamClient *client;
};

static inline struct filter_data *get_filter_data(void *data)
{
    return (struct filter_data *)data;
}

static const char *filter_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "ComfyStream";
}

static void *filter_create(obs_data_t * /*settings*/, obs_source_t *source)
{
    struct filter_data *filter = (struct filter_data *)bzalloc(sizeof(struct filter_data));
    filter->source = source;
    filter->client = new ComfyStreamClient("https://0.0.0.0:8888");
    return filter;
}

static void filter_destroy(void *data)
{
    struct filter_data *filter = get_filter_data(data);
    delete filter->client;
    bfree(filter);
}

static void filter_tick(void *data, float /*seconds*/)
{
    struct filter_data *filter = get_filter_data(data);
    if (!filter || !filter->source) return;

    obs_source_t *target = obs_filter_get_target(filter->source);
    if (!target) {
        obs_log(LOG_WARNING, "ComfyStream Filter: No target source found.");
        return;
    }

    obs_source_frame *frame = obs_source_get_frame(target);
    if (!frame) {
        // obs_log(LOG_WARNING, "ComfyStream Filter: No frame received from target source.");
        return;
    }

    filter->client->send_frame(frame);
    obs_source_release_frame(target, frame);
}

static void filter_video_render(void *data, gs_effect_t * /*effect*/)
{
    struct filter_data *filter = get_filter_data(data);
    if (!filter || !filter->source) return;

    obs_source_t *target = obs_filter_get_target(filter->source);
    if (!target) {
        obs_log(LOG_WARNING, "ComfyStream Filter: No target source found.");
        return;
    }

    obs_source_frame *new_frame = filter->client->receive_frame();
    if (!new_frame) return;

    obs_source_output_video(filter->source, new_frame);
    obs_source_frame_destroy(new_frame);
}

static struct obs_source_info filter_info = {
    .id = "comfystream_filter",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO,
    .get_name = filter_get_name,
    .create = filter_create,
    .destroy = filter_destroy,
    .video_tick = filter_tick,
    .video_render = filter_video_render
};

bool obs_module_load(void)
{
    obs_register_source(&filter_info);
    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "plugin unloaded");
}
