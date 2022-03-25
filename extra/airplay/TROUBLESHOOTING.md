## Troubleshooting

### Buffer underflow to audio backend

**Problem **

Audio may seem to pause or drop for several seconds. iOS devices may regularly disconnect altogether and display an error message. This may be caused by a combination of factors listed above such as slow WiFi or limited resources on the device running Shairport Sync.

**Possible Solution **

If none of the above steps completely remove the issue, try increasing the audio backend buffer setting in the backend section of shairport-sync.conf. (This section may vary depending on the value of the "output_backend" setting.)

For example:

```
audio_backend_buffer_desired_length = 19845;
```

Is triple the default for the ALSA backend and effectively solves the above issue with a Pi Zero on a busy network.
