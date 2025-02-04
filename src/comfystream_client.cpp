#include "comfystream_client.h"
#include <rtc/rtc.hpp>
#include <obs-module.h>
#include <iostream>
#include <json/json.h>
#include <httplib.h>
#include <plugin-support.h>

using namespace std;

// Function to send offer to the Python server
Json::Value sendOfferToServer(const std::string& offerSdp, const std::string& serverUrl) {
    httplib::Client client(serverUrl.c_str());

    Json::Value requestBody;
    requestBody["offer"]["sdp"] = offerSdp;
    requestBody["offer"]["type"] = "offer";
    requestBody["prompt"] = "test-prompt";  // Example additional parameter

    Json::StreamWriterBuilder writer;
    std::string requestBodyStr = Json::writeString(writer, requestBody);

    auto res = client.Post("/offer", requestBodyStr, "application/json");

    if (res && res->status == 200) {
        Json::CharReaderBuilder reader;
        Json::Value response;
        std::string errors;
        std::istringstream s(res->body);

        if (!Json::parseFromStream(reader, s, &response, &errors)) {
            throw std::runtime_error("Failed to parse JSON response: " + errors);
        }

        return response;
    } else {
        throw std::runtime_error("Failed to send offer to server");
    }
}

// Constructor
// Constructor
ComfyStreamClient::ComfyStreamClient(const std::string& serverUrl) : serverUrl(serverUrl) {
    // Create WebRTC peer connection.
    rtc::Configuration config;
    auto pc = std::make_shared<rtc::PeerConnection>(config);
    obs_log(LOG_INFO, "Created WebRTC peer connection");

    // Create data channel.
    auto dc = pc->createDataChannel("control");
    dc->onOpen([]() { std::cout << "Data channel opened\n"; });

    // Correctly handle std::variant for messages
    dc->onMessage([](auto data) {
        std::visit([](auto&& arg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::string>) {
                std::cout << "Message received: " << arg << "\n";
            } else {
                std::cout << "Binary message received\n";
            }
        }, data);
    });

    obs_log(LOG_INFO, "Created data channel");

    // Set up offer and handle answer
    pc->onLocalDescription([&, pc](rtc::Description desc) {
        std::cout << "Generated local SDP offer:\n" << std::string(desc) << "\n";

        try {
            Json::Value response = sendOfferToServer(std::string(desc), serverUrl);

            pc->setRemoteDescription(rtc::Description(
                response["sdp"].asString(),
                response["type"].asString()
            ));

            std::cout << "Remote SDP set successfully\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    });

    // Monitor ICE candidate gathering
    pc->onLocalCandidate([](const rtc::Candidate& candidate) {
        std::cout << "Local ICE candidate gathered: " << candidate.candidate() << "\n";
    });

    // Monitor ICE state changes
    pc->onIceStateChange([](rtc::PeerConnection::IceState state) {
        std::cout << "ICE state changed: " << static_cast<int>(state) << "\n";
    });

    // Corrected Local Description Setting
    pc->setLocalDescription(rtc::Description::Type::Offer);
    std::cout << "Local description set successfully\n";

    std::cout << "Waiting for WebRTC connection...\n";
}


// Destructor
ComfyStreamClient::~ComfyStreamClient() {
    // Cleanup logic
}

// Sending and Receiving Frames (to be implemented)
void ComfyStreamClient::send_frame(obs_source_frame* /*frame*/) {
    // Convert OBS frame to WebRTC frame and send
}

obs_source_frame* ComfyStreamClient::receive_frame() {
    return nullptr;
}
