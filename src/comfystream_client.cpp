#include "comfystream_client.h"
#include <rtc/rtc.hpp>
#include <obs-module.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <plugin-support.h>

using namespace std;
using json = nlohmann::json;

const int SERVER_TIMEOUT = 60; // seconds

// Default media pass-through pipeline prompt for the AI WebRTC Server.
const std::string defaultPipelinePrompt =
	R"({ "12": { "inputs": { "image": "sampled_frame.jpg", "upload": "image" }, "class_type": "LoadImage", "_meta": { "title": "Load Image" } }, "13": { "inputs": { "images": ["12", 0] }, "class_type": "PreviewImage", "_meta": { "title": "Preview Image" } } })";

/**
 * Send the SDP offer to the ComfyStream server.
 * @param sdp The SDP offer to send.
 * @return The server response.
 */
std::string ComfyStreamClient::sendSDPToServer(const std::string &sdp)
{
	httplib::Client cli(serverUrl);

	json request;
	request["offer"] = {{"sdp", sdp}, {"type", "offer"}};
	json prompt = json::parse(defaultPipelinePrompt);
	request["prompt"] = prompt;

	auto res = cli.Post("/offer", request.dump(), "application/json");
	return (res && res->status == 200) ? res->body : "";
}

/**
 * Detach all handlers and clean up the remote connection.
 */
void ComfyStreamClient::cleanupConnection()
{
    if (pc) {
        obs_log(LOG_INFO, "Cleaning up WebRTC peer connection");

        // Detach handlers.
        pc->onLocalDescription(nullptr);
        pc->onStateChange(nullptr);
        pc->onLocalCandidate(nullptr);
        pc->onIceStateChange(nullptr);
        pc->onSignalingStateChange(nullptr);
        pc->onGatheringStateChange(nullptr);
        pc->onTrack(nullptr);

        // Close the peer connection.
        pc->close();
        obs_log(LOG_INFO, "WebRTC peer connection closed");
    }
}

/**
 * ComfyStream client constructor.
 * 
 * @param url The ComfyStream server URL.
 */
ComfyStreamClient::ComfyStreamClient(const std::string &url) : serverUrl(url)
{
	rtc::InitLogger(rtc::LogLevel::Info);

	// Create WebRTC peer connection.
	obs_log(LOG_INFO, "Creating WebRTC peer connection");
	rtc::Configuration config;
	config.iceServers.emplace_back("stun:stun.l.google.com:19302");
	pc = std::make_shared<rtc::PeerConnection>(config);

	// Create WebRTC handlers.
	pc->onLocalDescription([this](rtc::Description sdp) {
		obs_log(LOG_DEBUG, "Generated SDP Offer:\n%s", std::string(sdp).c_str());
	});
	pc->onStateChange([](rtc::PeerConnection::State state) {
		obs_log(LOG_DEBUG, "[PeerConnection State Change] New state: %d", static_cast<int>(state));
	});
	pc->onLocalCandidate([](rtc::Candidate candidate) {
		obs_log(LOG_DEBUG, "[Local Candidate] %s", candidate.candidate().c_str());
	});
	pc->onIceStateChange([](rtc::PeerConnection::IceState state) {
		obs_log(LOG_DEBUG, "[ICE State Change] New state: %d", static_cast<int>(state));
	});
	pc->onSignalingStateChange([](rtc::PeerConnection::SignalingState state) {
		obs_log(LOG_DEBUG, "[Signaling State Change] New state: %d", static_cast<int>(state));
	});

	// Connect to ComfyStream WebRTC server.
	pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
		// Use non-trickling ICE.
		if (state == rtc::PeerConnection::GatheringState::Complete) {
			obs_log(LOG_DEBUG, "All ICE candidates have been gathered");

			// Retrieve local description and send it to the server.
			auto description = pc->localDescription();
			if (!description) {
				obs_log(LOG_ERROR, "Failed to get local description");
				cleanupConnection();
				return;
			}
			std::string server_response;
			try {
				server_response = sendSDPToServer(static_cast<std::string>(*description));
			} catch (const std::exception &e) {
				obs_log(LOG_ERROR, "Failed to send SDP offer to server: %s", e.what());
				cleanupConnection();
				return;
			}

			// Set remote description from server response.
			if (server_response.empty()) {
				obs_log(LOG_ERROR, "Failed to receive SDP answer from server");
				cleanupConnection();
				return;
			}
			obs_log(LOG_DEBUG, "Received SDP Answer:\n%s", server_response.c_str());
			json response_json = json::parse(server_response);
			std::string sdp_answer = response_json["sdp"];
			pc->setRemoteDescription(rtc::Description(sdp_answer, "answer"));
			obs_log(LOG_INFO, "WebRTC connection established");
		}
	});

	// Add video channel.
	const rtc::SSRC ssrc = 42;
	rtc::Description::Video media("video", rtc::Description::Direction::SendRecv);
	media.addH264Codec(102);
	media.addSSRC(ssrc, "video-sendrecv");
	auto track = pc->addTrack(media);
	if (!track) {
		obs_log(LOG_ERROR, "Failed to add video track");
		cleanupConnection();
		return;
	}
	pc->onTrack([](std::shared_ptr<rtc::Track> track) {
		obs_log(LOG_DEBUG, "[Track] Added with mid: %s", track->mid().c_str());
	});

	// Add control data channel.
	auto dc = pc->createDataChannel("control");
	if (!dc) {
		obs_log(LOG_ERROR, "Failed to create data channel");
		cleanupConnection();
		return;
	}
	dc->onOpen([]() {
		obs_log(LOG_DEBUG, "[Data Channel] Opened");
	});
	dc->onClosed([&]() { obs_log(LOG_DEBUG, "[Data Channel] Closed"); });
	dc->onError([](std::string error) { obs_log(LOG_ERROR, "[Data Channel] Error: %s", error.c_str()); });
	dc->onMessage([](auto data) {
		if (std::holds_alternative<std::string>(data)) {
			obs_log(LOG_DEBUG, "[Data Channel] Received: %s", std::get<std::string>(data).c_str());
		}
	});

	obs_log(LOG_INFO, "ComfyStream client initialized");
}

/**
 * ComfyStream client destructor.
 */
ComfyStreamClient::~ComfyStreamClient()
{
	cleanupConnection();
	obs_log(LOG_DEBUG, "ComfyStream client destroyed");
}

/**
 * Send an OBS frame to the ComfyStream server.
 * 
 * @param frame The OBS frame to send.
 */
void ComfyStreamClient::send_frame(obs_source_frame * /*frame*/)
{
	// TODO: Implement frame sending.
}

/**
 * Receive a frame from the ComfyStream server.
 * 
 * @return The received OBS frame.
 */
obs_source_frame *ComfyStreamClient::receive_frame()
{	
	// TODO: Implement frame receiving.
	return nullptr;
}
