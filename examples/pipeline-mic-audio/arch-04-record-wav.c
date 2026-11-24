/*
 * arch-04-record-wav
 *
 * Objective capture proof. Records a chosen device to a WAV file and prints
 * hard numbers: frames captured, duration, peak, RMS, and the share of
 * non-silent samples. "Bars moving" is subjective; a WAV with RMS > 0 that you
 * can play back is not.
 *
 * Usage:
 *   arch-04-record-wav <device-substring|default> <seconds> <out.wav>
 *
 * Examples:
 *   arch-04-record-wav default 5 /tmp/mic.wav              # mic
 *   arch-04-record-wav "monitor of ryzen" 5 /tmp/out.wav   # system output tap
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#define SAMPLE_RATE 16000
#define CHANNELS    1

static float      *g_buf = NULL;
static size_t      g_cap_frames = 0;   /* capacity in frames */
static size_t      g_pos = 0;          /* frames written */

static void on_data(ma_device *pDevice, void *pOutput, const void *pInput,
                    ma_uint32 frameCount) {
    (void)pDevice;
    (void)pOutput;
    const float *in = (const float *)pInput;
    size_t room = g_cap_frames - g_pos;
    size_t n = frameCount < room ? frameCount : room;
    if (n) {
        memcpy(g_buf + g_pos, in, n * sizeof(float) * CHANNELS);
        g_pos += n;
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <device-substring|default> <seconds> <out.wav>\n",
                argv[0]);
        return 2;
    }
    const char *match = argv[1];
    int seconds = atoi(argv[2]);
    const char *out = argv[3];
    if (seconds <= 0) seconds = 5;

    g_cap_frames = (size_t)SAMPLE_RATE * seconds;
    g_buf = calloc(g_cap_frames, sizeof(float) * CHANNELS);
    if (!g_buf) { fprintf(stderr, "oom\n"); return 1; }

    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        fprintf(stderr, "context init failed\n"); return 1;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.format    = ma_format_f32;
    cfg.capture.channels  = CHANNELS;
    cfg.sampleRate        = SAMPLE_RATE;
    cfg.capture.shareMode = ma_share_mode_shared;
    cfg.dataCallback      = on_data;

    ma_device_id chosen_id;
    char chosen_name[256];
    snprintf(chosen_name, sizeof chosen_name, "(default capture)");

    if (strcasecmp(match, "default") != 0) {
        ma_device_info *pPlay, *pCap; ma_uint32 nPlay, nCap;
        if (ma_context_get_devices(&context, &pPlay, &nPlay, &pCap, &nCap)
            != MA_SUCCESS) {
            fprintf(stderr, "enumerate failed\n");
            ma_context_uninit(&context); return 1;
        }
        int found = 0;
        for (ma_uint32 i = 0; i < nCap && !found; i++) {
            if (strcasestr(pCap[i].name, match)) {
                chosen_id = pCap[i].id;
                snprintf(chosen_name, sizeof chosen_name, "%s", pCap[i].name);
                found = 1;
            }
        }
        if (!found) {
            fprintf(stderr, "no capture device matching \"%s\"\n", match);
            ma_context_uninit(&context); return 1;
        }
        cfg.capture.pDeviceID = &chosen_id;
    }

    ma_device device;
    if (ma_device_init(&context, &cfg, &device) != MA_SUCCESS) {
        fprintf(stderr, "device init failed\n");
        ma_context_uninit(&context); return 1;
    }
    printf("Device : %s\n", chosen_name);
    printf("Format : %d Hz, %u ch, f32, shared\n", device.sampleRate,
           device.capture.channels);
    printf("Recording %d s -> %s\n", seconds, out);

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "start failed\n");
        ma_device_uninit(&device); ma_context_uninit(&context); return 1;
    }
    struct timespec ts = {seconds, 0};
    nanosleep(&ts, NULL);
    ma_device_stop(&device);
    ma_device_uninit(&device);
    ma_context_uninit(&context);

    /* stats */
    double peak = 0, sumsq = 0; size_t nonzero = 0;
    for (size_t i = 0; i < g_pos; i++) {
        float v = g_buf[i];
        float a = fabsf(v);
        if (a > peak) peak = a;
        sumsq += (double)v * v;
        if (a > 1e-4f) nonzero++;
    }
    double rms = g_pos ? sqrt(sumsq / g_pos) : 0.0;
    double nonzero_pct = g_pos ? (100.0 * nonzero / g_pos) : 0.0;

    /* write WAV */
    ma_encoder_config ec = ma_encoder_config_init(ma_encoding_format_wav,
                                                  ma_format_f32, CHANNELS,
                                                  SAMPLE_RATE);
    ma_encoder enc;
    if (ma_encoder_init_file(out, &ec, &enc) == MA_SUCCESS) {
        ma_uint64 written = 0;
        ma_encoder_write_pcm_frames(&enc, g_buf, g_pos, &written);
        ma_encoder_uninit(&enc);
    } else {
        fprintf(stderr, "warning: could not write WAV\n");
    }

    printf("\n--- RESULT ---\n");
    printf("frames captured : %zu (%.2f s)\n", g_pos,
           (double)g_pos / SAMPLE_RATE);
    printf("peak            : %.4f\n", peak);
    printf("RMS             : %.6f\n", rms);
    printf("non-silent      : %.1f%% of samples\n", nonzero_pct);
    printf("verdict         : %s\n",
           rms > 1e-4 ? "SIGNAL CAPTURED (real audio)"
                      : "SILENCE (no signal reached the device)");

    free(g_buf);
    return rms > 1e-4 ? 0 : 3;
}
