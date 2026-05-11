#include "rtsp_server.h"
#include "config.h"
#include "board_config.h"
#include "status_led.h"

// Minimum free heap required to accept a new RTSP client.
// Below this, the ESP32 risks OOM crashes during frame encoding.
#define MIN_HEAP_FOR_CLIENT  32000

WiFiServer rtspServer(RTSP_PORT);
CRtspSession *session = nullptr;

// Conditionally define the streamer type.
#ifdef VIDEO_CODEC_H264
    CStreamer *streamer = nullptr;
    static bool s_h264Active = false;
#else
    MyStreamer *streamer = nullptr;
#endif

String getRTSPUrl() {
    #ifdef VIDEO_CODEC_H264
        return "rtsp://" + WiFi.localIP().toString() + ":" + String(RTSP_PORT) + "/h264/1";
    #else
        return "rtsp://" + WiFi.localIP().toString() + ":" + String(RTSP_PORT) + "/mjpeg/1";
    #endif
}

const char* getCodecName() {
    #ifdef VIDEO_CODEC_H264
        return "H.264";
    #else
        return "MJPEG";
    #endif
}

static void getH264Resolution(uint16_t &width, uint16_t &height) {
    #ifdef H264_HW_ENCODER
        width  = 1280;
        height = 720;
    #else
        width  = 640;
        height = 480;
    #endif
}

void rtsp_server_start() {
    #ifdef VIDEO_CODEC_H264
        LOG_I("Creating H.264 streamer...");
        H264Streamer *h264 = new H264Streamer();
        uint16_t width, height;
        getH264Resolution(width, height);

        if (!h264->init(width, height)) {
            LOG_E("H.264 encoder init failed! Falling back to MJPEG.");
            delete h264;
            if (streamer) delete streamer;
            streamer = new MyStreamer();
            s_h264Active = false;
        } else {
            if (streamer) delete streamer;
            streamer = h264;
            s_h264Active = true;
        }
        LOG_I("RTSP server started at " + getRTSPUrl() + (s_h264Active ? " (" + String(getCodecName()) + ")" : " (MJPEG fallback)"));
    #else
        LOG_I("Creating MJPEG streamer...");
        if (!streamer) streamer = new MyStreamer();
        LOG_I("RTSP server started at " + getRTSPUrl());
    #endif

    rtspServer.begin();

    #ifdef BOARD_NAME
        LOG_I("Board: " + String(BOARD_NAME) + ", Codec: " + String(getCodecName()));
    #endif
}

void rtsp_server_loop() {
    // Accept new clients (drains TCP backlog even if we reject)
    WiFiClient client = rtspServer.available();
    if (client) {
        // Guard 1: Only one RTSP session at a time
        if (session) {
            LOG_I("RTSP Client rejected: session already active");
            client.stop();
        }
        // Guard 2: Reject if heap is dangerously low
        else if (ESP.getFreeHeap() < MIN_HEAP_FOR_CLIENT) {
            LOG_I("RTSP Client rejected: low heap (" + String(ESP.getFreeHeap()) + " bytes)");
            client.stop();
        }
        else {
            // Allocate WiFiClient on heap so it survives this scope
            WiFiClient *clientPtr = new WiFiClient(client);

            // Ensure streamer exists
            if (!streamer) {
                LOG_E("Streamer is NULL! Re-initializing...");
                #ifdef VIDEO_CODEC_H264
                    if (s_h264Active) {
                        uint16_t width, height;
                        getH264Resolution(width, height);
                        H264Streamer *h264 = new H264Streamer();
                        if (!h264->init(width, height)) {
                            LOG_E("H.264 reinit failed. Using MJPEG.");
                            delete h264;
                            streamer = new MyStreamer();
                            s_h264Active = false;
                        } else {
                            streamer = h264;
                        }
                    } else {
                        streamer = new MyStreamer();
                    }
                #else
                    streamer = new MyStreamer();
                #endif
            }

            if (streamer) {
                streamer->setClientSocket(clientPtr);
                session = new CRtspSession(clientPtr, streamer);
                LOG_I("RTSP Client Connected (" + String(getCodecName()) + ", heap: " + String(ESP.getFreeHeap()) + ")");

                #ifdef VIDEO_CODEC_H264
                    if (s_h264Active) {
                        static_cast<H264Streamer*>(streamer)->requestIDR();
                    }
                #endif
            } else {
                LOG_E("Streamer init failed. Closing client.");
                clientPtr->stop();
                delete clientPtr;
            }
        }
    }

    // Service existing session
    if (session) {
        session->handleRequests(0);  // Non-blocking

        // Frame rate limiting
        static uint32_t lastFrameTime = 0;
        uint32_t now = millis();

        #ifdef VIDEO_CODEC_H264
            uint32_t frameInterval = 1000 / H264_FPS;
        #else
            uint32_t frameInterval = 50;  // ~20 FPS
        #endif

        if (now - lastFrameTime > frameInterval) {
            if (!session->m_stopped) {
                session->broadcastCurrentFrame(now);
            }
            lastFrameTime = now;
        }

        // Teardown on disconnect
        if (session->m_stopped) {
            LOG_I("RTSP client disconnected (heap: " + String(ESP.getFreeHeap()) + ")");
            delete session;
            session = nullptr;
        }
    }
}
