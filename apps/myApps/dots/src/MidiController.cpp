//
//  MidiController.cpp
//  dotsLocal
//
//  Created by Christopher Anderson on 4/5/16.
//
//

#include "MidiController.h"
#include "DotsSections.h"

// Fastest rate parameter changes are sent back on DotsOutput
static const float SEND_INTERVAL = 1.0 / 20.0;

MidiController::~MidiController() {
    midiIn.closePort();
    midiIn.removeListener(this);
}

void MidiController::setup() {
//    vector<string> &ports = midiIn.getPortList();
//
//    // Auto select nanoKontrol if available
//    int port = 0;
//    for (int i = 0; i < ports.size(); ++i) {
//        cout << ports[i] << endl;
//        if (ports[i].find("IAC Driver Dots") != string::npos) {
//            cout << "Port found" << endl;
//            port = i;
//            break;
//        }
//        else {
//            cout << "Port not found" << endl;
//        }
//    }
//    midiIn.openPort(port);

    midiIn.openVirtualPort("DotsInput");
    midiOut.openVirtualPort("DotsOutput");

    midiIn.addListener(this);

    for (int i = 0; i <16; ++i) {
        sliders[i] = -1;
        buttons[i] = -1;
    }

}

void MidiController::add_listeners() {
    // The CCs here match the ones controllerToSceneParams() reads
    DotsScene &scene = stateManager->activeScene;
    outputs = {
        {0,  &stateManager->activeTrack.wanderSpeed,  twisterColorForSection("Wander")},
        {1,  &scene.fftDecayRate,                     twisterColorForSection("Audio Analysis")},
        {2,  &scene.centroidMaxBucket,                twisterColorForSection("Audio Analysis")},
        {3,  &scene.rmsMultiple,                      twisterColorForSection("Audio Analysis")},
        {4,  &scene.mpxSmoothingFactor,               twisterColorForSection("Audio Analysis")},
        {5,  &scene.mpySmoothingFactor,               twisterColorForSection("Audio Analysis")},
        {6,  &scene.clearSpeed,                       twisterColorForSection("Drawing")},
        {7,  &scene.particleAlpha,                    twisterColorForSection("Drawing")},
        {8,  &scene.overallScale,                     twisterColorForSection("Drawing")},
        {9,  &scene.saturationPct,                    twisterColorForSection("Drawing")},
        {10, &scene.baseSpeed,                        twisterColorForSection("Speed")},
        {11, &scene.rmsSpeedMult,                     twisterColorForSection("Speed")},
        {12, &scene.pointRadiusAudioScaleAmt,         twisterColorForSection("Dots")},
        {13, &scene.pointRadiusAudioScale,            twisterColorForSection("Dots")},
        {14, &scene.basePointRadius,                  twisterColorForSection("Dots")},
        {15, &scene.maxLineLength,                    twisterColorForSection("Lines")},
        {16, &scene.motionId,                         twisterColorForSection("Audio Effects")},
        {17, &scene.audioEffectSize1,                 twisterColorForSection("Audio Effects")},
        {18, &scene.audioEffectSize2,                 twisterColorForSection("Audio Effects")},
        {19, &scene.audioEffectSize3,                 twisterColorForSection("Audio Effects")},
        {20, &scene.audioEffectSize4,                 twisterColorForSection("Audio Effects")},
        {DOTS_CC_MILKDROP_MIX, &gui->milkdropMix,     twisterColorForSection("MilkDrop")},
        {DOTS_CC_LIGHTS_AMBIENT, &lighting->ambient,  twisterColorForSection("Lights")},
        {DOTS_CC_FEED_A_LIGHTS, &lighting->feedAIncludesLights, twisterColorForSection("Lights")},
        {DOTS_CC_FEED_B_SOURCE, &lighting->feedBSource, twisterColorForSection("Lights")},
    };
    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        Light &light = lighting->lights[i];
        ofAbstractParameter *params[DOTS_CCS_PER_LIGHT] = {&light.on, &light.mode, &light.intensity, &light.hue,
            &light.saturation, &light.x, &light.y, &light.size, &light.focus, &light.frost};
        for (int k = 0; k < DOTS_CCS_PER_LIGHT; ++k) {
            outputs.push_back({DOTS_CC_LIGHTS + i * DOTS_CCS_PER_LIGHT + k, params[k], twisterColorForSection("Lights")});
        }
    }
    for (int i = 0; i < 128; ++i) {
        sentValues[i] = -1;
    }
    lastSendTime = 0;
    // Send everything once at startup, in case a controller is already listening
    sendAllRequested = true;
}

