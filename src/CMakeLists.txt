
#
# Source Directories
#

# common types, logging, helpers and shared session key
add_subdirectory(base)

# config
add_subdirectory(config)

# asio wrapper
add_subdirectory(io)

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
