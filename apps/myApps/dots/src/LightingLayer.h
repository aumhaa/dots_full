//
//  LightingLayer.h
//  dots
//
//  Up to four spotlights over the final picture (flames and MilkDrop mixed).
//  An "add" light lays coloured light on top; a "reveal" light lights the
//  picture under it while everything outside the reveal beams drops to the
//  ambient level, like a follow-spot. The lighting is its own layer, kept
//  apart from tracks and scenes, with numbered snapshots for each venue.
//

#pragma once

#include "ofMain.h"
#include "ofxXmlSettings.h"

#define LIGHTING_N_LIGHTS 4
#define LIGHTING_MODE_ADD 0
#define LIGHTING_MODE_REVEAL 1

// What the second Syphon feed (B) shows
#define LIGHTING_FEED_B_FLAMES 0     // the flames, masked by the light
#define LIGHTING_FEED_B_MILKDROP 1   // MilkDrop, masked by the light
#define LIGHTING_FEED_B_LIGHT 2      // the light alone

struct Light {
    ofParameterGroup group;
    ofParameter<bool> on;
    ofParameter<int> mode;       // LIGHTING_MODE_ADD or LIGHTING_MODE_REVEAL
    ofParameter<float> intensity;
    ofParameter<float> hue, saturation;
    ofParameter<float> x, y;     // beam centre, 0-1 across the picture (y down)
    ofParameter<float> size;     // beam radius, as a fraction of the picture height
    ofParameter<float> focus;    // 1 is a hard edge, 0 a soft one
    ofParameter<float> frost;    // diffusion: a wider, softer glow
};

class LightingLayer {
public:
    // Parameters only; the shader is built on the first apply()
    void setup();

    // Feed A: the lit picture, or source itself when no light is on or A leaves the lights out
    ofFbo &apply(ofFbo &source);
    // Feed B: flames or MilkDrop masked by the light, or the light alone, as a top layer:
    // alpha is the amount of light (fully transparent when no light is on)
    ofFbo &renderFeedB(ofFbo &flames, ofFbo &milkdrop);
    bool feedBUsesMilkDrop();
    bool feedBUsesFlames();
    bool anyOn();

    void saveSnapshot(int slot);
    void loadSnapshot(int slot);

    ofParameterGroup parameters;
    ofParameter<float> ambient;     // how much of the picture shows outside reveal beams
    ofParameter<int> selected;      // light the preview's mouse moves when not near another (1-4)
    ofParameter<int> snapshot;      // slot for Save/Load (1-8)
    ofParameter<bool> feedAIncludesLights;
    ofParameter<int> feedBSource;   // LIGHTING_FEED_B_FLAMES, _MILKDROP or _LIGHT
    Light lights[LIGHTING_N_LIGHTS];

private:
    void render(ofFbo &source, ofFbo &target, int outputMode);

    ofShader shader;
    ofFbo result, feedB;
    bool feedBWasLit = true;
};
