/*
 * arch-05-multi-input
 *
 * Answers: "does it also capture if I have two input mics?"
 *
 * A single ma_device captures ONE source. To record several mics at once we
 * open ONE capture device per mic, each with its own callback, all in shared
 * mode (so nothing conflicts). This probe finds every real microphone input
 * (capture devices that are NOT monitors), opens them simultaneously, records
 * a few seconds, and reports per-device peak/RMS. Speak near each mic.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#define MAX_MICS    8
#define SAMPLE_RATE 16000

typedef struct {
    char   name[256];
    double peak;
    double sumsq;
    size_t count;
} mic_t;

static mic_t g_mics[MAX_MICS];

static void on_data(ma_device *pDevice, void *pOutput, const void *pInput,
                    ma_uint32 frameCount) {
    (void)pOutput;
    mic_t *m = (mic_t *)pDevice->pUserData;
    const float *in = (const float *)pInput;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float a = fabsf(in[i]);
        if (a > m->peak) m->peak = a;
        m->sumsq += (double)in[i] * in[i];
        m->count++;
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); /* unbuffered: see progress live */
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        fprintf(stderr, "context init failed\n"); return 1;
    }

    ma_device_info *pPlay, *pCap; ma_uint32 nPlay, nCap;
    if (ma_context_get_devices(&context, &pPlay, &nPlay, &pCap, &nCap)
        != MA_SUCCESS) {
        fprintf(stderr, "enumerate failed\n");
        ma_context_uninit(&context); return 1;
    }

    ma_device devices[MAX_MICS];
    int n = 0;
    for (ma_uint32 i = 0; i < nCap && n < MAX_MICS; i++) {
        if (strcasestr(pCap[i].name, "monitor")) continue; /* skip output taps */

        snprintf(g_mics[n].name, sizeof g_mics[n].name, "%s", pCap[i].name);
        g_mics[n].peak = 0; g_mics[n].sumsq = 0; g_mics[n].count = 0;

        ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
        cfg.capture.pDeviceID = &pCap[i].id;
        cfg.capture.format    = ma_format_f32;
        cfg.capture.channels  = 1;
        cfg.sampleRate        = SAMPLE_RATE;
        cfg.capture.shareMode = ma_share_mode_shared;
        cfg.dataCallback      = on_data;
        cfg.pUserData         = &g_mics[n];

        printf("  opening: %s ...\n", g_mics[n].name);
        if (ma_device_init(&context, &cfg, &devices[n]) != MA_SUCCESS) {
            fprintf(stderr, "  (could not open: %s)\n", g_mics[n].name);
            continue;
        }
        printf("  opened:  %s\n", g_mics[n].name);
        n++;
    }

    if (n == 0) { fprintf(stderr, "no microphone inputs found\n");
                  ma_context_uninit(&context); return 1; }

    printf("Opened %d microphone input(s) simultaneously:\n", n);
    for (int i = 0; i < n; i++) {
        printf("  [%d] %s\n", i, g_mics[i].name);
        ma_device_start(&devices[i]);
    }

    printf("\nSpeak near each mic... (6 s)\n");
    struct timespec ts = {6, 0};
    nanosleep(&ts, NULL);

    printf("\n--- PER-MIC RESULT ---\n");
    for (int i = 0; i < n; i++) {
        ma_device_stop(&devices[i]);
        double rms = g_mics[i].count ? sqrt(g_mics[i].sumsq / g_mics[i].count) : 0;
        printf("  [%d] %-45s peak=%.3f rms=%.5f  %s\n", i, g_mics[i].name,
               g_mics[i].peak, rms,
               rms > 1e-4 ? "SIGNAL" : "silent");
        ma_device_uninit(&devices[i]);
    }
    printf("\nIf two rows show SIGNAL, two mics were captured at the same time.\n");

    ma_context_uninit(&context);
    return 0;
}
