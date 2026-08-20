import sys
import re

BOARD_MAP = {
    'CONFIG_BOARD_AI_THINKER_ESP32CAM': 'esp32cam',
    'CONFIG_BOARD_M5STACK_CAMERA': 'm5stack_camera',
    'CONFIG_BOARD_M5STACK_PSRAM': 'm5stack_camera', # Fallback
    'CONFIG_BOARD_M5STACK_WIDE': 'm5stack_camera',
    'CONFIG_BOARD_M5STACK_UNITCAM': 'm5stack_camera',
    'CONFIG_BOARD_TTGO_T_CAMERA': 'ttgo_t_camera',
    'CONFIG_BOARD_TTGO_T_JOURNAL': 'ttgo_t_camera',
    'CONFIG_BOARD_WROVER_KIT': 'esp_wrover_kit',
    'CONFIG_BOARD_ESP_EYE': 'esp_eye',
    'CONFIG_BOARD_FREENOVE_ESP32S3': 'freenove_esp32s3_cam',
    'CONFIG_BOARD_SEEED_XIAO_S3': 'seeed_xiao_esp32s3_sense',
    'CONFIG_BOARD_ESP32S3_EYE': 'esp32s3_eye',
    'CONFIG_BOARD_ESP32P4_FUNCTION_EV': 'esp32p4_function_ev',
    'CONFIG_BOARD_ESP32P4_ETH': 'esp32p4_eth',
    'CONFIG_BOARD_CUSTOM': 'esp32cam' # Default fallback
}

