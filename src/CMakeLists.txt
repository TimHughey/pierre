
#
# Source Directories
#

include(FetchContent)
FetchContent_Declare(ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v6.19.3
)

FetchContent_MakeAvailable(ArduinoJson)

# common types, logging, helpers and shared session key
add_subdirectory(base)

# args, config, host, service
add_subdirectory(core)

# mdns brought to you by Avahi
add_subdirectory(mdns)

# top-level for all comms (RTSP, RTP, Buffered Audio, Control, Event)
add_subdirectory(airplay)

# generalized types to represent packets
add_subdirectory(packet)

# rtp time (PTP master clock and RTP anchor)
add_subdirectory(rtp_time)

# pkayer (aka queued)
add_subdirectory(player)

# add_subdirectory(dmx)
# add_subdirectory(lightdesk)
