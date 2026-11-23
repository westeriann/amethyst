// Offline test bench for the beat clock and the song-structure follower.
// amethyst-analyze <file.wav> [out.png]  -> prints tempo + events, draws a timeline.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "structure.h"
#include "tempo.h"
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DEVICE_IO
#include <miniaudio/miniaudio.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// decode any wav/flac/mp3 to 48 kHz stereo float
static bool decode(const char* path, std::vector<float>& out) {
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 2, 48000);
    ma_decoder dec;
    if (ma_decoder_init_file(path, &cfg, &dec) != MA_SUCCESS) return false;
    float buf[4096 * 2];
    ma_uint64 got = 0;
    while (ma_decoder_read_pcm_frames(&dec, buf, 4096, &got) == MA_SUCCESS && got > 0) out.insert(out.end(), buf, buf + got * 2);
    ma_decoder_uninit(&dec);
    return !out.empty();
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: amethyst-analyze file.wav [out.png] [--tail seconds-of-silence]\n"); return 1; }
    std::vector<float> src;
    if (!decode(argv[1], src)) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    size_t n = src.size() / 2;
    std::vector<float> s((n + 48000 * 4) * 2, 0.f);
    std::copy(src.begin(), src.end(), s.begin() + 48000 * 2);
    Tempo tempo;
    MusicStructure ms;
    const int blk = 480;
    size_t frames = s.size() / 2;
    double dur = frames / 48000.0;
    // plot: 10 px per second
    int W = (int)(dur * 10) + 1, H = 300;
    std::vector<uint8_t> img(W * H * 3, 18);
    auto px = [&](int x, int y, int r, int g, int b) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        uint8_t* p = &img[(y * W + x) * 3];
        p[0] = r, p[1] = g, p[2] = b;
    };
    auto vline = [&](int x, int y0, int y1, int r, int g, int b) { for (int y = y0; y < y1; y++) px(x, y, r, g, b); };
    double lp = 0, bassAcc = 0, allAcc = 0;
    int acc = 0;
    double lastBpmPrint = -100;
    long long lastBar = -1;
    for (size_t f0 = 0; f0 + blk <= frames; f0 += blk) {
        double t = f0 / 48000.0;
        const float* p = &s[f0 * 2];
        tempo.feed(p, blk, t + blk / 48000.0);
        tempo.update(t);
        double beat = tempo.beatAt(t), bpm = tempo.bpm();
        ms.feed(p, blk, t, beat, bpm);
        for (int i = 0; i < blk; i++) {
            float m = 0.5f * (p[i * 2] + p[i * 2 + 1]);
            lp += 0.0194 * (m - lp);
            bassAcc += lp * lp, allAcc += m * m;
        }
        if ((acc += blk) >= 4800) {  // one plot column per 0.1 s
            int x = (int)(t * 10);
            int yb = H - 40 - (int)(std::sqrt(bassAcc / acc) * 160), ya = H - 40 - (int)(std::sqrt(allAcc / acc) * 160);
            vline(x, std::max(60, ya), H - 40, 70, 70, 110);
            vline(x, std::max(60, yb), H - 40, 230, 120, 60);
            // section strip
            static const uint8_t col[6][3] = {{40, 40, 40}, {90, 160, 90}, {120, 120, 200}, {60, 110, 250}, {250, 220, 60}, {250, 60, 60}};
            auto c = col[ms.section()];
            vline(x, H - 30, H - 10, c[0], c[1], c[2]);
            // bpm trace (60..200 mapped to rows 10..50)
            px(x, 50 - (int)((bpm - 60) / 140 * 40), 200, 200, 200);
            bassAcc = allAcc = 0;
            acc = 0;
        }
        long long bar = (long long)std::floor((beat - 0) / 4);
        if (bar != lastBar) {
            lastBar = bar;
            int x = (int)(t * 10);
            bool phrase = ms.barInPhrase(beat) == 0;
            vline(x, 52, phrase ? 70 : 58, phrase ? 255 : 120, phrase ? 255 : 120, phrase ? 255 : 120);
        }
        for (auto& e : ms.take()) {
            printf("%7.2fs  beat %7.2f  %-10s  bpm %.1f\n", e.time - 1.0, e.beat, MusicStructure::name(e.type), bpm);
            int x = (int)(e.time * 10);
            int r = 255, g = 255, b = 255;
            if (e.type == MusicStructure::DropHit) r = 255, g = 40, b = 40;
            if (e.type == MusicStructure::BreakdownStart) r = 60, g = 120, b = 255;
            vline(x, 60, H - 30, r, g, b);
            vline(x + 1, 60, H - 30, r, g, b);
        }
        if (t - lastBpmPrint >= 30) {
            lastBpmPrint = t;
            printf("%7.2fs  tempo %.1f  conf %.1f  section %s\n", t - 1.0, bpm, tempo.confidence(), MusicStructure::name(ms.section()));
        }
    }
    if (argc > 2) stbi_write_png(argv[2], W, H, 3, img.data(), W * 3);
    return 0;
}
