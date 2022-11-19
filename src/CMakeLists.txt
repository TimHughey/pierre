
#
# Source Directories
#

include(FetchContent)

FetchContent_Declare(ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v6.19.4
)

FetchContent_Declare(InfluxData
    GIT_REPOSITORY https://github.com/offa/influxdb-cxx
    GIT_TAG        822e42254288387f5077d2c60157391c6384bd97
)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.2.0
)

FetchContent_MakeAvailable(ArduinoJson InfluxData tomlplusplus)

# common types, logging, helpers and shared session key
add_subdirectory(base)

# config
add_subdirectory(config)

# asio wrapper
add_subdirectory(io)

# influx-cxx wrapper
add_subdirectory(stats)

# mdns brought to you by Avahi
add_subdirectory(mdns)

# top-level for all comms (RTSP, RTP, Buffered Audio, Control, Event)
add_subdirectory(airplay)

# Frame of processed audio, master clock and anchor
add_subdirectory(frame)

# faders
add_subdirectory(fader)

# desk (head units, FX
add_subdirectory(desk)
