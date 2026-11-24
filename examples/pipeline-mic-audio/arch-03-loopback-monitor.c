/*
 * arch-03-loopback-monitor
 *
 * Proves THE key Linux doubt: we can passively capture system OUTPUT (what you
 * hear) without owning the audio path. On Linux miniaudio has no native
 * loopback device type, so we open the sink ".monitor" as a normal CAPTURE
 * device. Play any audio (music, a video) while this runs; the level should
 * move, and playback must NOT be interrupted.
 *
 * It auto-picks the first capture device whose name contains "monitor".
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
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        fprintf(stderr, "Failed to init context.\n");
        return 1;
    }

    ma_device_info *pPlayback, *pCapture;
    ma_uint32 nPlayback, nCapture;
    if (ma_context_get_devices(&context, &pPlayback, &nPlayback,
                               &pCapture, &nCapture) != MA_SUCCESS) {
        fprintf(stderr, "Failed to enumerate.\n");
        ma_context_uninit(&context);
        return 1;
    }

    ma_device_id monitor_id;
    char monitor_name[256] = {0};
    int found = 0;
    for (ma_uint32 i = 0; i < nCapture && !found; i++) {
        if (strstr(pCapture[i].name, "monitor") ||
            strstr(pCapture[i].name, "Monitor")) {
            monitor_id = pCapture[i].id;
            snprintf(monitor_name, sizeof monitor_name, "%s", pCapture[i].name);
            found = 1;
        }
    }
    if (!found) {
        fprintf(stderr, "No monitor capture source found. "
                        "Run arch-01 to inspect devices.\n");
        ma_context_uninit(&context);
        return 1;
    }
    printf("Tapping system output via: %s\n", monitor_name);

    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &monitor_id;
    cfg.capture.format    = ma_format_f32;
    cfg.capture.channels  = 1;
    cfg.sampleRate        = 16000;
    cfg.capture.shareMode = ma_share_mode_shared;
    cfg.dataCallback      = on_data;

    ma_device device;
    if (ma_device_init(&context, &cfg, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open monitor capture device.\n");
        ma_context_uninit(&context);
        return 1;
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start.\n");
        ma_device_uninit(&device);
        ma_context_uninit(&context);
        return 1;
    }

    printf("Play some audio now... (8 s)\n");
    for (int s = 0; s < 8; s++) {
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
        int bars = (int)(g_peak * 50.0f);
        printf("  out peak %.3f |", g_peak);
        for (int b = 0; b < bars; b++) putchar('#');
        putchar('\n');
        g_peak = 0.0f;
    }

    ma_device_uninit(&device);
    ma_context_uninit(&context);
    printf("Done. Bars moving + uninterrupted playback = passive loopback OK.\n");
    return 0;
}
