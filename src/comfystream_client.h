#ifndef COMFYSTREAM_CLIENT_H
#define COMFYSTREAM_CLIENT_H

#include <rtc/rtc.hpp>
#include <obs-module.h>
#include <json/json.h>

class ComfyStreamClient {
public:
    explicit ComfyStreamClient(const std::string& serverUrl);
    ~ComfyStreamClient();

    void send_frame(obs_source_frame* frame);
    obs_source_frame* receive_frame();

private:
    std::string serverUrl;
};

#endif
