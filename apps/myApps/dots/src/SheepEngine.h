//
//  SheepEngine.h
//  dots
//
//  Electric Sheep, rendered live: a sheep from the genebank (bin/data/genebank2.flam3,
//  the flock's genomes, already loaded for the flames) turns as a sheep loop does,
//  flam3 rotating its xforms a full turn every loop, and FlameRenderer draws each
//  frame of it on the GPU.
//
//  As the screensaver does, it moves from sheep to sheep by morphing: flam3
//  interpolates the two genomes, both still turning, over the morph time. A new
//  sheep chosen by hand morphs in; with auto advance on, the next one comes after
//  so many loops, in order or shuffled. The music can push it: the overall level
//  speeds the turning, bass twists the xforms further and pulses the zoom, the
//  level brightens it, and the high end shifts the colours along the palette.
//

#pragma once

#include "ofMain.h"
#include "FlameRenderer.h"

class SynAudio;

class SheepEngine {
public:
    SheepEngine();
    ~SheepEngine();

    // Needs the GL context; the genebank stays the StateManager's
    void setup(int width, int height, flam3_genome *genebank, int count, const SynAudio *audio = nullptr);
    void resize(int width, int height);
    void render();
    ofFbo &getFbo() { return renderer.getFbo(); }

    int getCount() const { return count; }
    int getIndex() const { return sheep; }
    string getName(int index) const;
    void select(int index);
    // Morphs to the sheep over the given seconds instead of the morph time (a preset's fade)
    void select(int index, float morphOverride);
    void next() { select(sheep + 1); }
    void previous() { select(sheep - 1); }
    string getStatus() const;

    // DOTS_TEST_SHEEP=folder: draws `count` sheep spread over the genebank, a second of each,
    // and saves each as a PNG with its number, unsupported variations and time per frame
    void testShots(const string &folder, int count);
    // DOTS_TEST_SHEEP_MORPH=folder: frames of a morph from one sheep to another
    void testMorph(const string &folder, int from, int to, int frames);

    ofParameterGroup parameters;
    ofParameter<int> sheep;
    ofParameter<float> loopSeconds;   // a full turn of the sheep
    ofParameter<float> morphSeconds;  // from one sheep to the next; 0 cuts
    ofParameter<bool> autoAdvance, shuffle;
    ofParameter<int> loopsPerSheep;
    ofParameter<int> pointsK;         // the flock, in thousands
    ofParameter<int> passes;          // steps of every point a frame
    ofParameter<float> trail;         // how much of the picture a frame keeps
    ofParameter<float> exposure, gammaScale;
    ofParameterGroup audioParameters;
    ofParameter<float> audioSpeed, audioTwist, audioZoom, audioBright, audioColour;

private:
    void advance();
    void build(float angle, float morph);   // this frame's genome into frame

    FlameRenderer renderer;
    const SynAudio *audio = nullptr;
    flam3_genome *genebank = nullptr;
    int count = 0;
    flam3_genome frame;      // this frame's genome
    flam3_genome pair[2];    // the two sheep of a morph, turned
    float phase = 0, morphTime = 0, morphDuration = 0, nextMorph = -1;
    int shownSheep = -1, morphFrom = -1, loopsDone = 0, unsupported = 0;
};
