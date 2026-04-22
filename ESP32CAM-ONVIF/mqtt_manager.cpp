#include "mqtt_manager.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "camera_control.h"
#include "auto_flash.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static unsigned long last_status_publish = 0;
static const unsigned long STATUS_INTERVAL = 60000; // 60 seconds

static String clientId = "";
static String topic_base = "";
static String state_topic = "";
static String cmd_topic = "";
static String motion_topic = "";
static String status_topic = "";
static bool mqtt_initialized = false;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    Serial.printf("[MQTT] Message arrived on topic: %s. Message: %s\n", topic, msg.c_str());

    if (String(topic) == cmd_topic) {
        if (msg == "ON") {
            set_flash_led(true);
            mqttClient.publish(state_topic.c_str(), "ON", true);
        } else if (msg == "OFF") {
            set_flash_led(false);
            mqttClient.publish(state_topic.c_str(), "OFF", true);
        }
    }
}

void mqtt_reconnect() {
    if (!mqttClient.connected()) {
        Serial.print("[MQTT] Attempting connection...");
        if (mqttClient.connect(clientId.c_str(), appSettings.mqttUser, appSettings.mqttPassword, status_topic.c_str(), 1, true, "offline")) {
            Serial.println("connected!");
            mqttClient.publish(status_topic.c_str(), "online", true);
            mqttClient.subscribe(cmd_topic.c_str());
            publish_ha_discovery();
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again later");
        }
    }
}

void init_mqtt() {
    if (!appSettings.mqttEnabled) {
        Serial.println("[MQTT] Disabled in settings.");
        return;
    }
    if (strlen(appSettings.mqttBroker) == 0) {
        Serial.println("[MQTT] No broker configured, skipping.");
        return;
    }

    topic_base = String(MQTT_TOPIC_BASE);
    state_topic = topic_base + "/flash/state";
    cmd_topic = topic_base + "/flash/set";
    motion_topic = topic_base + "/motion";
    status_topic = topic_base + "/status";

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    clientId = "ESP32CAM_" + mac;

    mqttClient.setServer(appSettings.mqttBroker, appSettings.mqttPort);
    mqttClient.setCallback(mqtt_callback);
    mqttClient.setBufferSize(1024);
    mqtt_initialized = true;
    Serial.printf("[MQTT] Initialized. Broker: %s:%d\n", appSettings.mqttBroker, appSettings.mqttPort);
}

void handle_mqtt() {
    if (!appSettings.mqttEnabled || !mqtt_initialized) return;

    if (!mqttClient.connected()) {
        static unsigned long lastReconnectAttempt = 0;
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            mqtt_reconnect();
        }
    } else {
        mqttClient.loop();

        unsigned long now = millis();
        if (now - last_status_publish > STATUS_INTERVAL) {
            last_status_publish = now;
            publish_status();
        }
    }
}

void publish_ha_discovery() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String device_id = "esp32cam_" + mac;

    // Switch Entity for Flash
    String sw_topic = String(HA_DISCOVERY_PREFIX) + "/switch/" + device_id + "/flash/config";
    StaticJsonDocument<512> sw_doc;
    sw_doc["name"] = "ESP32-CAM Flash";
    sw_doc["unique_id"] = device_id + "_flash";
    sw_doc["cmd_t"] = cmd_topic;
    sw_doc["stat_t"] = state_topic;
    sw_doc["avty_t"] = status_topic;
    sw_doc["ic"] = "mdi:flashlight";
    JsonObject dev = sw_doc.createNestedObject("dev");
    dev["ids"] = device_id;
    dev["name"] = "ESP32-CAM";
    dev["mf"] = DEVICE_MANUFACTURER;
    dev["mdl"] = DEVICE_MODEL;
    dev["sw"] = FIRMWARE_VERSION;

    char sw_buffer[512];
    serializeJson(sw_doc, sw_buffer);
    mqttClient.publish(sw_topic.c_str(), sw_buffer, true);

    // Binary Sensor for Motion
    String bin_topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + device_id + "/motion/config";
    StaticJsonDocument<512> bin_doc;
    bin_doc["name"] = "ESP32-CAM Motion";
    bin_doc["unique_id"] = device_id + "_motion";
    bin_doc["stat_t"] = motion_topic;
    bin_doc["avty_t"] = status_topic;
    bin_doc["dev_cla"] = "motion";
    bin_doc["dev"] = dev;

    char bin_buffer[512];
    serializeJson(bin_doc, bin_buffer);
    mqttClient.publish(bin_topic.c_str(), bin_buffer, true);
    
    Serial.println("[MQTT] Published HA Discovery payloads");
}

void publish_status() {
    if (!mqttClient.connected()) return;
    StaticJsonDocument<256> doc;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["version"] = FIRMWARE_VERSION;
    
    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish((topic_base + "/info").c_str(), buffer);
}

void mqtt_publish_motion(bool motion_detected) {
    if (appSettings.mqttEnabled && mqtt_initialized && mqttClient.connected()) {
        mqttClient.publish(motion_topic.c_str(), motion_detected ? "ON" : "OFF");
    }
}

bool mqtt_is_connected() {
    return appSettings.mqttEnabled && mqtt_initialized && mqttClient.connected();
}
