#pragma once

#include "config.h"

// MQTT is always compiled in, but runtime-enabled via appSettings.mqttEnabled
void init_mqtt();
void handle_mqtt();
void publish_ha_discovery();
void publish_status();
void mqtt_publish_motion(bool motion_detected);
bool mqtt_is_connected();
