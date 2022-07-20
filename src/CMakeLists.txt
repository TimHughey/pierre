
#
# Source Directories
#

# common types, logging, helpers and shared session key
add_subdirectory(base)

# args, host, service
add_subdirectory(core)

# config
add_subdirectory(config)

# asio wrapper
add_subdirectory(io)

# mdns brought to you by Avahi
add_subdirectory(mdns)

# top-level for all comms (RTSP, RTP, Buffered Audio, Control, Event)
add_subdirectory(airplay)

# rtp time (PTP master clock and RTP anchor)
add_subdirectory(rtp_time)

# Digital Signal Processing
add_subdirectory(dsp)

# player (aka queued)
add_subdirectory(player)

# faders
add_subdirectory(fader)

# desk (head units, FX 
add_subdirectory(desk)

