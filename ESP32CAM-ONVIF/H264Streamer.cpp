// ==============================================================================
//   H.264 RTP Streamer Implementation
// ==============================================================================
// Implements H.264 video streaming over RTP per RFC 6184.
// Key features:
//   - NAL unit packetization (Single NAL, FU-A fragmentation)
//   - SPS/PPS extraction for SDP
//   - Integration with esp_h264 encoder
// ==============================================================================

#include "H264Streamer.h"
#include "config.h"
#include "board_config.h"

#ifdef VIDEO_CODEC_H264
#ifdef H264_CAPABLE

#include <Arduino.h>
#include "esp_camera.h"
#include "h264_encoder.h"

// RTP Header size
#define RTP_HEADER_SIZE 12

// Maximum RTP payload size (MTU - IP/UDP headers)
#define MAX_RTP_PAYLOAD 1400

// NAL unit types
#define NAL_TYPE_SLICE    1
#define NAL_TYPE_IDR      5
#define NAL_TYPE_SEI      6
#define NAL_TYPE_SPS      7
#define NAL_TYPE_PPS      8
#define NAL_TYPE_FU_A    28

H264Streamer::H264Streamer() 
    : CStreamer(NULL, 640, 480), 
      m_initialized(false),
      m_spsSize(0),
      m_ppsSize(0),
      m_spsPpsValid(false) {
    
    memset(&m_config, 0, sizeof(m_config));
    memset(m_sps, 0, sizeof(m_sps));
    memset(m_pps, 0, sizeof(m_pps));
}

H264Streamer::~H264Streamer() {
    if (m_initialized) {
        h264_encoder_destroy();
    }
}

bool H264Streamer::init(uint16_t width, uint16_t height) {
    Serial.printf("[INFO] H264Streamer: Initializing %dx%d\n", width, height);
    
    // Configure encoder
    m_config.width = width;
    m_config.height = height;
    m_config.fps = H264_FPS;
    m_config.bitrate = H264_BITRATE;
    m_config.gop = H264_GOP;
    m_config.qp_min = H264_QP_MIN;
    m_config.qp_max = H264_QP_MAX;
    
    // Check software encoder resolution limits
    #ifndef H264_HW_ENCODER
    if (width > H264_SW_MAX_WIDTH || height > H264_SW_MAX_HEIGHT) {
        Serial.printf("[WARN] H264Streamer: Resolution exceeds SW limits, clamping to %dx%d\n",
                      H264_SW_MAX_WIDTH, H264_SW_MAX_HEIGHT);
        m_config.width = H264_SW_MAX_WIDTH;
        m_config.height = H264_SW_MAX_HEIGHT;
    }
    #endif
    
    // Initialize the encoder
    h264_status_t status = h264_encoder_init(&m_config);
    if (status != H264_OK) {
        Serial.printf("[ERROR] H264Streamer: Encoder init failed (status=%d)\n", status);
        return false;
    }
    
    m_initialized = true;
    Serial.printf("[INFO] H264Streamer: Initialized with %s encoder\n", 
                  h264_encoder_get_type_string());
    
    return true;
}

