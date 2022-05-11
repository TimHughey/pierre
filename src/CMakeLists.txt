
# args, config, host, service
add_subdirectory(core)

# mdns brought to you by Avahi
add_subdirectory(mdns)

# top-level for all comms (RTSP, RTP, Buffered Audio, Control, Event)
add_subdirectory(airplay)

# generalized types to represent packets
add_subdirectory(packet)



# add_subdirectory(anchor)


# add_subdirectory(misc)
# add_subdirectory(audio)
# add_subdirectory(dmx)
# add_subdirectory(lightdesk)
# add_subdirectory(mdns)
# add_subdirectory(rtsp)
# add_subdirectory(rtp)
# add_subdirectory(pcm)
# add_subdirectory(decouple)