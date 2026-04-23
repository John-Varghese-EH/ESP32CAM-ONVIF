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
        Serial.println("[INFO] Creating H.264 streamer...");
        H264Streamer *h264 = new H264Streamer();
        uint16_t width, height;
        getH264Resolution(width, height);

        if (!h264->init(width, height)) {
            Serial.println("[ERROR] H.264 encoder init failed! Falling back to MJPEG.");
            delete h264;
            streamer = new MyStreamer();
            s_h264Active = false;
        } else {
            streamer = h264;
            s_h264Active = true;
        }
        Serial.printf("[INFO] RTSP server started at %s (%s)\n",
                      getRTSPUrl().c_str(), s_h264Active ? getCodecName() : "MJPEG (fallback)");
    #else
        Serial.println("[INFO] Creating MJPEG streamer...");
        streamer = new MyStreamer();
        Serial.println("[INFO] RTSP server started at " + getRTSPUrl());
    #endif

    rtspServer.begin();

    #ifdef BOARD_NAME
        Serial.printf("[INFO] Board: %s, Codec: %s\n", BOARD_NAME, getCodecName());
    #endif
}

void rtsp_server_loop() {
    // Accept new clients (drains TCP backlog even if we reject)
    WiFiClient client = rtspServer.available();
    if (client) {
        // Guard 1: Only one RTSP session at a time
        if (session) {
            Serial.println("[WARN] RTSP Client rejected: session already active");
            client.stop();
        }
        // Guard 2: Reject if heap is dangerously low
        else if (ESP.getFreeHeap() < MIN_HEAP_FOR_CLIENT) {
            Serial.printf("[WARN] RTSP Client rejected: low heap (%u bytes)\n", ESP.getFreeHeap());
            client.stop();
        }
        else {
            // Allocate WiFiClient on heap so it survives this scope
            WiFiClient *clientPtr = new WiFiClient(client);

            // Ensure streamer exists
            if (!streamer) {
                Serial.println("[ERROR] Streamer is NULL! Re-initializing...");
                #ifdef VIDEO_CODEC_H264
                    if (s_h264Active) {
                        uint16_t width, height;
                        getH264Resolution(width, height);
                        H264Streamer *h264 = new H264Streamer();
                        if (!h264->init(width, height)) {
                            Serial.println("[ERROR] H.264 reinit failed. Using MJPEG.");
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
                Serial.printf("[INFO] RTSP Client Connected (%s, heap: %u)\n",
                              getCodecName(), ESP.getFreeHeap());

                #ifdef VIDEO_CODEC_H264
                    if (s_h264Active) {
                        static_cast<H264Streamer*>(streamer)->requestIDR();
                    }
                #endif
            } else {
                Serial.println("[FATAL] Streamer init failed. Closing client.");
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
            Serial.printf("[INFO] RTSP client disconnected (heap: %u)\n", ESP.getFreeHeap());
            delete session;
            session = nullptr;
        }
    }
}
