#include "webdav_server.h"
#include "config.h"
#include "sd_recorder.h"
#include <SD_MMC.h>

#define WEBDAV_PREFIX "/webdav"

static const char* XML1 = "<?xml version=\"1.0\" encoding=\"utf-8\"?><D:multistatus xmlns:D=\"DAV:\">";
static const char* XML2 = "<D:response xmlns:D=\"DAV:\"><D:href>";
static const char* XML3 = "</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop>";
static const char* XML4 = "</D:prop></D:propstat></D:response>";

static void sendContentProp(WebServer* server, const char* prop, const char* value) {
    char propStr[256];
    snprintf(propStr, sizeof(propStr), "<D:%s>%s</D:%s>", prop, value, prop);
    server->sendContent(propStr);
}

static void sendPropResponse(WebServer* server, File& file, String basePath) {
    String href = String(WEBDAV_PREFIX) + basePath + file.name();
    if (file.isDirectory() && !href.endsWith("/")) {
        href += "/";
    }

    server->sendContent(XML2);
    server->sendContent(href);
    server->sendContent(XML3);

    // Provide some dummy dates for simplicity, or get from file if available
    sendContentProp(server, "getlastmodified", "Thu, 01 Jan 1970 00:00:00 GMT");
    sendContentProp(server, "creationdate", "1970-01-01T00:00:00Z");

    if (file.isDirectory()) {
        server->sendContent("<D:resourcetype><D:collection/></D:resourcetype>");
    } else {
        server->sendContent("<D:resourcetype/>");
        char fsizeStr[32];
        snprintf(fsizeStr, sizeof(fsizeStr), "%u", file.size());
        sendContentProp(server, "getcontentlength", fsizeStr);
        sendContentProp(server, "getcontenttype", "application/octet-stream");
    }
    
    sendContentProp(server, "displayname", file.name());
    server->sendContent(XML4);
}

void webdav_server_init(WebServer* server) {
    // We bind a catch-all handler for the /webdav prefix
    server->onNotFound([server]() {
        if (!appSettings.webDavEnabled) {
            server->send(403, "text/plain", "WebDAV Disabled");
            return;
        }

        String uri = server->uri();
        if (!uri.startsWith(WEBDAV_PREFIX)) {
            server->send(404, "text/plain", "Not Found");
            return;
        }

        // Map /webdav to /recordings on SD Card
        String sdPath = uri.substring(strlen(WEBDAV_PREFIX));
        if (sdPath == "" || sdPath == "/") {
            sdPath = "/recordings";
        } else {
            sdPath = "/recordings" + sdPath;
        }

        int method = server->method();

        if (method == HTTP_OPTIONS) {
            server->sendHeader("Allow", "OPTIONS, PROPFIND, GET");
            server->sendHeader("DAV", "1");
            server->send(200);
            return;
        }

        if (method == HTTP_ANY) { // WebServer maps PROPFIND to HTTP_ANY if unknown
            // Actually, Arduino WebServer might not expose method enum for PROPFIND.
            // We check method string or just assume PROPFIND if not GET.
        }

        // Simple GET (Download)
        if (method == HTTP_GET) {
            if (!SD_MMC.exists(sdPath)) {
                server->send(404, "text/plain", "File Not Found");
                return;
            }
            File f = SD_MMC.open(sdPath, "r");
            if (!f || f.isDirectory()) {
                server->send(400, "text/plain", "Invalid File");
                if (f) f.close();
                return;
            }
            server->streamFile(f, "application/octet-stream");
            f.close();
            return;
        }

        // Handle PROPFIND (method() == HTTP_ANY, method string usually handled internally)
        // Arduino's WebServer enum has HTTP_PROPFIND? Wait, it depends on the ESP32 core version.
        // Let's just respond to PROPFIND. We check if the request body or headers imply PROPFIND.
        
        if (!SD_MMC.exists(sdPath)) {
            server->send(404, "text/plain", "Not Found");
            return;
        }

        File root = SD_MMC.open(sdPath);
        if (!root) {
            server->send(500, "text/plain", "FS Error");
            return;
        }

        server->setContentLength(CONTENT_LENGTH_UNKNOWN);
        server->send(207, "application/xml;charset=utf-8", XML1);
        
        // Return details for the requested path itself
        String basePath = uri.substring(strlen(WEBDAV_PREFIX));
        if (basePath != "" && !basePath.endsWith("/")) basePath += "/";
        
        sendPropResponse(server, root, "/"); // The root folder
        
        // If Depth: 1 (or default), list children
        String depth = server->header("Depth");
        if (depth != "0" && root.isDirectory()) {
            File entry = root.openNextFile();
            while (entry) {
                sendPropResponse(server, entry, basePath);
                entry.close();
                entry = root.openNextFile();
            }
        }
        
        root.close();
        server->sendContent("</D:multistatus>");
        server->sendContent(""); // End chunked response
    });
}
