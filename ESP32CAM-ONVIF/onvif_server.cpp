#include <Arduino.h>
#include "onvif_server.h"
#include "rtsp_server.h"
#include "camera_control.h"
#include <WiFiUdp.h>
#include <WebServer.h>
#include <time.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "config.h"

WebServer onvifServer(ONVIF_PORT);
WiFiUDP onvifUDP;
static bool _onvifEnabled = DEFAULT_ONVIF_ENABLED;

bool onvif_is_enabled() { return _onvifEnabled; }
void onvif_set_enabled(bool en) { _onvifEnabled = en; }

const char PROGMEM PART_HEADER[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" ";
const char PROGMEM PART_BODY[] = "<SOAP-ENV:Body>";
const char PROGMEM PART_END[] = "</SOAP-ENV:Body></SOAP-ENV:Envelope>";



// --- PROGMEM Templates ---
// GetCapabilities Response - Uses tt: namespace for Capabilities content per ONVIF spec
const char PROGMEM TPL_CAPABILITIES[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetCapabilitiesResponse>"
    "<tds:Capabilities>"
    "<tt:Device>"
        "<tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr>"
        "<tt:Network><tt:IPFilter>false</tt:IPFilter><tt:ZeroConfiguration>false</tt:ZeroConfiguration><tt:IPVersion6>false</tt:IPVersion6><tt:DynDNS>false</tt:DynDNS></tt:Network>"
        "<tt:System><tt:DiscoveryResolve>false</tt:DiscoveryResolve><tt:DiscoveryBye>false</tt:DiscoveryBye><tt:RemoteDiscovery>false</tt:RemoteDiscovery><tt:SystemBackup>false</tt:SystemBackup><tt:FirmwareUpgrade>false</tt:FirmwareUpgrade><tt:SupportedVersions><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tt:SupportedVersions></tt:System>"
        "<tt:Security><tt:TLS1.0>false</tt:TLS1.0><tt:TLS1.1>false</tt:TLS1.1><tt:TLS1.2>false</tt:TLS1.2><tt:OnboardKeyGeneration>false</tt:OnboardKeyGeneration><tt:AccessPolicyConfig>false</tt:AccessPolicyConfig><tt:DefaultAccessPolicy>false</tt:DefaultAccessPolicy><tt:Dot1X>false</tt:Dot1X><tt:RemoteUserHandling>false</tt:RemoteUserHandling><tt:X.509Token>false</tt:X.509Token><tt:SAMLToken>false</tt:SAMLToken><tt:KerberosToken>false</tt:KerberosToken><tt:UsernameToken>true</tt:UsernameToken><tt:HttpDigest>false</tt:HttpDigest><tt:RELToken>false</tt:RELToken></tt:Security>"
    "</tt:Device>"
    "<tt:Media>"
        "<tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr>"
        "<tt:StreamingCapabilities><tt:RTPMulticast>false</tt:RTPMulticast><tt:RTP_TCP>true</tt:RTP_TCP><tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP></tt:StreamingCapabilities>"
    "</tt:Media>"
    "<tt:Imaging><tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr></tt:Imaging>"
    "</tds:Capabilities>"
    "</tds:GetCapabilitiesResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_DEV_INFO[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<SOAP-ENV:Body>"
    "<tds:GetDeviceInformationResponse>"
    "<tds:Manufacturer>" DEVICE_MANUFACTURER "</tds:Manufacturer>"
    "<tds:Model>" DEVICE_MODEL "</tds:Model>"
    "<tds:FirmwareVersion>" DEVICE_VERSION "</tds:FirmwareVersion>"
    "<tds:SerialNumber>J0X%s</tds:SerialNumber>"
    "<tds:HardwareId>" DEVICE_HARDWARE_ID "</tds:HardwareId>"
    "</tds:GetDeviceInformationResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_STREAM_URI[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetStreamUriResponse>"
    "<trt:MediaUri>"
    "<tt:Uri>rtsp://%s:%d/mjpeg/1</tt:Uri>"
    "<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
    "<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
    "<tt:Timeout>PT0S</tt:Timeout>"
    "</trt:MediaUri>"
    "</trt:GetStreamUriResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
    
// Template for Dynamic Time
const char PROGMEM TPL_TIME_FMT[] =
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetSystemDateAndTimeResponse>"
        "<tds:SystemDateAndTime>"
            "<tt:DateTimeType>NTP</tt:DateTimeType>"
            "<tt:DaylightSavings>false</tt:DaylightSavings>"
            "<tt:TimeZone><tt:TZ>IST-5:30</tt:TZ></tt:TimeZone>"
            "<tt:UTCDateTime>"
                "<tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>"
                "<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date>"
            "</tt:UTCDateTime>"
        "</tds:SystemDateAndTime>"
    "</tds:GetSystemDateAndTimeResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
    
// Time Sync: Minimal 'Ok' response template for SetSystemDateAndTime
const char PROGMEM TPL_SET_TIME_RES[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<SOAP-ENV:Body>"
    "<tds:SetSystemDateAndTimeResponse/>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

// --- Network Services (Hikvision Requirement) ---
const char PROGMEM TPL_NTP[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetNTPResponse>"
        "<tds:NTPInformation>"
            "<tt:FromDHCP>false</tt:FromDHCP>"
            "<tt:NTPManual><tt:Type>DNS</tt:Type><tt:DNSname>pool.ntp.org</tt:DNSname></tt:NTPManual>"
        "</tds:NTPInformation>"
    "</tds:GetNTPResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_DNS[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetDNSResponse>"
        "<tds:DNSInformation>"
            "<tt:FromDHCP>false</tt:FromDHCP>"
            "<tt:DNSManual><tt:Type>IPv4</tt:Type><tt:IPv4Address>8.8.8.8</tt:IPv4Address></tt:DNSManual>"
        "</tds:DNSInformation>"
    "</tds:GetDNSResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_NET_PROTOCOLS[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetNetworkProtocolsResponse>"
        "<tds:NetworkProtocols><tt:Name>HTTP</tt:Name><tt:Enabled>true</tt:Enabled><tt:Port>80</tt:Port></tds:NetworkProtocols>"
        "<tds:NetworkProtocols><tt:Name>RTSP</tt:Name><tt:Enabled>true</tt:Enabled><tt:Port>554</tt:Port></tds:NetworkProtocols>"
        "<tds:NetworkProtocols><tt:Name>ONVIF</tt:Name><tt:Enabled>true</tt:Enabled><tt:Port>8000</tt:Port></tds:NetworkProtocols>"
    "</tds:GetNetworkProtocolsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

// Helper to base64 decode
int base64_decode(String input, uint8_t *output) {
    size_t olen;
    mbedtls_base64_decode(output, 64, &olen, (const unsigned char *)input.c_str(), input.length());
    return olen;
}

// Helper to base64 encode
String base64_encode(uint8_t *data, size_t length) {
    unsigned char output[128];
    size_t olen;
    mbedtls_base64_encode(output, sizeof(output), &olen, data, length);
    return String((char *)output);
}

// WS-UsernameToken Verification
// Helper function to find XML element value regardless of namespace prefix
// Handles: <wsse:Username>, <Username>, <ns1:Username>, etc.
int findXmlElementStart(const String &xml, const String &elementName, int searchFrom) {
    // Try with common namespace prefixes first
    const char* prefixes[] = {"wsse:", "wsu:", "", "ns1:", "ns2:", "sec:"};
    for (const char* prefix : prefixes) {
        String tag = String("<") + prefix + elementName;
        int idx = xml.indexOf(tag, searchFrom);
        if (idx >= 0) {
            // Find the closing > of the opening tag (handles attributes)
            int closeTag = xml.indexOf(">", idx);
            if (closeTag >= 0) {
                return closeTag + 1; // Return position after >
            }
        }
    }
    return -1;
}

int findXmlElementEnd(const String &xml, const String &elementName, int searchFrom) {
    // Try with common namespace prefixes
    const char* prefixes[] = {"wsse:", "wsu:", "", "ns1:", "ns2:", "sec:"};
    for (const char* prefix : prefixes) {
        String tag = String("</") + prefix + elementName + ">";
        int idx = xml.indexOf(tag, searchFrom);
        if (idx >= 0) {
            return idx;
        }
    }
    return -1;
}

String extractXmlElement(const String &xml, const String &elementName, int searchFrom) {
    int start = findXmlElementStart(xml, elementName, searchFrom);
    if (start < 0) return "";
    
    int end = findXmlElementEnd(xml, elementName, start);
    if (end < 0) return "";
    
    String value = xml.substring(start, end);
    value.trim();
    return value;
}

bool verify_soap_header(String &soapReq) {
    // 1. Check if Security Header exists (namespace-agnostic)
    int secIdx = soapReq.indexOf("Security");
    if (secIdx < 0) {
        LOG_D("Auth: No Security header in request");
        return false;
    }
    
    // Debug: Log positions for troubleshooting
    if (DEBUG_LEVEL >= 3) {
        int usernamePos = soapReq.indexOf("Username", secIdx);
        int passwordPos = soapReq.indexOf("Password", secIdx);
        Serial.printf("[DEBUG] Security header at %d, Username at %d, Password at %d\n", 
                      secIdx, usernamePos, passwordPos);
    }

    // 2. Extract Username (handles wsse:Username, Username, etc.)
    String username = extractXmlElement(soapReq, "Username", secIdx);
    if (username.length() == 0) {
        LOG_E("Auth: No Username element found");
        return false;
    }
    
    if (username != WEB_USER) {
        LOG_E("Auth: User mismatch. Expected: '" + String(WEB_USER) + "', Got: '" + username + "'");
        return false;
    }

    // 3. Extract Password (Digest) - handles wsse:Password, Password, etc.
    String digestBase64 = extractXmlElement(soapReq, "Password", secIdx);
    if (digestBase64.length() == 0) {
        LOG_E("Auth: No Password element found");
        return false;
    }

    // 4. Extract Nonce - handles wsse:Nonce, Nonce, etc.
    String nonceBase64 = extractXmlElement(soapReq, "Nonce", secIdx);
    if (nonceBase64.length() == 0) {
        LOG_E("Auth: No Nonce element found");
        return false;
    }

    // 5. Extract Created timestamp - handles wsu:Created, Created, etc.
    String created = extractXmlElement(soapReq, "Created", secIdx);
    if (created.length() == 0) {
        LOG_E("Auth: No Created timestamp found");
        return false;
    }
    
    // Debug output for troubleshooting (only in verbose mode)
    if (DEBUG_LEVEL >= 3) {
        Serial.println("[DEBUG] Auth components:");
        Serial.println("  User: '" + username + "'");
        Serial.println("  Nonce: '" + nonceBase64 + "'");
        Serial.println("  Created: '" + created + "'");
        Serial.println("  Password (config): '" + String(WEB_PASS) + "'");
        Serial.println("  Digest (received): '" + digestBase64 + "'");
    }
    
    // 6. Verify Digest = Base64(SHA1(Base64Decode(Nonce) + Created + Password))
    uint8_t nonce[64];
    memset(nonce, 0, sizeof(nonce));
    int nonceLen = base64_decode(nonceBase64, nonce);
    
    if (nonceLen <= 0) {
        LOG_E("Auth: Failed to decode nonce");
        return false;
    }
    
    // Concatenate: nonce + created + password
    uint8_t buffer[256];  // Increased buffer size
    size_t offset = 0;
    memcpy(buffer + offset, nonce, nonceLen);
    offset += nonceLen;
    memcpy(buffer + offset, created.c_str(), created.length());
    offset += created.length();
    memcpy(buffer + offset, WEB_PASS, strlen(WEB_PASS));
    offset += strlen(WEB_PASS);
    
    size_t totalLen = offset;
    
    uint8_t sha1Result[20];
    mbedtls_sha1(buffer, totalLen, sha1Result);
    
    String calculatedDigest = base64_encode(sha1Result, 20);
    
    if (DEBUG_LEVEL >= 3) {
        Serial.println("  Digest (calculated): '" + calculatedDigest + "'");
    }
    
    // Verify digest match
    if (calculatedDigest.equals(digestBase64)) {
         LOG_D("Auth: Digest verification successful");
         return true;
    }
    
    LOG_E("Auth: Digest verification failed");
    if (DEBUG_LEVEL >= 2) {
        Serial.println("  Expected: " + calculatedDigest);
        Serial.println("  Got: " + digestBase64);
    }
    return false; 
}

// Handle SetSystemDateAndTime
// Helper to calculate UTC Epoch from YMDHMS (Simple, no TZ issues)
time_t timegm_impl(struct tm *tm) {
    time_t year = tm->tm_year + 1900;
    time_t month = tm->tm_mon;
    if (month > 11) { year += month/12; month %= 12; }
    else if (month < 0) { int years_diff = (-month + 11)/12; year -= years_diff; month += 12 * years_diff; }
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(((year%4 == 0) && (year%100 != 0)) || (year%400 == 0)) days_in_month[1] = 29;
    time_t total_days = 0;
    for(int y = 1970; y < year; y++) {
       total_days += (((y%4 == 0) && (y%100 != 0)) || (y%400 == 0)) ? 366 : 365;
    }
    for(int m = 0; m < month; m++) {
       total_days += days_in_month[m];
    }
    total_days += tm->tm_mday - 1;
    return (total_days * 86400) + (tm->tm_hour * 3600) + (tm->tm_min * 60) + tm->tm_sec;
}

void handle_SetSystemDateAndTime(String &req) {
    int container = req.indexOf("UTCDateTime");
    if (container < 0) container = req.indexOf("DateTime"); // Fallback
    
    if (container > 0) {
        int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
        
        // Robust Tag Parsing (Handles optional namespaces)
        // Helper lambda to find value between tags
        auto getVal = [&](String tag) -> int {
            int start = req.indexOf("<" + tag + ">", container); // check <tag>
            if(start < 0) start = req.indexOf(":" + tag + ">", container); // check :tag>
            if(start > 0) {
                int valStart = req.indexOf(">", start) + 1;
                int valEnd = req.indexOf("<", valStart);
                return req.substring(valStart, valEnd).toInt();
            }
            return 0;
        };
        
        year = getVal("Year");
        month = getVal("Month");
        day = getVal("Day");
        hour = getVal("Hour");
        min = getVal("Minute");
        sec = getVal("Second");

        if(year > 2000) {
            struct tm tm;
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = min;
            tm.tm_sec = sec;
            
            // Use timegm to treat input as UTC
            time_t t = timegm_impl(&tm);
            struct timeval now = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&now, NULL);
            
            // Verification Code for User
            time_t now_check;
            struct tm timeinfo;
            time(&now_check);
            localtime_r(&now_check, &timeinfo);
            
            LOG_I("Time Sync: UTC " + String(year) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(min) + " -> Local " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min));
        }
    }
}

void sendPROGMEM(WebServer &server, const char* content) {
    server.send_P(200, "application/soap+xml", content);
}

// Helper to send SOAP Fault
void send_soap_fault(WebServer &server, const char* code, const char* subcode, const char* reason) {
    String fault = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    fault += "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:ter=\"http://www.onvif.org/ver10/error\">";
    fault += "<SOAP-ENV:Body><SOAP-ENV:Fault>";
    fault += "<SOAP-ENV:Code><SOAP-ENV:Value>" + String(code) + "</SOAP-ENV:Value>";
    fault += "<SOAP-ENV:Subcode><SOAP-ENV:Value>" + String(subcode) + "</SOAP-ENV:Value></SOAP-ENV:Subcode>";
    fault += "</SOAP-ENV:Code>";
    fault += "<SOAP-ENV:Reason><SOAP-ENV:Text xml:lang=\"en\">" + String(reason) + "</SOAP-ENV:Text></SOAP-ENV:Reason>";
    fault += "</SOAP-ENV:Fault></SOAP-ENV:Body></SOAP-ENV:Envelope>";
    
    server.send(500, "application/soap+xml", fault);
}

// Optimized heap-less send for dynamic content
// Optimized send for dynamic content (Static Buffer = No Heap Frag)
void sendDynamicPROGMEM(WebServer &server, const char* tpl, const char* ip, int port) {
    static char buffer[2048]; // Static allocation (Global memory, created on startup)
    // No OOM check needed for static
    
    snprintf_P(buffer, sizeof(buffer), PART_HEADER); 
    size_t len = strlen(buffer);
    snprintf_P(buffer + len, sizeof(buffer) - len, tpl, ip, port);
    server.send(200, "application/soap+xml", buffer);
}

// Overload for just sending fixed PROGMEM with header
void sendFixedPROGMEM(WebServer &server, const char* tpl) {
    static char buffer[2048]; // reuse static buffer (safe single-threaded)
    
    snprintf_P(buffer, sizeof(buffer), PART_HEADER);
    strncat_P(buffer, tpl, sizeof(buffer) - strlen(buffer) - 1);
    server.send(200, "application/soap+xml", buffer);
}

// --- New Handlers ---

// Mandatory for many NVRs to link Profile to Source
// Mandatory for many NVRs to link Profile to Source
// Now Dynamic to report actual Brightness/Contrast/Color
const char PROGMEM TPL_VIDEO_SOURCES[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetVideoSourcesResponse>"
        "<trt:VideoSources token=\"VideoSource_1\">"
            "<tt:Framerate>20.0</tt:Framerate>"
            "<tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution>"
            "<tt:Imaging>"
                "<tt:BacklightCompensation><tt:Mode>OFF</tt:Mode></tt:BacklightCompensation>"
                "<tt:Brightness>%d</tt:Brightness>"
                "<tt:ColorSaturation>%d</tt:ColorSaturation>"
                "<tt:Contrast>%d</tt:Contrast>"
                "<tt:Exposure><tt:Mode>AUTO</tt:Mode></tt:Exposure>"
            "</tt:Imaging>"
        "</trt:VideoSources>"
    "</trt:GetVideoSourcesResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

// Tells NVR valid ranges (resolutions, quality, etc)
const char PROGMEM TPL_VIDEO_OPTIONS[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetVideoEncoderConfigurationOptionsResponse>"
        "<trt:Options>"
            "<tt:QualityRange><tt:Min>0</tt:Min><tt:Max>63</tt:Max></tt:QualityRange>"
            "<tt:JPEG><tt:ResolutionsAvailable><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:ResolutionsAvailable>"
            "<tt:FrameRateRange><tt:Min>1</tt:Min><tt:Max>20</tt:Max></tt:FrameRateRange>"
            "<tt:EncodingIntervalRange><tt:Min>1</tt:Min><tt:Max>1</tt:Max></tt:EncodingIntervalRange>"
            "</tt:JPEG>"
        "</trt:Options>"
    "</trt:GetVideoEncoderConfigurationOptionsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_VIDEO_ENCODER_CONFIG_MAIN[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetVideoEncoderConfigurationResponse>"
        "<trt:Configuration token=\"VideoEncoderToken_Main\">"
        "<tt:Name>VideoEncoderConfig_Main</tt:Name>"
        "<tt:UseCount>1</tt:UseCount>"
        "<tt:Encoding>JPEG</tt:Encoding>"
        "<tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution>"
        "<tt:Quality>10</tt:Quality>"
        "<tt:RateControl><tt:FrameRateLimit>20</tt:FrameRateLimit><tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>4096</tt:BitrateLimit></tt:RateControl>"
        "<tt:JPEG><tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution></tt:JPEG>"
        "<tt:Multicast><tt:Address><tt:Type>IPv4</tt:Type><tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address><tt:Port>0</tt:Port><tt:TTL>1</tt:TTL><tt:AutoStart>false</tt:AutoStart></tt:Multicast>"
        "<tt:SessionTimeout>PT60S</tt:SessionTimeout>"
        "</trt:Configuration>"
    "</trt:GetVideoEncoderConfigurationResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_VIDEO_ENCODER_CONFIG_SUB[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetVideoEncoderConfigurationResponse>"
        "<trt:Configuration token=\"VideoEncoderToken_Sub\">"
        "<tt:Name>VideoEncoderConfig_Sub</tt:Name>"
        "<tt:UseCount>1</tt:UseCount>"
        "<tt:Encoding>JPEG</tt:Encoding>"
        "<tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution>"
        "<tt:Quality>10</tt:Quality>"
        "<tt:RateControl><tt:FrameRateLimit>20</tt:FrameRateLimit><tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>4096</tt:BitrateLimit></tt:RateControl>"
        "<tt:JPEG><tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution></tt:JPEG>"
        "<tt:Multicast><tt:Address><tt:Type>IPv4</tt:Type><tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address><tt:Port>0</tt:Port><tt:TTL>1</tt:TTL><tt:AutoStart>false</tt:AutoStart></tt:Multicast>"
        "<tt:SessionTimeout>PT60S</tt:SessionTimeout>"
        "</trt:Configuration>"
    "</trt:GetVideoEncoderConfigurationResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";




const char PROGMEM TPL_HOSTNAME[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetHostnameResponse>"
    "<tds:HostnameInformation>"
       "<tds:FromDHCP>false</tds:FromDHCP>"
       "<tds:Name>" DEVICE_MODEL "</tds:Name>"
    "</tds:HostnameInformation>"
    "</tds:GetHostnameResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

// Audio Stubs (Empty to prevent errors)
// Imaging Options (Brightness/Contrast/Saturation)
const char PROGMEM TPL_IMAGING_OPTIONS[] = 
    "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<timg:GetOptionsResponse>"
        "<timg:ImagingOptions>"
            "<tt:BacklightCompensation><tt:Mode>OFF</tt:Mode><tt:Mode>ON</tt:Mode></tt:BacklightCompensation>"
            "<tt:Brightness><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Brightness>"
            "<tt:ColorSaturation><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:ColorSaturation>"
            "<tt:Contrast><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Contrast>"
            "<tt:Exposure><tt:Mode>AUTO</tt:Mode><tt:MinExposureTime>0.0</tt:MinExposureTime><tt:MaxExposureTime>0.0</tt:MaxExposureTime><tt:MinGain>0.0</tt:MinGain><tt:MaxGain>0.0</tt:MaxGain><tt:MinIris>0.0</tt:MinIris><tt:MaxIris>0.0</tt:MaxIris></tt:Exposure>"
            "<tt:Focus><tt:AutoFocusModes>AUTO</tt:AutoFocusModes><tt:DefaultSpeed><tt:PanTilt x=\"1\" y=\"1\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace\"/><tt:Zoom x=\"1\" space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace\"/></tt:DefaultSpeed><tt:NearLimit>0.0</tt:NearLimit><tt:FarLimit>0.0</tt:FarLimit></tt:Focus>"
            "<tt:IrCutFilterModes>ON</tt:IrCutFilterModes><tt:IrCutFilterModes>OFF</tt:IrCutFilterModes><tt:IrCutFilterModes>AUTO</tt:IrCutFilterModes>"
        "</timg:ImagingOptions>"
    "</timg:GetOptionsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_AUDIO_OPTIONS[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetAudioEncoderConfigurationOptionsResponse>"
        "<trt:Options>"
        "</trt:Options>"
    "</trt:GetAudioEncoderConfigurationOptionsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_AUDIO_CONFIG[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetAudioEncoderConfigurationResponse>"
    "</trt:GetAudioEncoderConfigurationResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_MOVE_OPTIONS[] = 
    "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tptz:GetMoveOptionsResponse>"
        "<tptz:MoveOptions>"
            "<tt:Absolute>"
                "<tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>"
                "<tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>"
            "</tt:Absolute>"
            "<tt:Continuous>"
                "<tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>"
                "<tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>"
            "</tt:Continuous>"
        "</tptz:MoveOptions>"
    "</tptz:GetMoveOptionsResponse>"
    "</tptz:GetMoveOptionsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_IMAGING_MOVE_OPTIONS[] = 
    "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<timg:GetMoveOptionsResponse>"
        "<timg:MoveOptions>"
            "<tt:Absolute>"
                "<tt:Focus>"
                     "<tt:Position><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:Position>"
                     "<tt:Speed><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:Speed>"
                "</tt:Focus>"
            "</tt:Absolute>"
            "<tt:Continuous>"
                 "<tt:Focus>"
                     "<tt:Speed><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:Speed>"
                "</tt:Focus>"
            "</tt:Continuous>"
        "</timg:MoveOptions>"
    "</timg:GetMoveOptionsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_SET_SYNC_POINT[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:SetSynchronizationPointResponse/>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

// OSD Stubs (Empty to prevent errors)
const char PROGMEM TPL_OSD_OPTIONS[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetOSDOptionsResponse>"
        "<trt:OSDOptions>"
        "</trt:OSDOptions>"
    "</trt:GetOSDOptionsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

const char PROGMEM TPL_ANALYTICS_CONFIG[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetVideoAnalyticsConfigurationsResponse>"
    "</trt:GetVideoAnalyticsConfigurationsResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

// Network Interfaces (MAC Address)
const char PROGMEM TPL_NETWORK_INTERFACES[] = 
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<tds:GetNetworkInterfacesResponse>"
        "<tds:NetworkInterfaces token=\"InterfaceToken\">"
            "<tt:Enabled>true</tt:Enabled>"
            "<tt:Info>"
                "<tt:Name>wlan0</tt:Name>"
                "<tt:HwAddress>%s</tt:HwAddress>"
                "<tt:MTU>1500</tt:MTU>"
            "</tt:Info>"
            "<tt:IPv4>"
                "<tt:Enabled>true</tt:Enabled>"
                "<tt:Config>"
                    "<tt:Manual>"
                        "<tt:Address>%s</tt:Address>"
                        "<tt:PrefixLength>24</tt:PrefixLength>"
                    "</tt:Manual>"
                    "<tt:DHCP>false</tt:DHCP>"
                "</tt:Config>"
            "</tt:IPv4>"
        "</tds:NetworkInterfaces>"
    "</tds:GetNetworkInterfacesResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

void handle_GetCapabilities() {
    String ip = WiFi.localIP().toString();
    
    LOG_D("Sending GetCapabilities response");
    
    // Build complete response first for better reliability
    char buffer[1500];
    int len = snprintf(buffer, sizeof(buffer),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
        "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "<SOAP-ENV:Body>"
        "<tds:GetCapabilitiesResponse>"
        "<tds:Capabilities>"
            "<tt:Device>"
                "<tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr>"
                "<tt:Network>"
                    "<tt:IPFilter>false</tt:IPFilter>"
                    "<tt:ZeroConfiguration>false</tt:ZeroConfiguration>"
                    "<tt:IPVersion6>false</tt:IPVersion6>"
                    "<tt:DynDNS>false</tt:DynDNS>"
                "</tt:Network>"
                "<tt:System>"
                    "<tt:DiscoveryResolve>false</tt:DiscoveryResolve>"
                    "<tt:DiscoveryBye>false</tt:DiscoveryBye>"
                    "<tt:RemoteDiscovery>false</tt:RemoteDiscovery>"
                    "<tt:SystemBackup>false</tt:SystemBackup>"
                    "<tt:FirmwareUpgrade>false</tt:FirmwareUpgrade>"
                    "<tt:SupportedVersions>"
                        "<tt:Major>2</tt:Major>"
                        "<tt:Minor>0</tt:Minor>"
                    "</tt:SupportedVersions>"
                "</tt:System>"
            "</tt:Device>"
            "<tt:Media>"
                "<tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr>"
                "<tt:StreamingCapabilities>"
                    "<tt:RTPMulticast>false</tt:RTPMulticast>"
                    "<tt:RTP_TCP>true</tt:RTP_TCP>"
                    "<tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP>"
                "</tt:StreamingCapabilities>"
            "</tt:Media>"
            "<tt:Events>"
                "<tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr>"
                "<tt:WSSubscriptionPolicySupport>false</tt:WSSubscriptionPolicySupport>"
                "<tt:WSPullPointSupport>false</tt:WSPullPointSupport>"
            "</tt:Events>"
        "</tds:Capabilities>"
        "</tds:GetCapabilitiesResponse>"
        "</SOAP-ENV:Body>"
        "</SOAP-ENV:Envelope>",
        ip.c_str(), ONVIF_PORT,
        ip.c_str(), ONVIF_PORT,
        ip.c_str(), ONVIF_PORT);
    
    if (len > 0 && len < sizeof(buffer)) {
        onvifServer.send(200, "application/soap+xml", buffer);
        LOG_D("GetCapabilities sent, size: " + String(len));
    } else {
        LOG_E("GetCapabilities buffer overflow!");
        onvifServer.send(500, "text/plain", "Buffer overflow");
    }
}

void handle_GetStreamUri() {
    sendDynamicPROGMEM(onvifServer, TPL_STREAM_URI, WiFi.localIP().toString().c_str(), RTSP_PORT);
}

void handle_GetSystemDateAndTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    
    char *buffer = new char[1024];
    if(buffer) {
        snprintf_P(buffer, 1024, PART_HEADER);
        size_t len = strlen(buffer);
        // Note: tm_year is years since 1900, tm_mon is 0-11
        snprintf_P(buffer + len, 1024 - len, TPL_TIME_FMT, 
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
            
        onvifServer.send(200, "application/soap+xml", buffer);
        delete[] buffer;
    } else {
        onvifServer.send(500, "text/plain", "OOM");
    }
}







// Simple parser for SetImagingSettings
// We look for <tt:IrCutFilterMode>OFF</tt:IrCutFilterMode> to turn on 'Night Mode' (Flash ON)
// and ON or AUTO for 'Day Mode' (Flash OFF)
void handle_set_imaging_settings(String &req) {
    if (!FLASH_LED_ENABLED) return;
    
    // Very basic string parsing as XML parsing is heavy
    if (req.indexOf("IrCutFilterMode") > 0) {
        if (req.indexOf(">OFF<") > 0) {
            // Night mode -> Flash ON
            set_flash_led(true);
            LOG_I("Night Mode: ON (Flash)");
        } else {
            // Day mode (ON or AUTO) -> Flash OFF
            set_flash_led(false);
            LOG_I("Night Mode: OFF (Day)");
        }
    }
}



void handle_ptz(String &req) {
   #if PTZ_ENABLED
   // AbsoluteMove
   // <tptz:Vector PanTilt="x" y="0.5"/>
   // Simplified parsing: find PanTilt space x=" and y="
   // This is fragile but suffices for minimal SOAP
   
   if (req.indexOf("AbsoluteMove") > 0) {
       // Look for x="0.5" y="0.5" or similar
       // Or PanTilt x="0.5" y="0.5"
       // Actually ONVIF usually sends: <tt:PanTilt x="0.5" y="0.5" ... />
       
       float x = 0.5f; 
       float y = 0.5f;
       
       int xIdx = req.indexOf("x=\"");
       if (xIdx > 0) {
           int endQ = req.indexOf("\"", xIdx + 3);
           String val = req.substring(xIdx + 3, endQ);
           x = val.toFloat();
       }
       
       int yIdx = req.indexOf("y=\"");
       if (yIdx > 0) {
           int endQ = req.indexOf("\"", yIdx + 3);
           String val = req.substring(yIdx + 3, endQ);
           y = val.toFloat();
       }
       
       // ONVIF uses -1 to 1. Map to 0 to 1.
       // x = (x + 1.0) / 2.0;
       // y = (y + 1.0) / 2.0; 
       // NOTE: Some NVRs assume 0..1, others -1..1. 
       // Let's assume -1..1 for standard ONVIF PTZ vectors.
       
       float finalX = (x + 1.0f) / 2.0f;
       float finalY = (y + 1.0f) / 2.0f;
       
       ptz_set_absolute(finalX, finalY);
       Serial.printf("[INFO] PTZ Move: x=%.2f y=%.2f -> servo=%.2f, %.2f\n", x, y, finalX, finalY);
   }
   #endif
}

// Note: Some NVRs will fail Probe/Discovery if authentication is required for simple gets.
// ONVIF Specification: GetCapabilities, GetServices, GetSystemDateAndTime, GetDeviceInformation
// should be PUBLIC (no auth required) to allow discovery.
// Only protected actions like GetStreamUri, GetProfiles need authentication.
void handle_onvif_soap() {
  String req = onvifServer.arg(0);
  
  // Detect action first for proper logging and auth decisions
  String action = "Unknown";
  if (req.indexOf("GetSystemDateAndTime") > 0) action = "GetSystemDateAndTime";
  else if (req.indexOf("SetSystemDateAndTime") > 0) action = "SetSystemDateAndTime";
  else if (req.indexOf("SetSynchronizationPoint") > 0) action = "SetSynchronizationPoint";
  else if (req.indexOf("GetCapabilities") > 0) action = "GetCapabilities";
  else if (req.indexOf("GetServices") > 0) action = "GetServices";
  else if (req.indexOf("GetDeviceInformation") > 0) action = "GetDeviceInformation";
  else if (req.indexOf("GetProfiles") > 0) action = "GetProfiles";
  else if (req.indexOf("GetStreamUri") > 0) action = "GetStreamUri";
  else if (req.indexOf("GetSnapshotUri") > 0) action = "GetSnapshotUri";
  else if (req.indexOf("GetVideoSources") > 0) action = "GetVideoSources";
  else if (req.indexOf("GetVideoEncoderConfigurationOptions") > 0) action = "GetVideoOptions";
  else if (req.indexOf("GetVideoEncoderConfiguration") > 0) action = "GetVideoConfig";
  else if (req.indexOf("GetAudioEncoderConfiguration") > 0) action = "GetAudioConfig";
  else if (req.indexOf("SetVideoEncoderConfiguration") > 0) action = "SetVideoConfig";
  else if (req.indexOf("GetNetworkInterfaces") > 0) action = "GetNetworkInterfaces";
  else if (req.indexOf("GetNetworkProtocols") > 0) action = "GetNetworkProtocols";
  else if (req.indexOf("GetScopes") > 0) action = "GetScopes";
  else if (req.indexOf("GetHostname") > 0) action = "GetHostname";
  else if (req.indexOf("GetDNS") > 0) action = "GetDNS";
  else if (req.indexOf("GetNTP") > 0) action = "GetNTP";
  else if (req.indexOf("GetOSDOptions") > 0) action = "GetOSDOptions";
  else if (req.indexOf("GetMoveOptions") > 0) action = "GetMoveOptions";
  else if (req.indexOf("GetVideoAnalyticsConfigurations") > 0) action = "GetAnalyticsConfig";
  else if (req.indexOf("GetOptions") > 0 && req.indexOf("VideoSourceToken") > 0) action = "GetImagingOptions";
  else if (req.indexOf("SetImagingSettings") > 0) action = "SetImagingSettings";
  else if (req.indexOf("AbsoluteMove") > 0 || req.indexOf("ContinuousMove") > 0 || req.indexOf("Stop") > 0) action = "PTZ";
  
  // PUBLIC actions (no auth required per ONVIF spec)
  // These are needed for device discovery and initial handshake
  // Hikvision calls many of these during initial camera probe
  bool isPublicAction = (
      action == "GetCapabilities" ||
      action == "GetServices" ||
      action == "GetSystemDateAndTime" ||
      action == "GetDeviceInformation" ||
      action == "GetScopes" ||
      action == "GetHostname" ||
      action == "GetNetworkInterfaces" ||
      action == "GetNetworkProtocols" ||
      action == "GetDNS" ||
      action == "GetNTP" ||
      action == "GetVideoOptions" ||   // Needed for codec negotiation
      action == "GetAudioConfig"       // Usually empty, safe to expose
  );
  
  // PROTECTED actions (require authentication)
  // These provide access to actual streams or modify settings
  bool isProtectedAction = (
      action == "GetStreamUri" || 
      action == "GetProfiles" || 
      action == "SetSystemDateAndTime" ||
      action == "GetVideoSources" ||
      action == "GetVideoConfig" ||
      action == "GetSnapshotUri" ||
      action == "SetVideoConfig" ||
      action == "SetImagingSettings" ||
      action == "PTZ"
  );
  
  // Check if request contains Security header
  bool hasSecurity = (req.indexOf("Security") > 0);
  
  // Authentication logic:
  // 1. If Security header is present, we MUST verify it (even for public actions)
  // 2. If action is protected but no Security header, require auth
  // 3. If action is public and no Security header, allow through
  
  if (hasSecurity) {
      // Request has auth header - verify it
      if (!verify_soap_header(req)) {
          LOG_E("Auth Failed for: " + action);
          if (DEBUG_LEVEL >= 3) {
              // Verbose: show why auth failed
              int secIdx = req.indexOf("Security");
              int userIdx = req.indexOf("wsse:Username");
              int passIdx = req.indexOf("wsse:Password");
              Serial.printf("[DEBUG] Security header at %d, Username at %d, Password at %d\n", 
                           secIdx, userIdx, passIdx);
          }
          send_soap_fault(onvifServer, "env:Sender", "ter:NotAuthorized", "Authentication failed");
          return;
      }
      LOG_D("Auth OK for: " + action);
  } else if (isProtectedAction) {
      // Protected action without auth - reject
      LOG_E("Auth Required for: " + action + " (no credentials provided)");
      send_soap_fault(onvifServer, "env:Sender", "ter:NotAuthorized", "Authentication required");
      return;
  }
  // Public action without auth - allow through
  
  LOG_I("ONVIF: " + action);

  // Handle unknown actions with debug output
  if (action == "Unknown") {
      // Find the Body tag to show relevant info without header spam
      int bodyIdx = req.indexOf("<SOAP-ENV:Body>");
      if (bodyIdx == -1) bodyIdx = req.indexOf("Body>");
      
      Serial.println("[DEBUG] UNKNOWN ACTION BODY:");
      if (bodyIdx > 0) {
          Serial.println(req.substring(bodyIdx)); 
      } else {
          Serial.println(req); 
      }
  }

  if (req.indexOf("GetCapabilities") > 0) {
    handle_GetCapabilities();
  } else if (req.indexOf("GetStreamUri") > 0) {
    handle_GetStreamUri();
  } else if (req.indexOf("GetSnapshotUri") > 0) {
    // Send dynamic Snapshot URI pointing to /snapshot
    const char PROGMEM TPL_SNAPSHOT_URI[] = 
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body>"
    "<trt:GetSnapshotUriResponse>"
    "<trt:MediaUri>"
    "<tt:Uri>http://%s:%d/snapshot</tt:Uri>"
    "<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
    "<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
    "<tt:Timeout>PT0S</tt:Timeout>"
    "</trt:MediaUri>"
    "</trt:GetSnapshotUriResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
    
    sendDynamicPROGMEM(onvifServer, TPL_SNAPSHOT_URI, WiFi.localIP().toString().c_str(), WEB_PORT);
  } else if (req.indexOf("GetDeviceInformation") > 0) {
    // Dynamically insert MAC address as Serial Number for better NVR compatibility
    sendDynamicPROGMEM(onvifServer, TPL_DEV_INFO, WiFi.macAddress().c_str(), 0); 
  } else if (req.indexOf("GetSystemDateAndTime") > 0) {
    handle_GetSystemDateAndTime();
  } else if (req.indexOf("GetServices") > 0) {
    onvifServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    onvifServer.send(200, "application/soap+xml", "");
    
    char buffer[512];
    String ip = WiFi.localIP().toString();

    // Header + Device Service
    snprintf_P(buffer, sizeof(buffer), PSTR("%s"
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<SOAP-ENV:Body>"
    "<tds:GetServicesResponse>"
        "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace><tds:XAddr>http://%s:%d/onvif/device_service</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>"), PART_HEADER, ip.c_str(), ONVIF_PORT);
    onvifServer.sendContent(buffer);
    
    // Media Service + Footer
    snprintf_P(buffer, sizeof(buffer), PSTR(
        "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace><tds:XAddr>http://%s:%d/onvif/device_service</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>"
    "</tds:GetServicesResponse>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>"), ip.c_str(), ONVIF_PORT);
    onvifServer.sendContent(buffer);

  } else if (req.indexOf("GetProfiles") > 0) {
     LOG_D("Sending GetProfiles response");
     
     char buffer[1800];
     int len;
     
     // ============================================================================
     // EXPERIMENTAL: Claim H.264 to satisfy Hikvision HVR
     // The actual RTSP stream is still MJPEG but we tell Hikvision it's H.264
     // This may allow Hikvision to accept the camera (though video may not display)
     // To use MJPEG instead: comment out the H.264 section below and uncomment the MJPEG section
     // ============================================================================
     
     // --- H.264 CLAIM (EXPERIMENTAL - for Hikvision HVR compatibility) ---
     len = snprintf(buffer, sizeof(buffer),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
        "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "<SOAP-ENV:Body>"
        "<trt:GetProfilesResponse>"
            "<trt:Profiles token=\"Profile_1\" fixed=\"true\">"
                "<tt:Name>MainStream</tt:Name>"
                "<tt:VideoSourceConfiguration token=\"VideoSourceToken\">"
                    "<tt:Name>VideoSource</tt:Name>"
                    "<tt:UseCount>1</tt:UseCount>"
                    "<tt:SourceToken>VideoSource_1</tt:SourceToken>"
                    "<tt:Bounds x=\"0\" y=\"0\" width=\"640\" height=\"480\"/>"
                "</tt:VideoSourceConfiguration>"
                "<tt:VideoEncoderConfiguration token=\"VideoEncoderToken\">"
                    "<tt:Name>VideoEncoder</tt:Name>"
                    "<tt:UseCount>1</tt:UseCount>"
                    "<tt:Encoding>H264</tt:Encoding>"
                    "<tt:Resolution>"
                        "<tt:Width>640</tt:Width>"
                        "<tt:Height>480</tt:Height>"
                    "</tt:Resolution>"
                    "<tt:Quality>5</tt:Quality>"
                    "<tt:RateControl>"
                        "<tt:FrameRateLimit>20</tt:FrameRateLimit>"
                        "<tt:EncodingInterval>1</tt:EncodingInterval>"
                        "<tt:BitrateLimit>2048</tt:BitrateLimit>"
                    "</tt:RateControl>"
                    "<tt:H264>"
                        "<tt:GovLength>30</tt:GovLength>"
                        "<tt:H264Profile>Baseline</tt:H264Profile>"
                    "</tt:H264>"
                    "<tt:Multicast>"
                        "<tt:Address><tt:Type>IPv4</tt:Type><tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address>"
                        "<tt:Port>0</tt:Port>"
                        "<tt:TTL>1</tt:TTL>"
                        "<tt:AutoStart>false</tt:AutoStart>"
                    "</tt:Multicast>"
                    "<tt:SessionTimeout>PT60S</tt:SessionTimeout>"
                "</tt:VideoEncoderConfiguration>"
            "</trt:Profiles>"
        "</trt:GetProfilesResponse>"
        "</SOAP-ENV:Body>"
        "</SOAP-ENV:Envelope>");
     
     /*
     // --- MJPEG (ORIGINAL - for NVRs that support MJPEG like Blue Iris, Shinobi) ---
     len = snprintf(buffer, sizeof(buffer),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
        "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "<SOAP-ENV:Body>"
        "<trt:GetProfilesResponse>"
            "<trt:Profiles token=\"Profile_1\" fixed=\"true\">"
                "<tt:Name>MainStream</tt:Name>"
                "<tt:VideoSourceConfiguration token=\"VideoSourceToken\">"
                    "<tt:Name>VideoSource</tt:Name>"
                    "<tt:UseCount>1</tt:UseCount>"
                    "<tt:SourceToken>VideoSource_1</tt:SourceToken>"
                    "<tt:Bounds x=\"0\" y=\"0\" width=\"640\" height=\"480\"/>"
                "</tt:VideoSourceConfiguration>"
                "<tt:VideoEncoderConfiguration token=\"VideoEncoderToken\">"
                    "<tt:Name>VideoEncoder</tt:Name>"
                    "<tt:UseCount>1</tt:UseCount>"
                    "<tt:Encoding>JPEG</tt:Encoding>"
                    "<tt:Resolution>"
                        "<tt:Width>640</tt:Width>"
                        "<tt:Height>480</tt:Height>"
                    "</tt:Resolution>"
                    "<tt:Quality>5</tt:Quality>"
                    "<tt:RateControl>"
                        "<tt:FrameRateLimit>20</tt:FrameRateLimit>"
                        "<tt:EncodingInterval>1</tt:EncodingInterval>"
                        "<tt:BitrateLimit>4096</tt:BitrateLimit>"
                    "</tt:RateControl>"
                    "<tt:Multicast>"
                        "<tt:Address><tt:Type>IPv4</tt:Type><tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address>"
                        "<tt:Port>0</tt:Port>"
                        "<tt:TTL>1</tt:TTL>"
                        "<tt:AutoStart>false</tt:AutoStart>"
                    "</tt:Multicast>"
                    "<tt:SessionTimeout>PT60S</tt:SessionTimeout>"
                "</tt:VideoEncoderConfiguration>"
            "</trt:Profiles>"
        "</trt:GetProfilesResponse>"
        "</SOAP-ENV:Body>"
        "</SOAP-ENV:Envelope>");
     */
     
     if (len > 0 && len < sizeof(buffer)) {
         onvifServer.send(200, "application/soap+xml", buffer);
         LOG_D("GetProfiles sent, size: " + String(len));
     } else {
         LOG_E("GetProfiles buffer overflow!");
         onvifServer.send(500, "text/plain", "Buffer overflow");
     }


  } else if (req.indexOf("GetVideoSources") > 0) {
    // Inject current Sensor values
    sensor_t * s = esp_camera_sensor_get();
    // Map -2..2 to 0..100 or similar if needed, but ONVIF is often 0..100.
    // ESP32Cam standard is -2 to 2. Let's map linearly: -2=0, -1=25, 0=50, 1=75, 2=100
    // Brightness
    int br = (s->status.brightness + 2) * 25; 
    // Contrast
    int cn = (s->status.contrast + 2) * 25;
    // Saturation
    int sa = (s->status.saturation + 2) * 25;

    char *buffer = new char[2048];
    if(buffer) {
        snprintf_P(buffer, 2048, PART_HEADER);
        size_t len = strlen(buffer);
        snprintf_P(buffer + len, 2048 - len, TPL_VIDEO_SOURCES, br, sa, cn);
        onvifServer.send(200, "application/soap+xml", buffer);
        delete[] buffer;
    } else {
        onvifServer.send(500, "text/plain", "OOM");
    }
  } else if (req.indexOf("GetVideoEncoderConfigurationOptions") > 0) {
    sendFixedPROGMEM(onvifServer, TPL_VIDEO_OPTIONS);
  } else if (req.indexOf("GetVideoEncoderConfiguration") > 0) {
    if (req.indexOf("VideoEncoderToken_Sub") > 0) {
        sendFixedPROGMEM(onvifServer, TPL_VIDEO_ENCODER_CONFIG_SUB);
    } else {
        // Default to Main if unspecified or Main
        sendFixedPROGMEM(onvifServer, TPL_VIDEO_ENCODER_CONFIG_MAIN);
    }
  } else if (req.indexOf("GetNetworkInterfaces") > 0) {
    // Pass MAC and IP to the template
    char *buffer = new char[2048];
    if(buffer) {
        snprintf_P(buffer, 2048, PART_HEADER); 
        size_t len = strlen(buffer);
        snprintf_P(buffer + len, 2048 - len, TPL_NETWORK_INTERFACES, 
            WiFi.macAddress().c_str(), 
            WiFi.localIP().toString().c_str());
        onvifServer.send(200, "application/soap+xml", buffer);
        delete[] buffer;
    } else {
        onvifServer.send(500, "text/plain", "OOM");
    }
  } else if (req.indexOf("GetAudioEncoderConfigurationOptions") > 0) {
    sendFixedPROGMEM(onvifServer, TPL_AUDIO_OPTIONS); // Return empty options
  } else if (req.indexOf("GetAudioEncoderConfiguration") > 0) {
     // Return empty or fault? Empty list is safer for "Not Supported"
     sendFixedPROGMEM(onvifServer, TPL_AUDIO_CONFIG);
  } else if (req.indexOf("GetOSDOptions") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_OSD_OPTIONS);
  } else if (req.indexOf("GetVideoAnalyticsConfigurations") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_ANALYTICS_CONFIG);
  } else if (req.indexOf("GetVideoAnalyticsConfigurations") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_ANALYTICS_CONFIG);
  } else if (req.indexOf("GetOptions") > 0 && req.indexOf("VideoSourceToken") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_IMAGING_OPTIONS);
  } else if (req.indexOf("GetScopes") > 0) {
     const char PROGMEM TPL_SCOPES[] = 
     "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
     "<SOAP-ENV:Body>"
     "<tds:GetScopesResponse>"
         "<tds:Scopes><tt:ScopeDef>Configurable</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/name/" DEVICE_MODEL "</tt:ScopeItem></tds:Scopes>"
         "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/type/Network_Video_Transmitter</tt:ScopeItem></tds:Scopes>"
         "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/hardware/" DEVICE_HARDWARE_ID "</tt:ScopeItem></tds:Scopes>"
         "<tds:Scopes><tt:ScopeDef>Configurable</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/location/Office</tt:ScopeItem></tds:Scopes>"
     "</tds:GetScopesResponse>"
     "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
     sendFixedPROGMEM(onvifServer, TPL_SCOPES);
  } else if (req.indexOf("GetHostname") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_HOSTNAME);
  } else if (req.indexOf("SetSystemDateAndTime") > 0) {
    handle_SetSystemDateAndTime(req);
    sendFixedPROGMEM(onvifServer, TPL_SET_TIME_RES);
  } else if (req.indexOf("SetImagingSettings") > 0 || req.indexOf("SetVideoEncoderConfiguration") > 0) {
    // Acknowledge setting commands with OK (we ignore the actual values to enforce stability)
    onvifServer.send(200, "application/soap+xml", "<ok/>"); 
  } else if (req.indexOf("GetDNS") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_DNS);
  } else if (req.indexOf("GetNTP") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_NTP);
  } else if (req.indexOf("GetNetworkProtocols") > 0) {
     sendFixedPROGMEM(onvifServer, TPL_NET_PROTOCOLS);
  } else if (req.indexOf("GetMoveOptions") > 0) {
      if (req.indexOf("VideoSourceToken") > 0) {
          sendFixedPROGMEM(onvifServer, TPL_IMAGING_MOVE_OPTIONS);
      } else {
          sendFixedPROGMEM(onvifServer, TPL_MOVE_OPTIONS);
      }
  } else if (req.indexOf("SetSynchronizationPoint") > 0) {
      sendFixedPROGMEM(onvifServer, TPL_SET_SYNC_POINT);
  } else if (req.indexOf("AbsoluteMove") > 0 || req.indexOf("ContinuousMove") > 0 || req.indexOf("Stop") > 0) {
    handle_ptz(req);
    onvifServer.send(200, "application/soap+xml", "<ok/>");
  } else {
    onvifServer.send(200, "application/soap+xml", "<ok/>");
  }
}

