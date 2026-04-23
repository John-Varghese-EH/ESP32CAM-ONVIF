#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>

void onvif_server_start();
void onvif_server_loop();
bool onvif_is_enabled();
void onvif_set_enabled(bool en);
void onvif_reconnect();