void H264Streamer::streamImage(uint32_t curMsec) {
    if (!m_initialized) {
        Serial.println("[ERROR] H264Streamer: Not initialized");
        return;
    }
    
    // Get camera frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[ERROR] H264Streamer: Failed to get camera frame");
        return;
    }
    
    h264_frame_t encoded_frame;
    h264_status_t status;
    
    // Check frame format
    if (fb->format == PIXFORMAT_JPEG) {
        // Need to decode JPEG first - not ideal for performance
        // For best H.264 performance, configure camera for YUV output
        status = h264_encoder_encode_jpeg(fb->buf, fb->len, &encoded_frame);
    } else {
        // Raw YUV - direct encoding (preferred)
        status = h264_encoder_encode(fb->buf, fb->len, &encoded_frame);
    }
    
    esp_camera_fb_return(fb);
    
    if (status != H264_OK) {
        if (status != H264_ERR_NOT_SUPPORTED) {
            Serial.printf("[ERROR] H264Streamer: Encoding failed (status=%d)\n", status);
        }
        return;
    }
    
    // Extract SPS/PPS if this is an IDR frame
    if (encoded_frame.contains_sps_pps) {
        extractSPSPPS(encoded_frame.data, encoded_frame.size);
    }
    
    // Calculate RTP timestamp (90kHz clock)
    static uint32_t lastMsec = 0;
    static uint32_t rtpTimestamp = 0;
    
    if (lastMsec == 0) {
        lastMsec = curMsec;
    }
    
    uint32_t deltaMsec = curMsec - lastMsec;
    rtpTimestamp += (deltaMsec * 90); // 90kHz = 90 ticks per ms
    lastMsec = curMsec;
    
    // Parse and send NAL units
    size_t offset = 0;
    while (offset < encoded_frame.size) {
        // Find NAL unit start code (0x00 0x00 0x00 0x01 or 0x00 0x00 0x01)
        int nalStart = findNextNALUnit(encoded_frame.data, encoded_frame.size, offset);
        if (nalStart < 0) break;
        
        // Find end of this NAL unit (start of next, or end of buffer)
        int nalEnd = findNextNALUnit(encoded_frame.data, encoded_frame.size, nalStart + 4);
        if (nalEnd < 0) {
            nalEnd = encoded_frame.size;
        }
        
        size_t nalSize = nalEnd - nalStart;
        bool isLast = (nalEnd >= (int)encoded_frame.size);
        
        // Send the NAL unit
        sendNALUnit(encoded_frame.data + nalStart, nalSize, isLast, rtpTimestamp);
        
        offset = nalEnd;
    }
}

int H264Streamer::findNextNALUnit(const uint8_t* data, size_t size, size_t offset) {
    // Look for start code: 0x00 0x00 0x01 or 0x00 0x00 0x00 0x01
    for (size_t i = offset; i < size - 3; i++) {
        if (data[i] == 0x00 && data[i+1] == 0x00) {
            if (data[i+2] == 0x01) {
                return i + 3; // 3-byte start code
            }
            if (data[i+2] == 0x00 && i < size - 4 && data[i+3] == 0x01) {
                return i + 4; // 4-byte start code
            }
        }
    }
    return -1;
}

void H264Streamer::sendNALUnit(const uint8_t* nalData, size_t nalSize, bool isLast, uint32_t timestamp) {
    if (nalSize == 0) return;
    
    uint8_t nalType = nalData[0] & 0x1F;
    
    // Cache SPS/PPS for SDP
    if (nalType == NAL_TYPE_SPS && nalSize <= sizeof(m_sps)) {
        memcpy(m_sps, nalData, nalSize);
        m_spsSize = nalSize;
        m_spsPpsValid = true;
    } else if (nalType == NAL_TYPE_PPS && nalSize <= sizeof(m_pps)) {
        memcpy(m_pps, nalData, nalSize);
        m_ppsSize = nalSize;
    }
    
    // Single NAL unit mode (small NAL fits in one packet)
    if (nalSize <= MAX_RTP_PAYLOAD) {
        sendH264RtpPacket(nalData, nalSize, isLast, timestamp);
        return;
    }
    
    // FU-A Fragmentation mode (NAL too large for single packet)
    // Fragment the NAL unit header is removed and replaced with FU indicator + FU header
    uint8_t fuIndicator = (nalData[0] & 0xE0) | NAL_TYPE_FU_A;  // F, NRI from original, type = 28
    uint8_t nalTypeOriginal = nalData[0] & 0x1F;
    
    const uint8_t* payloadData = nalData + 1;  // Skip original NAL header
    size_t payloadRemaining = nalSize - 1;
    bool isFirst = true;
    
    while (payloadRemaining > 0) {
        size_t chunkSize = (payloadRemaining > MAX_RTP_PAYLOAD - 2) ? 
                           (MAX_RTP_PAYLOAD - 2) : payloadRemaining;
        bool isEnd = (payloadRemaining <= MAX_RTP_PAYLOAD - 2);
        
        // Build FU-A packet
        uint8_t fuPacket[MAX_RTP_PAYLOAD];
        fuPacket[0] = fuIndicator;
        fuPacket[1] = nalTypeOriginal;
        
        if (isFirst) fuPacket[1] |= 0x80;  // Start bit
        if (isEnd)   fuPacket[1] |= 0x40;  // End bit
        
        memcpy(fuPacket + 2, payloadData, chunkSize);
        
        bool marker = isEnd && isLast;
        sendH264RtpPacket(fuPacket, chunkSize + 2, marker, timestamp);
        
        payloadData += chunkSize;
        payloadRemaining -= chunkSize;
        isFirst = false;
        
        // Small delay to prevent overwhelming WiFi buffer
        yield();
        delayMicroseconds(200);
    }
}

