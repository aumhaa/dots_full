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

class MidiController : public ofxMidiListener {
public:
    ~MidiController();
    void setup();

    void newMidiMessage(ofxMidiMessage& eventArgs);

    void handleWanderingChanged(bool & wandering);
    void handleClearSpeedChanged(float & clearSpeed);
    void handleOverallScaleChanged(float & overallScale);
    void handleBasePointRadiusChanged(float & basePointRadius);
    void handleMaxLineLengthChanged(float & maxLineLength);
    void handleAudioEffectSize1Changed(float & audioEffectSize1);
    void handleAudioEffectSize2Changed(float & audioEffectSize2);
    void handleAudioEffectSize3Changed(float & audioEffectSize3);
    void handleAudioEffectSize4Changed(float & audioEffectSize4);
    void handleBaseSpeedChanged(float & baseSpeed);
    void handleRmsSpeedMultChanged(float & rmsSpeedMult);
    void handleSaturationPctChanged(float & saturationPct);
    void handleParticleAlphaChanged(float & particleAlpha);
    void handleWanderSpeedChanged(float & wanderSpeed);
    void handleFftDecayRateChanged(float & fftDecayRate);
    void handleCentroidMaxBucketChanged(float & centroidMaxBucket);
    void handleRmsMultipleChanged(float & rmsMultiple);
    void handleMpxSmoothingFactorChanged(float & mpxSmoothingFactor);
    void handleMpySmoothingFactorChanged(float & mpySmoothingFactor);
    void handlePointRadiusAudioScaleAmtChanged(float & pointRadiusAudioScaleAmt);
    void handlePointRadiusAudioScaleChanged(float & pointRadiusAudioScale);
    
    void add_listeners();

    ofxMidiIn midiIn;
    ofxMidiOut midiOut;

    float sliders[20];
    float knobs[16];
    int buttons[16];

    
    bool last_wandering;
    float last_clearSpeed;
    float last_overallScale;
    float last_basePointRadius;
    float last_maxLineLength;
    float last_audioEffectSize1;
    float last_audioEffectSize2;
    float last_audioEffectSize3;
    float last_audioEffectSize4;
    float last_baseSpeed;
    float last_rmsSpeedMult;
    float last_saturationPct;
    float last_particleAlpha;
    float last_wanderSpeed;
    float last_fftDecayRate;
    float last_centroidMaxBucket;
    float last_rmsMultiple;
    float last_mpxSmoothingFactor;
    float last_mpySmoothingFactor;
    float last_pointRadiusAudioScaleAmt;
    float last_pointRadiusAudioScale;
    
    // State manager
    shared_ptr<StateManager> stateManager;
    
    //GUI App
    shared_ptr<GuiApp> gui;
};

#endif /* MidiController_h */



