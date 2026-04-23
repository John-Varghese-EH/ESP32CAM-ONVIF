#include "CRtspSession.h"
#include <stdio.h>
#include <time.h>
#include "esp_camera.h"

// Shared RTSP response buffers — single-threaded, no concurrency risk.
static char s_RtspResponse[1024];
static char s_RtspSDP[1024];
static char s_RtspURL[256];

CRtspSession::CRtspSession(SOCKET aRtspClient, CStreamer * aStreamer)
    : m_RtspClient(aRtspClient), m_Streamer(aStreamer)
{
    printf("Creating RTSP session\n");
    Init();

    m_RtspSessionID  = getRandom();
    m_RtspSessionID |= 0x80000000;
    m_StreamID       = -1;
    m_ClientRTPPort  =  0;
    m_ClientRTCPPort =  0;
    m_TcpTransport   =  false;
    m_streaming = false;
    m_stopped = false;
};

CRtspSession::~CRtspSession()
{
    if (m_RtspClient) {
        m_RtspClient->stop();
        delete m_RtspClient;
        m_RtspClient = nullptr;
    }
};

void CRtspSession::Init()
{
    m_RtspCmdType   = RTSP_UNKNOWN;
    memset(m_URLPreSuffix, 0x00, sizeof(m_URLPreSuffix));
    memset(m_URLSuffix,    0x00, sizeof(m_URLSuffix));
    memset(m_CSeq,         0x00, sizeof(m_CSeq));
    memset(m_URLHostPort,  0x00, sizeof(m_URLHostPort));
    m_ContentLength  =  0;
};