// A parameter's position between its min and max, as a 0-127 MIDI value
static int midiValue(ofAbstractParameter *param) {
    float t = 0;
    if (param->type() == typeid(ofParameter<float>).name()) {
        ofParameter<float> &p = param->cast<float>();
        t = ofMap(p.get(), p.getMin(), p.getMax(), 0, 1, true);
    } else if (param->type() == typeid(ofParameter<int>).name()) {
        ofParameter<int> &p = param->cast<int>();
        t = ofMap(p.get(), p.getMin(), p.getMax(), 0, 1, true);
    } else if (param->type() == typeid(ofParameter<bool>).name()) {
        t = param->cast<bool>().get() ? 1 : 0;
    }
    return (int)(t * 127.0f);
}

// Sets a parameter from a 0-127 MIDI value across its min..max (a bool is on from 64)
static void setFromMidi(ofAbstractParameter *param, int value) {
    float t = value / 127.0f;
    if (param->type() == typeid(ofParameter<float>).name()) {
        ofParameter<float> &p = param->cast<float>();
        p.set(ofLerp(p.getMin(), p.getMax(), t));
    } else if (param->type() == typeid(ofParameter<int>).name()) {
        ofParameter<int> &p = param->cast<int>();
        p.set((int)round(ofLerp(p.getMin(), p.getMax(), t)));
    } else if (param->type() == typeid(ofParameter<bool>).name()) {
        param->cast<bool>().set(value >= 64);
    }
}

void MidiController::update() {
    float now = ofGetElapsedTimef();
    if (now - lastSendTime < SEND_INTERVAL) return;
    lastSendTime = now;

    // Asked for everything: resend all values, and the section colours on channel 2
    bool sendAll = sendAllRequested.exchange(false);
    if (sendAll) {
        for (const Output &output : outputs) {
            midiOut.sendControlChange(2, output.cc, output.twisterColor);
            sentValues[output.cc] = -1;
        }
    }

    // Only values that changed since they were last sent
    for (const Output &output : outputs) {
        int value = midiValue(output.param);
        if (value != sentValues[output.cc]) {
            sentValues[output.cc] = value;
            midiOut.sendControlChange(1, output.cc, value);
        }
    }
}

void MidiController::newMidiMessage(ofxMidiMessage& msg) {
//    cout << "Rcv midi: " + std::to_string(msg.status) + " " + std::to_string(msg.channel) + " " + std::to_string(msg.control) + " " + std::to_string(msg.pitch) << endl;
    if (msg.status == 176 && msg.channel == 1 && msg.control >= DOTS_CC_MILKDROP_MIX && msg.control != DOTS_CC_SEND_ALL) {
        // Everything past the scene parameters (MilkDrop, lights) goes by the outputs table
        for (const Output &output : outputs) {
            if (output.cc == msg.control) setFromMidi(output.param, msg.value);
        }
    } else if (msg.status == 144 && msg.channel == 1 && msg.pitch == DOTS_NOTE_PREV_PRESET && msg.velocity > 0) {
        milkdropPresetSteps--;
    } else if (msg.status == 144 && msg.channel == 1 && msg.pitch == DOTS_NOTE_NEXT_PRESET && msg.velocity > 0) {
        milkdropPresetSteps++;
    } else if (msg.status == 176 && msg.channel == 1 && msg.control >= 0 && msg.control < 24) {
        sliders[msg.control] = (float)msg.value/127.0;
        stateManager->controllerToSceneParams(msg.control, sliders[msg.control]);
    } else if (msg.status == 176 && msg.channel == 1 && msg.control == DOTS_CC_SEND_ALL) {
        sendAllRequested = true;
    } else if (msg.status == 144 && msg.channel == 1 && msg.pitch >=0 && msg.pitch <4 && msg.velocity > 0) {
        buttons[msg.pitch] = (float)msg.velocity/127.0;
//         cout << "Rcv note:" + std::to_string(msg.pitch) + std::to_string(msg.velocity) << endl;
        stateManager->noteToSceneParams(msg.pitch);
    } else if (msg.status == 144 && msg.channel == 1 && msg.pitch == 4 && msg.velocity > 0) {
        gui->wandering.set(!gui->wandering.get());
    }
}
