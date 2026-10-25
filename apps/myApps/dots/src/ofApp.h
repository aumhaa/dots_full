#pragma once

#include "ofMain.h"
#include "ofxFft.h"
#include "GuiApp.h"
#include "appConstants.h"
#include "ofxSyphon.h"
#include "StateManager.h"
#include "ProjectMEngine.h"
#include "LightingLayer.h"
#include "ShaderEngine.h"
#include "PresetManager.h"
#include "Fader.h"
#include <functional>

extern "C" {
#include "flam3.h"
}

class MidiController;
struct GLFWwindow;

typedef struct pointPair {
    int idx;
    ofVec3f pt1, pt2;
    float ptSize, lineWidth;
    ofFloatColor color;
    float cpPixelsPerUnit;
    ofVec2f cpCenter;
} pointPair;

typedef struct flameSeq {
    long frameCreated, frameUpdated;
    unsigned short *xform_distribution;
} flameSeq;

class ofApp : public ofBaseApp{
    
public:
    
    void setup();
    void update();
    void draw();
    
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void audioIn(ofSoundBuffer & buffer);

    void guiUpdate();
    bool startSoundStream(int deviceIdx);

    void setFlameParameters(flam3_genome &renderCp, flam3_genome &cp, const int motionId);
    void renderFlame(flam3_genome *toRender, double *flameSamples, const int nSamples,
                     randctx *rc, unsigned short *xform_distribution, const int fuse = 20);

    void flameDraw();
    void drawFlames();
    bool flamesActive = true;   // false while paused, by hand or because MilkDrop fills the mix
    int appliedMaxFps = -1;
    void flameWander();
    void flameRotate();

    void handleKey(int key);

    void handleTrackChanged(int & trackIdx);
    void handleSceneChanged(int & sceneIdx);
    void handleWanderingChanged(bool & wandering);

    // After interpolation and to render
    flam3_genome *cp, renderCp;
    int nsamples;
    double *prevFlameSamples, *currFlameSamples;

    // Interpolate between flam3 cps smoothly during wander
    int frame, swapFrame;
    vector<flameSeq> flameSequences;

    // Transition between non-wander cps
    flam3_genome *cp2, renderCp2;
    unsigned short *static_xform_distribution1, *static_xform_distribution2;

    // Dot shader
    ofShader billboardShader;

    // To draw
    ofVboMesh lines;
    vector<pointPair> pointPairs;
    float *pointRadii, *lineWidths, *cpPixelsPerUnit;
    ofVec2f *cpCenter;
    double totDotPixels, totLinePixels;

    // Display Paremters
    int fullscreen;
    float mpx, mpy, mpxSmoothingFactor, mpySmoothingFactor;
    float audioEffectSize1, audioEffectSize2, audioEffectSize3, audioEffectSize4;
    float mmpx, mmpy;
    float baseSpeed, rmsSpeedMult;
    float pointRadiusAudioScaleAmt;
    float frameClearSpeed;
    float particleAlpha, basePointRadius, pointRadiusAudioScale;
    float maxLineLength;
    float saturationPct;
    float overallScale;

    // Audio mode
    int audioMode;
    int soundStreamDevice, nChannels;
    ofSoundStream soundStream;  // for input

    // Audio analysis
    int nFftBuckets;
    static const int audioBufferSize = 512;   // samples the FFT takes
    float *audioInput;
    float *fftOutput;
    float audioRMS, smoothedAudioRMS, rmsMultiple;
    float audioCentroid, centroidMaxBucket;
    ofxFft *fft;
    float fftDecayRate;

    // MilkDrop, mixed over the flames
    ProjectMEngine projectM;
    float crossfade;   // mixer: 0 all A, 1 all B

    // ISF shaders, shared with the GUI
    shared_ptr<ShaderEngine> shaderEngine;
    ofxSyphonServer shaderSyphonServer;   // "dots shader": the shader engine's own output

    // Presets, and work other windows hand over to run here, in this window's GL context
    std::unique_ptr<PresetManager> presets;
    vector<std::function<void()>> pendingActions;
    void post(std::function<void()> action) { pendingActions.push_back(action); }

    // Which engines work this frame, and each engine's picture
    bool milkdropActive = false, shaderActive = false;
    bool shaderAnimatedOnly = true;
    ofFbo &engineFbo(int engine);

    // Preset fades: values glide (fader); what can't fade live fades out from a frozen frame
    Fader fader;
    struct FrozenFrame {
        ofFbo frame;
        float start = 0, seconds = 0;
        float opacity() const;   // 1 when frozen, easing to 0 over the fade
    };
    FrozenFrame flamesFrozen, outputFrozen;
    ofFbo flamesFading;          // the flames with their frozen frame over them
    void freezeFlames(float seconds);
    void freezeOutput(float seconds);
    void freeze(FrozenFrame &frozen, ofFbo &source, float seconds);
    float appliedPresetDuration = -1;   // GUI settings last passed to projectM
    int appliedLockPreset = -1, appliedShuffle = -1;

    // The GUI window
    shared_ptr<GuiApp> gui;
    ofFbo visualsFbo;   // the flames, with their trails
    ofFbo outputFbo;    // flames and MilkDrop mixed: what the window, the GUI and Syphon show
    shared_ptr<ofTexture> visuals;
    ofxSyphonServer syphonServer;       // "dots": the final, lit picture
    ofxSyphonServer feedBSyphonServer;  // "dots B": flames or MilkDrop masked by the spotlights, or the light alone

    // This window (the viewer); dots keeps rendering while it's hidden
    GLFWwindow *viewerWindow;
    bool viewerShown;

    // Spotlights over the mixed picture, shared with the GUI and MIDI
    shared_ptr<LightingLayer> lighting;

    // State manager
    shared_ptr<StateManager> stateManager;

    // Sends parameter changes back over MIDI
    shared_ptr<MidiController> midi;

    std::queue<int> keyPresses;
    bool cmdKeyDown;
};

