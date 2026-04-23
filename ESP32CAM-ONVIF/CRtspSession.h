#pragma once

#include "CStreamer.h"
#include "platglue.h"

// supported command types
enum RTSP_CMD_TYPES
{
    RTSP_OPTIONS,
    RTSP_DESCRIBE,
    RTSP_SETUP,
    RTSP_PLAY,
    RTSP_TEARDOWN,
    RTSP_GET_PARAMETER,
    RTSP_UNKNOWN
};

// Buffer sizes — tuned for ESP32 SRAM constraints.
// RTSP requests are typically <500 bytes; 2KB is generous.
#define RTSP_BUFFER_SIZE       2048
#define RTSP_PARAM_STRING_MAX  200
#define MAX_HOSTNAME_LEN       256

class CRtspSession
{
public:
    CRtspSession(SOCKET aRtspClient, CStreamer * aStreamer);
    ~CRtspSession();

    RTSP_CMD_TYPES Handle_RtspRequest(char const * aRequest, unsigned aRequestSize);
    int            GetStreamID();

    /**
       Read from our socket, parsing commands as possible.
       return false if the read timed out
     */
    bool handleRequests(uint32_t readTimeoutMs);

    /**
       broadcast a current frame
     */
    void broadcastCurrentFrame(uint32_t curMsec);

    bool m_streaming;
    bool m_stopped;

private:
    void Init();
    bool ParseRtspRequest(char const * aRequest, unsigned aRequestSize);
    char const * DateHeader();

    // RTSP request command handlers
    void Handle_RtspOPTION();
    void Handle_RtspDESCRIBE();
    void Handle_RtspSETUP();
    void Handle_RtspPLAY();
    void Handle_RtspGET_PARAMETER();

    // Session state
    int m_RtspSessionID;
    SOCKET m_RtspClient;                                      // RTSP socket (WiFiClient*)
    int m_StreamID;                                           // stream index
    IPPORT m_ClientRTPPort;                                   // client RTP port (UDP)
    IPPORT m_ClientRTCPPort;                                  // client RTCP port (UDP)
    bool m_TcpTransport;                                      // true = RTP-over-TCP
    CStreamer * m_Streamer;                                    // media streamer

    // Last parsed RTSP request fields
    RTSP_CMD_TYPES m_RtspCmdType;
    char m_URLPreSuffix[RTSP_PARAM_STRING_MAX];
    char m_URLSuffix[RTSP_PARAM_STRING_MAX];
    char m_CSeq[RTSP_PARAM_STRING_MAX];
    char m_URLHostPort[MAX_HOSTNAME_LEN];
    unsigned m_ContentLength;
};
