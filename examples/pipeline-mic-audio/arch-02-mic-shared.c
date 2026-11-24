/*
 * arch-02-mic-shared
 *
 * Proves: we can capture the default microphone in SHARED mode (non-intrusive)
 * at 16 kHz mono f32 (whisper's target format), for ~5 seconds, printing a live
 * peak level. Run another app using the mic at the same time: both should keep
 * working, confirming we never take exclusive ownership.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

static volatile float g_peak = 0.0f;

static void on_data(ma_device *pDevice, void *pOutput, const void *pInput,
                    ma_uint32 frameCount) {
    (void)pDevice;
    (void)pOutput;
    const float *in = (const float *)pInput;
    float peak = 0.0f;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float a = in[i] < 0 ? -in[i] : in[i];
        if (a > peak) peak = a;
    }
    if (peak > g_peak) g_peak = peak;
}

int main(void) {
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.format   = ma_format_f32;
    cfg.capture.channels = 1;
    cfg.sampleRate       = 16000;
    cfg.capture.shareMode = ma_share_mode_shared; /* never exclusive */
    cfg.dataCallback     = on_data;

    ma_device device;
    if (ma_device_init(NULL, &cfg, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open capture device.\n");
        return 1;
    }
    printf("Capturing from: %s\n", device.capture.name);
    printf("Format: %d Hz, %u ch, f32, shared mode\n",
           device.sampleRate, device.capture.channels);

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start device.\n");
        ma_device_uninit(&device);
        return 1;
    }

    printf("Speak now... (5 s)\n");
    for (int s = 0; s < 5; s++) {
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
        int bars = (int)(g_peak * 50.0f);
        printf("  peak %.3f |", g_peak);
        for (int b = 0; b < bars; b++) putchar('#');
        putchar('\n');
        g_peak = 0.0f;
    }

    ma_device_uninit(&device);
    printf("Done. If you saw bars move while speaking, shared capture works.\n");
    return 0;
}
