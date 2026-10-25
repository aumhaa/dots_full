//
//  MidiController.h
//  dotsLocal
//
//  Created by Christopher Anderson on 4/5/16.
//
//

#ifndef MidiController_h
#define MidiController_h
#include "StateManager.h"
#include "GuiApp.h"
#include "LightingLayer.h"
#include "ofMain.h"
#include "ofxMidi.h"

// CC (channel 1) that asks dots to send all its values and section colours
#define DOTS_CC_SEND_ALL 127

// Mixer crossfade A-B (CC), MilkDrop previous/next preset (notes), all channel 1
#define DOTS_CC_CROSSFADE 21
#define DOTS_NOTE_PREV_PRESET 5
#define DOTS_NOTE_NEXT_PRESET 6

// Mixer engines on A and B (0-2 across the range), shader running, previous/next shader
#define DOTS_CC_ENGINE_A 27
#define DOTS_CC_ENGINE_B 28
#define DOTS_CC_SHADER_RUNNING 29
#define DOTS_CC_PRESET_FADE 30      // preset fade time, 0-10 s
#define DOTS_NOTE_PREV_SHADER 7
#define DOTS_NOTE_NEXT_SHADER 8

// Spotlights: ambient, then 10 CCs per light from DOTS_CC_LIGHTS (on, mode,
// intensity, hue, saturation, x, y, size, focus, frost), all channel 1
#define DOTS_CC_LIGHTS_AMBIENT 22
#define DOTS_CC_FEED_A_LIGHTS 23    // lights on Syphon feed A (on from 64)
#define DOTS_CC_FEED_B_SOURCE 24    // Syphon feed B: flames, MilkDrop or light (0-2 across the range)
#define DOTS_CC_FLAMES_RUNNING 25   // flames processing on/off (on from 64)
#define DOTS_CC_MILKDROP_RUNNING 26 // MilkDrop processing on/off (on from 64)
#define DOTS_CC_LIGHTS 32
#define DOTS_CCS_PER_LIGHT 10

class MidiController : public ofxMidiListener {
public:
    ~MidiController();
    void setup();

    void newMidiMessage(ofxMidiMessage& eventArgs);

    void add_listeners();

    // Call once a frame: sends parameters whose MIDI value changed, at most 20 times a second
    void update();

    ofxMidiIn midiIn;
    ofxMidiOut midiOut;

    float sliders[24];
    float knobs[16];
    int buttons[16];

    // One parameter sent back on CC `cc`, channel 1, scaled from its min..max to 0-127
    struct Output {
        int cc;
        ofAbstractParameter *param;
        int twisterColor;
    };
    vector<Output> outputs;
    int sentValues[128];
    float lastSendTime;
    std::atomic<bool> sendAllRequested;

    // MilkDrop preset changes asked for over MIDI (+ next, - previous); ofApp takes them each frame
    std::atomic<int> milkdropPresetSteps{0};
    std::atomic<int> shaderSteps{0};

    // State manager
    shared_ptr<StateManager> stateManager;

    //GUI App
    shared_ptr<GuiApp> gui;

    // Spotlights
    shared_ptr<LightingLayer> lighting;
};

#endif /* MidiController_h */
