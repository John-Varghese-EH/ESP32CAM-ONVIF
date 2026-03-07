#include "rtsp_server.h"
#include "config.h"
#include "board_config.h"
#include "status_led.h"

WiFiServer rtspServer(RTSP_PORT);
CRtspSession *session = nullptr;

// Conditionally define the streamer type.
// In H.264 mode we use CStreamer* so that a MyStreamer can be substituted
// if H.264 encoder initialisation fails (MJPEG fallback).
#ifdef VIDEO_CODEC_H264
    CStreamer *streamer = nullptr;
    static bool s_h264Active = false; // true when the live streamer is H264Streamer
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

// Returns the H.264 encoder resolution appropriate for this build target.
// Extracted to keep init and reinit paths consistent.
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
    // The camera is already initialized in setup() via camera_init().
    // Create the appropriate streamer based on codec configuration.
    
    #ifdef VIDEO_CODEC_H264
        Serial.println("[INFO] Creating H.264 streamer...");
        H264Streamer *h264 = new H264Streamer();
        
        uint16_t width, height;
        getH264Resolution(width, height);
        
        if (!h264->init(width, height)) {
            Serial.println("[ERROR] H.264 encoder init failed! Falling back to MJPEG.");
            delete h264;
            // Fall back to MJPEG streamer so the server remains functional
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
    
    // Log board and codec info
    #ifdef BOARD_NAME
        Serial.printf("[INFO] Board: %s, Codec: %s\n", BOARD_NAME, getCodecName());
    #endif
}

void rtsp_server_loop() {
    // If we have an active session, handle it
    if (session) {
        session->handleRequests(0); // 0 timeout means non-blocking
        
        // Broadcast video frame
        // Frame rate limiting based on codec
        static uint32_t lastFrameTime = 0;
        uint32_t now = millis();
        
        #ifdef VIDEO_CODEC_H264
            // H.264: Use configured FPS
            uint32_t frameInterval = 1000 / H264_FPS;
        #else
            // MJPEG: ~20 FPS (50ms interval)
            uint32_t frameInterval = 50;
        #endif
        
        if (now - lastFrameTime > frameInterval) { 
            if (session && !session->m_stopped) { 
                session->broadcastCurrentFrame(now);
            }
            lastFrameTime = now;
        }

        // Check if the client has disconnected
        if (session->m_stopped) {
            Serial.println("[INFO] RTSP client disconnected.");
            delete session;
            session = nullptr;
        }
    } else {
        // No active session, check for new clients
        WiFiClient client = rtspServer.available();
        if (client) {
            // RTSP Crash Fix:
            // CRtspSession stores the SOCKET (WiFiClient*).
            // We MUST allocate it on heap to survive this scope.
            WiFiClient *clientPtr = new WiFiClient(client);
            
            // Ensure streamer is valid
            if (!streamer) {
                Serial.println("[ERROR] Streamer is NULL! Attempting re-init.");
                #ifdef VIDEO_CODEC_H264
                    if (s_h264Active) {
                        // Retry H.264 init with the same resolution used at startup
                        uint16_t width, height;
                        getH264Resolution(width, height);
                        H264Streamer *h264 = new H264Streamer();
                        if (!h264->init(width, height)) {
                            Serial.println("[ERROR] H.264 reinit failed. Falling back to MJPEG.");
                            delete h264;
                            streamer = new MyStreamer();
                            s_h264Active = false;
                            Serial.println("[INFO] Streamer switched to MJPEG (H.264 unavailable).");
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
                // Set client socket for RTP-over-TCP
                streamer->setClientSocket(clientPtr);
                
                session = new CRtspSession(clientPtr, streamer);
                session->m_ClientPtr = clientPtr;  // Store pointer for cleanup
                Serial.printf("[INFO] RTSP Client Connected (%s stream)\n", getCodecName());
                
                #ifdef VIDEO_CODEC_H264
                    // Request IDR frame for new client (only if H.264 streamer is active)
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
}