bool CRtspSession::ParseRtspRequest(char const * aRequest, unsigned aRequestSize)
{
    // NOTE: We parse directly from aRequest (which is the RecvBuf in handleRequests).
    // Previously a separate static CurRequest[RTSP_BUFFER_SIZE] was used as a copy,
    // wasting 2KB of BSS. Eliminated by making the receive buffer mutable.
    char * CurRequest = (char *)aRequest;  // Cast away const — we own this buffer
    unsigned CurRequestSize = aRequestSize;

    Init();

    // Extract client_port from SETUP request
    char * ClientPortPtr = strstr(CurRequest, "client_port");
    if (ClientPortPtr != nullptr)
    {
        char * TmpPtr = strstr(ClientPortPtr, "\r\n");
        if (TmpPtr != nullptr)
        {
            char saved = TmpPtr[0];
            TmpPtr[0] = '\0';
            char * pEq = strstr(ClientPortPtr, "=");
            if (pEq != nullptr)
            {
                pEq++;
                m_ClientRTPPort = atoi(pEq);
                m_ClientRTCPPort = m_ClientRTPPort + 1;
            }
            TmpPtr[0] = saved;  // Restore for further parsing
        }
    }

    // Read command name (up to first space)
    char CmdName[RTSP_PARAM_STRING_MAX];
    bool parseSucceeded = false;
    unsigned i;
    for (i = 0; i < sizeof(CmdName)-1 && i < CurRequestSize; ++i)
    {
        char c = CurRequest[i];
        if (c == ' ' || c == '\t')
        {
            parseSucceeded = true;
            break;
        }
        CmdName[i] = c;
    }
    CmdName[i] = '\0';
    if (!parseSucceeded) return false;

    printf("RTSP received %s\n", CmdName);

    // Identify command type — use strcmp for exact match (faster, no false substring hits)
    if      (strcmp(CmdName, "OPTIONS") == 0)       m_RtspCmdType = RTSP_OPTIONS;
    else if (strcmp(CmdName, "DESCRIBE") == 0)      m_RtspCmdType = RTSP_DESCRIBE;
    else if (strcmp(CmdName, "SETUP") == 0)         m_RtspCmdType = RTSP_SETUP;
    else if (strcmp(CmdName, "PLAY") == 0)          m_RtspCmdType = RTSP_PLAY;
    else if (strcmp(CmdName, "TEARDOWN") == 0)      m_RtspCmdType = RTSP_TEARDOWN;
    else if (strcmp(CmdName, "GET_PARAMETER") == 0) m_RtspCmdType = RTSP_GET_PARAMETER;

    // Check transport type for SETUP
    if (m_RtspCmdType == RTSP_SETUP)
    {
        m_TcpTransport = (strstr(CurRequest, "RTP/AVP/TCP") != nullptr);
    }

    // Skip over the "rtsp://" URL prefix
    unsigned j = i+1;
    while (j < CurRequestSize && (CurRequest[j] == ' ' || CurRequest[j] == '\t')) ++j;
    for (; (int)j < (int)(CurRequestSize-8); ++j)
    {
        if ((CurRequest[j]   == 'r' || CurRequest[j]   == 'R') &&
            (CurRequest[j+1] == 't' || CurRequest[j+1] == 'T') &&
            (CurRequest[j+2] == 's' || CurRequest[j+2] == 'S') &&
            (CurRequest[j+3] == 'p' || CurRequest[j+3] == 'P') &&
            CurRequest[j+4] == ':' && CurRequest[j+5] == '/')
        {
            j += 6;
            if (CurRequest[j] == '/')
            {
                ++j;
                unsigned uidx = 0;
                while (j < CurRequestSize && CurRequest[j] != '/' && CurRequest[j] != ' ' && uidx < sizeof(m_URLHostPort) - 1)
                {
                    m_URLHostPort[uidx++] = CurRequest[j++];
                }
            }
            else --j;
            i = j;
            break;
        }
    }

    // Extract URL suffix (before "RTSP/")
    parseSucceeded = false;
    for (unsigned k = i+1; (int)k < (int)(CurRequestSize-5); ++k)
    {
        if (CurRequest[k]   == 'R' && CurRequest[k+1] == 'T' &&
            CurRequest[k+2] == 'S' && CurRequest[k+3] == 'P' &&
            CurRequest[k+4] == '/')
        {
            while (--k >= i && CurRequest[k] == ' ') {}
            unsigned k1 = k;
            while (k1 > i && CurRequest[k1] != '/') --k1;
            if (k - k1 + 1 > sizeof(m_URLSuffix)) return false;
            unsigned n = 0, k2 = k1+1;
            while (k2 <= k) m_URLSuffix[n++] = CurRequest[k2++];
            m_URLSuffix[n] = '\0';

            if (k1 - i > sizeof(m_URLPreSuffix)) return false;
            n = 0; k2 = i + 1;
            while (k2 <= k1 - 1) m_URLPreSuffix[n++] = CurRequest[k2++];
            m_URLPreSuffix[n] = '\0';
            i = k + 7;
            parseSucceeded = true;
            break;
        }
    }
    if (!parseSucceeded) return false;

    // Extract CSeq header
    parseSucceeded = false;
    for (j = i; (int)j < (int)(CurRequestSize-5); ++j)
    {
        if (CurRequest[j]   == 'C' && CurRequest[j+1] == 'S' &&
            CurRequest[j+2] == 'e' && CurRequest[j+3] == 'q' &&
            CurRequest[j+4] == ':')
        {
            j += 5;
            while (j < CurRequestSize && (CurRequest[j] == ' ' || CurRequest[j] == '\t')) ++j;
            unsigned n;
            for (n = 0; n < sizeof(m_CSeq)-1 && j < CurRequestSize; ++n, ++j)
            {
                char c = CurRequest[j];
                if (c == '\r' || c == '\n')
                {
                    parseSucceeded = true;
                    break;
                }
                m_CSeq[n] = c;
            }
            m_CSeq[n] = '\0';
            break;
        }
    }
    if (!parseSucceeded) return false;

    // Extract Content-Length (optional)
    for (j = i; (int)j < (int)(CurRequestSize-15); ++j)
    {
        if (CurRequest[j]    == 'C'  && CurRequest[j+1]  == 'o'  &&
            CurRequest[j+2]  == 'n'  && CurRequest[j+3]  == 't'  &&
            CurRequest[j+4]  == 'e'  && CurRequest[j+5]  == 'n'  &&
            CurRequest[j+6]  == 't'  && CurRequest[j+7]  == '-'  &&
            (CurRequest[j+8] == 'L' || CurRequest[j+8]   == 'l') &&
            CurRequest[j+9]  == 'e'  && CurRequest[j+10] == 'n'  &&
            CurRequest[j+11] == 'g'  && CurRequest[j+12] == 't'  &&
            CurRequest[j+13] == 'h'  && CurRequest[j+14] == ':')
        {
            j += 15;
            while (j < CurRequestSize && (CurRequest[j] == ' ' || CurRequest[j] == '\t')) ++j;
            unsigned num;
            if (sscanf(&CurRequest[j], "%u", &num) == 1) m_ContentLength = num;
        }
    }
    return true;
};