void handle_onvif_discovery() {
  int packetSize = onvifUDP.parsePacket();
  if (packetSize) {
    char packet[1024];
    int len = onvifUDP.read(packet, 1024);
    if(len > 0) {
        packet[len] = 0;
        // Optimization: Use strstr on buffer instead of allocating String object
        if (strstr(packet, "Probe") != nullptr) {
          String ip = WiFi.localIP().toString();
      
      // WS-Discovery Probe Match Response
      // This is the CRITICAL packet for NVRs to find the camera.
      // - RelatesTo: Should match the MessageID of the Probe (omitted here for simplicity as UDP allows multicast broadcast).
      // - Types: dn:NetworkVideoTransmitter (Tells NVR this is a Camera).
      // - XAddrs: The URL to the implementation of the device service (http://<IP>:8000/onvif/device_service).
      // - Scopes: onvif://www.onvif.org/Profile/Streaming (Capabilities).
      
      String resp =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:SOAP-ENC=\"http://www.w3.org/2003/05/soap-encoding\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
        "<SOAP-ENV:Body>"
        "<ProbeMatches xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
        "<ProbeMatch>"
        "<EndpointReference><Address>urn:uuid:esp32-cam-onvif-" + WiFi.macAddress() + "</Address></EndpointReference>"
        "<Types>dn:NetworkVideoTransmitter</Types>"
        "<Scopes>onvif://www.onvif.org/type/Network_Video_Transmitter onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/location/Office onvif://www.onvif.org/name/" DEVICE_MODEL " onvif://www.onvif.org/hardware/" DEVICE_HARDWARE_ID "</Scopes>"
        "<XAddrs>http://" + ip + ":" + String(ONVIF_PORT) + "/onvif/device_service</XAddrs>"
        "<MetadataVersion>1</MetadataVersion>"
        "</ProbeMatch>"
        "</ProbeMatches>"
        "</SOAP-ENV:Body>"
        "</SOAP-ENV:Envelope>";
      onvifUDP.beginPacket(onvifUDP.remoteIP(), onvifUDP.remotePort());
      onvifUDP.write((const uint8_t*)resp.c_str(), resp.length());
      onvifUDP.endPacket();
    }
  }
  } // End if(packetSize)
} // End function


void onvif_server_start() {
  onvifServer.on("/onvif/device_service", HTTP_POST, handle_onvif_soap);
  onvifServer.on("/onvif/ptz_service", HTTP_POST, handle_onvif_soap); // Route PTZ to same handler for now
  onvifServer.begin();
  onvifUDP.beginMulticast(IPAddress(239,255,255,250), 3702); // Fixed: use only 2 args
  LOG_I("ONVIF server started.");
}

void onvif_server_loop() {
  if (!_onvifEnabled) return;
  onvifServer.handleClient();
  handle_onvif_discovery();
}