def parse_config(config_file):
    config = {}
    with open(config_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                if line.startswith('# CONFIG_') and line.endswith(' is not set'):
                    key = line[2:-11]
                    config[key] = 'false'
                continue
            if '=' in line:
                key, val = line.split('=', 1)
                if val == 'y':
                    val = 'true'
                elif val == 'n':
                    val = 'false'
                elif val.startswith('"') and val.endswith('"'):
                    pass
                elif val.isdigit() or (val.startswith('-') and val[1:].isdigit()):
                    pass
                else:
                    pass
                config[key] = val
    return config

def write_header(config, header_file):
    no_quote_keys = ['CONFIG_STATIC_IP_ADDR', 'CONFIG_STATIC_GATEWAY', 'CONFIG_STATIC_SUBNET', 'CONFIG_STATIC_DNS']

    with open(header_file, 'w') as f:
        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n\n")

        # Board Types mappings
        board_keys = [k for k in config.keys() if k.startswith('CONFIG_BOARD_') and k != 'CONFIG_BOARD_TYPE']
        selected_board = None
        for bk in board_keys:
            if config.get(bk) == 'true':
                selected_board = bk.replace('CONFIG_', '')
                break
        if selected_board:
            f.write(f"#define {selected_board}\n")

        # Codec mappings
        codec_keys = [k for k in config.keys() if k.startswith('CONFIG_VIDEO_CODEC_')]
        selected_codec = None
        for ck in codec_keys:
            if config.get(ck) == 'true':
                selected_codec = ck.replace('CONFIG_', '')
                break
        if selected_codec:
            f.write(f"#define {selected_codec}\n")

        # Encoder mappings
        encoder_keys = [k for k in config.keys() if k.startswith('CONFIG_H264_ENCODER_')]
        selected_encoder = None
        for ek in encoder_keys:
            if config.get(ek) == 'true':
                selected_encoder = ek.replace('CONFIG_', '')
                break
        if selected_encoder:
            f.write(f"#define {selected_encoder}\n")

        for key, val in config.items():
            if key.startswith('CONFIG_BOARD_') or key.startswith('CONFIG_VIDEO_CODEC_') or key.startswith('CONFIG_H264_ENCODER_'):
                continue
            if key == 'CONFIG_BLUETOOTH_ENABLED' and val == 'false':
                continue # Do not define BLUETOOTH_ENABLED at all if false
            define_name = key.replace('CONFIG_', '')
            if key in no_quote_keys and val.startswith('"') and val.endswith('"'):
                val = val[1:-1] # Strip quotes
            f.write(f"#define {define_name} {val}\n")

        f.write("\n")
        f.write("""
// Auto-detect Bluetooth stack based on chip
#ifdef BLUETOOTH_ENABLED
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
#define USE_BLUEDROID
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3)
#define USE_BLUEDROID
#elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(ESP32P4)
#define USE_BLUEDROID
#else
#define USE_NIMBLE
#endif
#endif

#if DEBUG_MODE
#define LOG_E(x) Serial.println("[ERROR] " + String(x))
#define LOG_I(x) if (DEBUG_LEVEL >= 2) Serial.println("[INFO] " + String(x))
#define LOG_D(x) if (DEBUG_LEVEL >= 3) Serial.println("[DEBUG] " + String(x))
#else
#define LOG_E(x)
#define LOG_I(x)
#define LOG_D(x)
#endif

// Fill empty strings for depends on to prevent errors
#ifndef MQTT_BROKER
#define MQTT_BROKER "192.168.1.100"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER "homeassistant"
#endif
#ifndef MQTT_PASS
#define MQTT_PASS "secret"
#endif
#ifndef MQTT_TOPIC_BASE
#define MQTT_TOPIC_BASE "home/camera/esp32cam"
#endif
#ifndef HA_DISCOVERY_PREFIX
#define HA_DISCOVERY_PREFIX "homeassistant"
#endif

#ifndef STATIC_IP_ADDR
#define STATIC_IP_ADDR 192, 168, 0, 150
#endif
#ifndef STATIC_GATEWAY
#define STATIC_GATEWAY 192, 168, 0, 1
#endif
#ifndef STATIC_SUBNET
#define STATIC_SUBNET 255, 255, 255, 0
#endif
#ifndef STATIC_DNS
#define STATIC_DNS 8, 8, 8, 8
#endif

enum AudioSource {
  AUDIO_SOURCE_NONE = 0,
  AUDIO_SOURCE_HARDWARE_I2S = 1,
  AUDIO_SOURCE_BLUETOOTH_HFP = 2
};

struct AppSettings {
  bool btEnabled;
  bool btStealthMode;
  char btPresenceMac[18];
  int btPresenceTimeout;
  int btMicGain;
  int hwMicGain;
  AudioSource audioSource;
  bool mqttEnabled;
  char mqttBroker[64];
  int mqttPort;
  char mqttUser[32];
  char mqttPassword[32];
  bool telegramEnabled;
  char telegramBotToken[64];
  char telegramChatId[32];
  bool googleDriveEnabled;
  bool googleDriveMotion;
  char googleDriveScriptUrl[128];
  bool webDavEnabled;
  bool continuousRecordingEnabled;
  int continuousRecordingChunkSize;
  char ntpServer[64];
  char timeZone[64];
};

void printBanner();
void fatalError(const char *msg);
extern AppSettings appSettings;
void loadSettings();
void saveSettings();
""")

def update_platformio_ini(config):
    board_keys = [k for k in config.keys() if k.startswith('CONFIG_BOARD_') and k != 'CONFIG_BOARD_TYPE']
    selected_env = 'esp32cam'
    for bk in board_keys:
        if config.get(bk) == 'true' and bk in BOARD_MAP:
            selected_env = BOARD_MAP[bk]
            break

    print(f"Updating platformio.ini to use environment: {selected_env}")

    with open('platformio.ini', 'r') as f:
        lines = f.readlines()

    with open('platformio.ini', 'w') as f:
        for line in lines:
            if line.startswith('default_envs'):
                f.write(f'default_envs = {selected_env}\n')
            else:
                f.write(line)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python gen_config.py <.config_file> <output_header>")
        sys.exit(1)
    config = parse_config(sys.argv[1])
    write_header(config, sys.argv[2])
    update_platformio_ini(config)