void H264Streamer::sendH264RtpPacket(const uint8_t* data, size_t size, bool marker, uint32_t timestamp) {
    // Buffer for RTP packet (interleaved header + RTP header + payload)
    static uint8_t rtpBuf[1600];
    static uint16_t sequenceNumber = 0;
    
    size_t rtpPacketSize = RTP_HEADER_SIZE + size;
    
    // RTP-over-RTSP interleaved header (4 bytes)
    rtpBuf[0] = '$';        // Magic
    rtpBuf[1] = 0;          // Channel (RTP)
    rtpBuf[2] = (rtpPacketSize >> 8) & 0xFF;
    rtpBuf[3] = rtpPacketSize & 0xFF;
    
    // RTP Header (12 bytes)
    rtpBuf[4] = 0x80;       // V=2, P=0, X=0, CC=0
    rtpBuf[5] = 96;         // PT=96 (dynamic H.264)
    if (marker) rtpBuf[5] |= 0x80;  // Marker bit
    
    // Sequence number (big endian)
    rtpBuf[6] = (sequenceNumber >> 8) & 0xFF;
    rtpBuf[7] = sequenceNumber & 0xFF;
    sequenceNumber++;
    
    // Timestamp (big endian)
    rtpBuf[8]  = (timestamp >> 24) & 0xFF;
    rtpBuf[9]  = (timestamp >> 16) & 0xFF;
    rtpBuf[10] = (timestamp >> 8) & 0xFF;
    rtpBuf[11] = timestamp & 0xFF;
    
    // SSRC (fixed)
    rtpBuf[12] = 0x13;
    rtpBuf[13] = 0xF9;
    rtpBuf[14] = 0x7E;
    rtpBuf[15] = 0x68;
    
    // Copy payload
    memcpy(rtpBuf + 4 + RTP_HEADER_SIZE, data, size);
    
    // Send via TCP (RTP-over-RTSP)
    if (m_TCPTransport && m_Client) {
        socketsend(m_Client, (char*)rtpBuf, rtpPacketSize + 4);
    }
    // Note: UDP path would need additional implementation
}

void H264Streamer::extractSPSPPS(const uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset < size - 4) {
        int nalStart = findNextNALUnit(data, size, offset);
        if (nalStart < 0) break;
        
        int nalEnd = findNextNALUnit(data, size, nalStart);
        if (nalEnd < 0) nalEnd = size;
        
        uint8_t nalType = data[nalStart] & 0x1F;
        size_t nalSize = nalEnd - nalStart;
        
        if (nalType == NAL_TYPE_SPS && nalSize <= sizeof(m_sps)) {
            memcpy(m_sps, data + nalStart, nalSize);
            m_spsSize = nalSize;
        } else if (nalType == NAL_TYPE_PPS && nalSize <= sizeof(m_pps)) {
            memcpy(m_pps, data + nalStart, nalSize);
            m_ppsSize = nalSize;
        }
        
        offset = nalEnd;
    }
    
    if (m_spsSize > 0 && m_ppsSize > 0) {
        m_spsPpsValid = true;
    }
}

bool H264Streamer::getSPS(uint8_t* buffer, size_t* size) {
    if (!m_spsPpsValid || m_spsSize == 0) return false;
    
    size_t copySize = (*size < m_spsSize) ? *size : m_spsSize;
    memcpy(buffer, m_sps, copySize);
    *size = m_spsSize;
    return true;
}

bool H264Streamer::getPPS(uint8_t* buffer, size_t* size) {
    if (!m_spsPpsValid || m_ppsSize == 0) return false;
    
    size_t copySize = (*size < m_ppsSize) ? *size : m_ppsSize;
    memcpy(buffer, m_pps, copySize);
    *size = m_ppsSize;
    return true;
}

void H264Streamer::requestIDR() {
    h264_encoder_request_idr();
}

bool H264Streamer::isHardwareEncoder() const {
    return h264_encoder_is_hw();
}

#endif // H264_CAPABLE
#endif // VIDEO_CODEC_H264
