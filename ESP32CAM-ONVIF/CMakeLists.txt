FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/ESP32CAM-ONVIF/*.cpp)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES arduino
    PRIV_REQUIRES esp32-camera esp_h264
)
