#ifndef COMFYSTREAM_CLIENT_H
#define COMFYSTREAM_CLIENT_H

#include <rtc/rtc.hpp>
#include <obs-module.h>

class ComfyStreamClient {
public:
    explicit ComfyStreamClient(const std::string& serverUrl = "http://0.0.0.0:8888");
    ~ComfyStreamClient();

    void send_frame(obs_source_frame* frame);
    obs_source_frame* receive_frame();

private:
    std::string sendSDPToServer(const std::string& sdp);
    void cleanupConnection();

    std::string serverUrl;
    std::shared_ptr<rtc::PeerConnection> pc;
};

#endif
