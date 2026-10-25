//
//  FlameRenderer.h
//  dots
//
//  Fractal flames (flam3 genomes, Electric Sheep's) rendered live on the GPU.
//  A flock of points runs flam3's chaos game in a vertex shader, each pass one
//  step of every point, its state kept between passes by transform feedback;
//  every step lands in a floating-point histogram (count and colour) that fades
//  a little each frame, so it builds up density over frames as flam3 builds it
//  over samples. A last pass tone-maps it as flam3 does: log density, gamma,
//  vibrancy, background.
//
//  The genome comes from the CPU each frame (flam3 rotates and interpolates it)
//  as a texture: a row per xform, its affine, post affine, colour, opacity,
//  probability and up to MAX_VARIATIONS variations with their parameters. The
//  variations the genebank uses are ported from flam3's variations.c; any other
//  falls back to linear, so a sheep still draws, if not exactly.
//

#pragma once

#include "ofMain.h"
extern "C" {
#include "flam3.h"
}

class FlameRenderer {
public:
    struct Settings {
        int points = 500000;        // the flock
        int passes = 8;             // steps of every point a frame, each one plotted
        float fade = 0.8f;          // how much of the histogram a frame keeps
        float exposure = 1;         // on top of the genome's brightness
        float gammaScale = 1;       // on the genome's gamma
        float paletteShift = 0;     // along the palette, wrapping
    };

    void setup(int width, int height);
    void resize(int width, int height);

    // Draws a frame of the genome, which must have had prepare_precalc_flags2
    void render(flam3_genome &genome, const Settings &settings);
    // Forget what's built up, for a jump to another genome
    void clear();

    ofFbo &getFbo() { return output; }

    // How many of the genome's variations the shader doesn't have (drawn as linear)
    static int unsupportedVariations(const flam3_genome &genome);

private:
    void allocatePoints(int count);
    void upload(flam3_genome &genome);

    ofShader iterate, toneMap;
    ofBufferObject points[2];
    ofVbo pointsVbo[2];
    int current = 0, pointCount = 0;
    ofFbo histogram, output;
    ofTexture xforms, palette;
    vector<float> xformData;
    ofMesh quad;
    uint32_t frame = 0;
    int numStandard = 0, finalRow = -1;
};
