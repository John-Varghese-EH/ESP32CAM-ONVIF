#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "board_config.h"
#include "CRtspSession.h"

// Include both streamers — the H.264 path may fall back to MyStreamer
#ifdef VIDEO_CODEC_H264
    #include "H264Streamer.h"
    #include "MyStreamer.h"
    // NOTE: streamer is CStreamer* (base class) to support MJPEG fallback
    extern CStreamer *streamer;
#else
    #include "MyStreamer.h"
    extern MyStreamer *streamer;
#endif

extern WiFiServer rtspServer;

String getRTSPUrl();
void rtsp_server_start();
void rtsp_server_loop();

const char* getCodecName();
