/*
 * arch-01-list-devices
 *
 * Proves: miniaudio can see this machine's audio backend and enumerate every
 * playback and capture device, including the PulseAudio/PipeWire ".monitor"
 * sources we need for passive output capture (loopback on Linux).
 *
 * Expected on this Arch box: backend "PulseAudio", and at least one capture
 * device whose name contains "monitor".
 */
#include <stdio.h>
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

static int is_monitor(const char *name) {
    return strstr(name, "monitor") != NULL || strstr(name, "Monitor") != NULL;
}

int main(void) {
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio context.\n");
        return 1;
    }
    printf("Backend: %s\n\n", ma_get_backend_name(context.backend));

    ma_device_info *pPlayback, *pCapture;
    ma_uint32 nPlayback, nCapture;
    if (ma_context_get_devices(&context, &pPlayback, &nPlayback,
                               &pCapture, &nCapture) != MA_SUCCESS) {
        fprintf(stderr, "Failed to enumerate devices.\n");
        ma_context_uninit(&context);
        return 1;
    }

    printf("Playback devices (%u):\n", nPlayback);
    for (ma_uint32 i = 0; i < nPlayback; i++) {
        printf("  [%u] %s%s\n", i, pPlayback[i].name,
               pPlayback[i].isDefault ? "  (default)" : "");
    }

    int monitors = 0;
    printf("\nCapture devices (%u):\n", nCapture);
    for (ma_uint32 i = 0; i < nCapture; i++) {
        int mon = is_monitor(pCapture[i].name);
        monitors += mon;
        printf("  [%u] %s%s%s\n", i, pCapture[i].name,
               pCapture[i].isDefault ? "  (default)" : "",
               mon ? "  <-- MONITOR (system output tap)" : "");
    }

    printf("\nMonitor capture sources found: %d %s\n", monitors,
           monitors > 0 ? "=> passive output capture is available"
                        : "=> NO monitor source; loopback needs another path");

    ma_context_uninit(&context);
    return 0;
}