RTSP_CMD_TYPES CRtspSession::Handle_RtspRequest(char const * aRequest, unsigned aRequestSize)
{
    if (ParseRtspRequest(aRequest, aRequestSize))
    {
        switch (m_RtspCmdType)
        {
        case RTSP_OPTIONS:       Handle_RtspOPTION();        break;
        case RTSP_DESCRIBE:      Handle_RtspDESCRIBE();      break;
        case RTSP_SETUP:         Handle_RtspSETUP();         break;
        case RTSP_PLAY:          Handle_RtspPLAY();          break;
        case RTSP_GET_PARAMETER: Handle_RtspGET_PARAMETER(); break;
        default: break;
        }
    }
    return m_RtspCmdType;
};

void CRtspSession::Handle_RtspOPTION()
{
    snprintf(s_RtspResponse, sizeof(s_RtspResponse),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, GET_PARAMETER\r\n\r\n", m_CSeq);
    socketsend(m_RtspClient, s_RtspResponse, strlen(s_RtspResponse));
}

void CRtspSession::Handle_RtspDESCRIBE()
{
    m_StreamID = -1;

    // Route stream paths
    if      (strcmp(m_URLPreSuffix, "mjpeg") == 0 && strcmp(m_URLSuffix, "1") == 0) m_StreamID = 0;
    else if (strcmp(m_URLPreSuffix, "mjpeg") == 0 && strcmp(m_URLSuffix, "2") == 0) m_StreamID = 1;
    else if (strcmp(m_URLPreSuffix, "h264")  == 0 && strcmp(m_URLSuffix, "1") == 0) m_StreamID = 2;
    else if (strcmp(m_URLPreSuffix, "h264")  == 0 && strcmp(m_URLSuffix, "2") == 0) m_StreamID = 3;
    else if (strlen(m_URLPreSuffix) == 0 && strcmp(m_URLSuffix, "1") == 0) m_StreamID = 0;
    else if (strlen(m_URLPreSuffix) == 0 && strcmp(m_URLSuffix, "2") == 0) m_StreamID = 1;

    if (m_StreamID == -1)
    {
        snprintf(s_RtspResponse, sizeof(s_RtspResponse),
                 "RTSP/1.0 404 Stream Not Found\r\nCSeq: %s\r\n%s\r\n",
                 m_CSeq, DateHeader());
        socketsend(m_RtspClient, s_RtspResponse, strlen(s_RtspResponse));
        return;
    }

    // Extract host IP from URL (strip port)
    static char OBuf[256];
    strcpy(OBuf, m_URLHostPort);
    char * ColonPtr = strstr(OBuf, ":");
    if (ColonPtr != nullptr) ColonPtr[0] = '\0';

    // Determine codec
    bool useH264 = (m_StreamID >= 2);

    if (useH264) {
        snprintf(s_RtspSDP, sizeof(s_RtspSDP),
                 "v=0\r\n"
                 "o=- %d 1 IN IP4 %s\r\n"
                 "s=ESP32-CAM H.264 Stream\r\n"
                 "t=0 0\r\n"
                 "a=tool:ESP32-CAM RTSP Server\r\n"
                 "a=type:broadcast\r\n"
                 "a=control:*\r\n"
                 "a=range:npt=0-\r\n"
                 "m=video 0 RTP/AVP 96\r\n"
                 "c=IN IP4 0.0.0.0\r\n"
                 "b=AS:2000\r\n"
                 "a=rtpmap:96 H264/90000\r\n"
                 "a=fmtp:96 packetization-mode=1;profile-level-id=42E01F\r\n"
                 "a=framerate:25\r\n"
                 "a=control:track1\r\n",
                 rand(), OBuf);
    } else {
        snprintf(s_RtspSDP, sizeof(s_RtspSDP),
                 "v=0\r\n"
                 "o=- %d 1 IN IP4 %s\r\n"
                 "s=ESP32-CAM RTSP Stream\r\n"
                 "t=0 0\r\n"
                 "a=tool:ESP32-CAM RTSP Server\r\n"
                 "a=type:broadcast\r\n"
                 "a=control:*\r\n"
                 "a=range:npt=0-\r\n"
                 "m=video 0 RTP/AVP 26\r\n"
                 "c=IN IP4 0.0.0.0\r\n"
                 "b=AS:4096\r\n"
                 "a=rtpmap:26 JPEG/90000\r\n"
                 "a=fmtp:26 width=640;height=480;quality=10\r\n"
                 "a=framerate:20\r\n"
                 "a=control:track1\r\n",
                 rand(), OBuf);
    }

    // Build stream name for Content-Base
    char StreamName[16];
    switch (m_StreamID) {
    case 0: strcpy(StreamName, "mjpeg/1"); break;
    case 1: strcpy(StreamName, "mjpeg/2"); break;
    case 2: strcpy(StreamName, "h264/1");  break;
    case 3: strcpy(StreamName, "h264/2");  break;
    default: strcpy(StreamName, "1");      break;
    }
    snprintf(s_RtspURL, sizeof(s_RtspURL), "rtsp://%s/%s", m_URLHostPort, StreamName);
    snprintf(s_RtspResponse, sizeof(s_RtspResponse),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Content-Base: %s/\r\n"
             "Content-Type: application/sdp\r\n"
             "Content-Length: %d\r\n\r\n"
             "%s",
             m_CSeq, DateHeader(), s_RtspURL, (int)strlen(s_RtspSDP), s_RtspSDP);
    socketsend(m_RtspClient, s_RtspResponse, strlen(s_RtspResponse));
}

void CRtspSession::Handle_RtspSETUP()
{
    if (!m_Streamer) {
        printf("[ERROR] m_Streamer is null in SETUP\n");
        return;
    }

    m_Streamer->InitTransport(m_ClientRTPPort, m_ClientRTCPPort, m_TcpTransport);

    static char Transport[255];
    if (m_TcpTransport)
        snprintf(Transport, sizeof(Transport), "RTP/AVP/TCP;unicast;interleaved=0-1");
    else
        snprintf(Transport, sizeof(Transport),
                 "RTP/AVP;unicast;destination=127.0.0.1;source=127.0.0.1;client_port=%i-%i;server_port=%i-%i",
                 m_ClientRTPPort, m_ClientRTCPPort,
                 m_Streamer->GetRtpServerPort(),
                 m_Streamer->GetRtcpServerPort());

    snprintf(s_RtspResponse, sizeof(s_RtspResponse),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Transport: %s\r\n"
             "Session: %i;timeout=60\r\n\r\n",
             m_CSeq, DateHeader(), Transport, m_RtspSessionID);
    socketsend(m_RtspClient, s_RtspResponse, strlen(s_RtspResponse));
}

void CRtspSession::Handle_RtspPLAY()
{
    char StreamPath[16];
    switch (m_StreamID) {
    case 0: strcpy(StreamPath, "mjpeg/1"); break;
    case 1: strcpy(StreamPath, "mjpeg/2"); break;
    case 2: strcpy(StreamPath, "h264/1");  break;
    case 3: strcpy(StreamPath, "h264/2");  break;
    default: strcpy(StreamPath, "mjpeg/1"); break;
    }

    snprintf(s_RtspResponse, sizeof(s_RtspResponse),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Range: npt=0.000-\r\n"
             "Session: %i;timeout=60\r\n"
             "RTP-Info: url=rtsp://%s/%s/track1;seq=0;rtptime=0\r\n\r\n",
             m_CSeq, DateHeader(), m_RtspSessionID, m_URLHostPort, StreamPath);
    socketsend(m_RtspClient, s_RtspResponse, strlen(s_RtspResponse));
}

char const * CRtspSession::DateHeader()
{
    static char buf[200];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

int CRtspSession::GetStreamID()
{
    return m_StreamID;
};

void CRtspSession::Handle_RtspGET_PARAMETER()
{
    snprintf(s_RtspResponse, sizeof(s_RtspResponse),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "Session: %i\r\n\r\n",
             m_CSeq, m_RtspSessionID);
    socketsend(m_RtspClient, s_RtspResponse, strlen(s_RtspResponse));
}

bool CRtspSession::handleRequests(uint32_t readTimeoutMs)
{
    if (m_stopped)
        return false;

    // Single receive buffer — ParseRtspRequest now operates on this directly
    // (no more redundant memcpy to a separate CurRequest buffer)
    static char RecvBuf[RTSP_BUFFER_SIZE];

    memset(RecvBuf, 0x00, sizeof(RecvBuf));
    int res = socketread(m_RtspClient, RecvBuf, sizeof(RecvBuf), readTimeoutMs);
    if (res > 0) {
        // Filter: valid RTSP commands start with O, D, S, P, T, or G
        char first = RecvBuf[0];
        if (first == 'O' || first == 'D' || first == 'S' || first == 'P' || first == 'T' || first == 'G')
        {
            RTSP_CMD_TYPES C = Handle_RtspRequest(RecvBuf, res);
            if (C == RTSP_PLAY)
                m_streaming = true;
            else if (C == RTSP_TEARDOWN)
                m_stopped = true;
        }
        return true;
    }
    else if (res == 0) {
        m_stopped = true;
        return true;
    }
    else {
        // Timeout
        return false;
    }
}

void CRtspSession::broadcastCurrentFrame(uint32_t curMsec) {
    if (m_streaming && !m_stopped && m_Streamer) {
        m_Streamer->streamImage(curMsec);
    }
}
