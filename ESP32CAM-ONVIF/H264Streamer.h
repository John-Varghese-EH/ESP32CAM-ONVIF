#pragma once
// ==============================================================================
//   H.264 RTP Streamer
// ==============================================================================
// Streams H.264 encoded video over RTP following RFC 6184.
// Supports NAL unit fragmentation (FU-A) for large frames.
// ==============================================================================

#include "config.h"
#include "board_config.h"
#include "CStreamer.h"
#include "h264_encoder.h"

#ifdef VIDEO_CODEC_H264

class H264Streamer : public CStreamer {
public:
    H264Streamer();
    virtual ~H264Streamer();
    
    // Initialize the H.264 encoder with default settings
    bool init(uint16_t width = 640, uint16_t height = 480);
    
    // Stream a new frame (overrides base class)
    virtual void streamImage(uint32_t curMsec) override;
    
    // Get SPS for SDP generation
    bool getSPS(uint8_t* buffer, size_t* size);
    
    // Get PPS for SDP generation  
    bool getPPS(uint8_t* buffer, size_t* size);
    
    // Request IDR frame (keyframe)
    void requestIDR();
    
    // Check if using hardware encoder
    bool isHardwareEncoder() const;
    
private:
    // Send a single NAL unit (handles fragmentation if needed)
    void sendNALUnit(const uint8_t* nalData, size_t nalSize, bool isLast, uint32_t timestamp);
    
    // Send RTP packet for H.264
    void sendH264RtpPacket(const uint8_t* data, size_t size, bool marker, uint32_t timestamp);
    
    // Parse NAL units from encoded frame
    int findNextNALUnit(const uint8_t* data, size_t size, size_t offset);
    
    bool m_initialized;
    h264_encoder_config_t m_config;
    
    // SPS/PPS cache for SDP
    uint8_t m_sps[64];
    size_t m_spsSize;
    uint8_t m_pps[64];
    size_t m_ppsSize;
    bool m_spsPpsValid;
};

#else // VIDEO_CODEC_H264 not defined

// Stub class when H.264 is disabled - just use MJPEG
#include "MyStreamer.h"
typedef MyStreamer H264Streamer;

#endif // VIDEO_CODEC_H264
