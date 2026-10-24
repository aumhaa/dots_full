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
#include "ofMain.h"
#include "ofxMidi.h"

// CC (channel 1) that asks dots to send all its values and section colours
#define DOTS_CC_SEND_ALL 127

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
    int sentValues[24];
    float lastSendTime;
    std::atomic<bool> sendAllRequested;

    // State manager
    shared_ptr<StateManager> stateManager;

    //GUI App
    shared_ptr<GuiApp> gui;
};

#endif /* MidiController_h */
