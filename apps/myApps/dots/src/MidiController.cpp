//
//  MidiController.cpp
//  dotsLocal
//
//  Created by Christopher Anderson on 4/5/16.
//
//

#include "MidiController.h"

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
    last_wandering = false;
    stateManager->wandering.addListener(this, &MidiController::handleWanderingChanged);
    stateManager->activeScene.clearSpeed.addListener(this, &MidiController::handleClearSpeedChanged, 0);
    stateManager->activeScene.overallScale.addListener(this, &MidiController::handleOverallScaleChanged, 0);
    stateManager->activeScene.basePointRadius.addListener(this, &MidiController::handleBasePointRadiusChanged, 0);
    stateManager->activeScene.maxLineLength.addListener(this, &MidiController::handleMaxLineLengthChanged, 0);
    stateManager->activeScene.audioEffectSize1.addListener(this, &MidiController::handleAudioEffectSize1Changed, 0);
    stateManager->activeScene.audioEffectSize2.addListener(this, &MidiController::handleAudioEffectSize2Changed, 0);
    stateManager->activeScene.audioEffectSize3.addListener(this, &MidiController::handleAudioEffectSize3Changed, 0);
    stateManager->activeScene.audioEffectSize4.addListener(this, &MidiController::handleAudioEffectSize4Changed, 0);
    stateManager->activeScene.baseSpeed.addListener(this, &MidiController::handleBaseSpeedChanged, 0);
    stateManager->activeScene.rmsSpeedMult.addListener(this, &MidiController::handleRmsSpeedMultChanged, 0);
    stateManager->activeScene.saturationPct.addListener(this, &MidiController::handleSaturationPctChanged, 0);
    stateManager->activeScene.particleAlpha.addListener(this, &MidiController::handleParticleAlphaChanged, 0);
    stateManager->activeTrack.wanderSpeed.addListener(this, &MidiController::handleWanderSpeedChanged, 0);
    stateManager->activeScene.fftDecayRate.addListener(this, &MidiController::handleFftDecayRateChanged, 0);
    stateManager->activeScene.centroidMaxBucket.addListener(this, &MidiController::handleCentroidMaxBucketChanged, 0);
    stateManager->activeScene.rmsMultiple.addListener(this, &MidiController::handleRmsMultipleChanged, 0);
    stateManager->activeScene.mpxSmoothingFactor.addListener(this, &MidiController::handleMpxSmoothingFactorChanged, 0);
    stateManager->activeScene.mpySmoothingFactor.addListener(this, &MidiController::handleMpySmoothingFactorChanged, 0);
    stateManager->activeScene.pointRadiusAudioScaleAmt.addListener(this, &MidiController::handlePointRadiusAudioScaleAmtChanged, 0);
    stateManager->activeScene.pointRadiusAudioScale.addListener(this, &MidiController::handlePointRadiusAudioScaleChanged, 0);
    
}


void MidiController::handleWanderSpeedChanged(float & wanderSpeed) {
    if (wanderSpeed != last_wanderSpeed) {
        last_wanderSpeed = wanderSpeed;
        sliders[0] = ((float)(wanderSpeed)*127.0);
        midiOut.sendControlChange(1, 0, sliders[0]);
//        cout << "wanderSpeed:" + std::to_string(wanderSpeed) << endl;
    }
}

void MidiController::handleFftDecayRateChanged(float & fftDecayRate) {
    if (fftDecayRate != last_fftDecayRate) {
        last_fftDecayRate = fftDecayRate;
        sliders[1] = ((float)(fftDecayRate)*127.0);
        midiOut.sendControlChange(1, 1, sliders[1]);
        //        cout << "fftDecayRate:" + std::to_string(fftDecayRate) << endl;
    }
}

void MidiController::handleCentroidMaxBucketChanged(float & centroidMaxBucket) {
    if (centroidMaxBucket != last_centroidMaxBucket) {
        last_centroidMaxBucket = centroidMaxBucket;
        sliders[2] = ((float)(centroidMaxBucket)*127.0);
        midiOut.sendControlChange(1, 2, sliders[2]);
        //        cout << "centroidMaxBucket:" + std::to_string(centroidMaxBucket) << endl;
    }
}

void MidiController::handleRmsMultipleChanged(float & rmsMultiple) {
    if (rmsMultiple != last_rmsMultiple) {
        last_rmsMultiple = rmsMultiple;
        sliders[3] = ((float)(rmsMultiple/15.0)*127.0);
        midiOut.sendControlChange(1, 3, sliders[3]);
        //        cout << "rmsMultiple:" + std::to_string(rmsMultiple) << endl;
    }
}

void MidiController::handleMpxSmoothingFactorChanged(float & mpxSmoothingFactor) {
    if (mpxSmoothingFactor != last_mpxSmoothingFactor) {
        last_mpxSmoothingFactor = mpxSmoothingFactor;
        sliders[4] = ((float)(mpxSmoothingFactor)*127.0);
        midiOut.sendControlChange(1, 4, sliders[4]);
        //        cout << "mpxSmoothingFactor:" + std::to_string(mpxSmoothingFactor) << endl;
    }
}

void MidiController::handleMpySmoothingFactorChanged(float & mpySmoothingFactor) {
    if (mpySmoothingFactor != last_mpySmoothingFactor) {
        last_mpySmoothingFactor = mpySmoothingFactor;
        sliders[5] = ((float)(mpySmoothingFactor)*127.0);
        midiOut.sendControlChange(1, 5, sliders[5]);
        //        cout << "mpySmoothingFactor:" + std::to_string(mpySmoothingFactor) << endl;
    }
}

void MidiController::handleClearSpeedChanged(float & clearSpeed) {
    if (clearSpeed != last_clearSpeed) {
        last_clearSpeed = clearSpeed;
        sliders[6] = ((float)clearSpeed/255)*127.0;
        midiOut.sendControlChange(1, 6, sliders[6]);
//        cout << "clearSpead:" + std::to_string(clearSpeed) << endl;
    }
}

void MidiController::handleParticleAlphaChanged(float & particleAlpha) {
    if (particleAlpha != last_particleAlpha) {
        last_particleAlpha = particleAlpha;
        sliders[7] = ((float)(particleAlpha)/255.0)*127.0;
        midiOut.sendControlChange(1, 7, sliders[7]);
//        cout << "particleAlpha:" + std::to_string(particleAlpha) << endl;
    }
}

void MidiController::handleOverallScaleChanged(float & overallScale) {
    if (overallScale != last_overallScale) {
        last_overallScale = overallScale;
        sliders[8] = ((float)(overallScale-.1)/2.9)*127.0;
        midiOut.sendControlChange(1, 8, sliders[8]);
//        cout << "overallScale:" + std::to_string(overallScale) << endl;
    }
}

void MidiController::handleSaturationPctChanged(float & saturationPct) {
    if (saturationPct != last_saturationPct) {
        last_saturationPct = saturationPct;
        sliders[9] = ((float)(saturationPct)*127.0);
        midiOut.sendControlChange(1, 9, sliders[9]);
//        cout << "saturationPct:" + std::to_string(saturationPct) << endl;
    }
}

void MidiController::handleBaseSpeedChanged(float & baseSpeed) {
    if (baseSpeed != last_baseSpeed) {
        last_baseSpeed = baseSpeed;
        sliders[10] = ((float)(baseSpeed+5.0)/15.0)*127.0;
        midiOut.sendControlChange(1, 10, sliders[10]);
//        cout << "baseSpeed:" + std::to_string(baseSpeed) << endl;
    }
}

void MidiController::handleRmsSpeedMultChanged(float & rmsSpeedMult) {
    if (rmsSpeedMult != last_rmsSpeedMult) {
        last_rmsSpeedMult = rmsSpeedMult;
        sliders[11] = ((float)(rmsSpeedMult)/100.0)*127.0;
        midiOut.sendControlChange(1, 11, sliders[11]);
//        cout << "rmsSpeedMult:" + std::to_string(rmsSpeedMult) << endl;
    }
}

void MidiController::handlePointRadiusAudioScaleAmtChanged(float & pointRadiusAudioScaleAmt) {
    if (pointRadiusAudioScaleAmt != last_pointRadiusAudioScaleAmt) {
        last_pointRadiusAudioScaleAmt = pointRadiusAudioScaleAmt;
        sliders[12] = ((float)(pointRadiusAudioScaleAmt)*127.0);
        midiOut.sendControlChange(1, 12, sliders[12]);
        //        cout << "pointRadiusAudioScaleAmt:" + std::to_string(pointRadiusAudioScaleAmt) << endl;
    }
}

void MidiController::handlePointRadiusAudioScaleChanged(float & pointRadiusAudioScale) {
    if (pointRadiusAudioScale != last_pointRadiusAudioScale) {
        last_pointRadiusAudioScale = pointRadiusAudioScale;
        sliders[13] = ((float)(pointRadiusAudioScale/50.0)*127.0);
        midiOut.sendControlChange(1, 13, sliders[13]);
        //        cout << "pointRadiusAudioScale:" + std::to_string(pointRadiusAudioScale) << endl;
    }
}

void MidiController::handleBasePointRadiusChanged(float & basePointRadius) {
    if (basePointRadius != last_basePointRadius) {
        last_basePointRadius = basePointRadius;
        sliders[14] = ((float)(basePointRadius)/50.0)*127.0;
        midiOut.sendControlChange(1, 14, sliders[14]);
//        cout << "basePointRadius:" + std::to_string(basePointRadius) << endl;
    }
}

void MidiController::handleMaxLineLengthChanged(float & maxLineLength) {
    if (maxLineLength != last_maxLineLength) {
        last_maxLineLength = maxLineLength;
        sliders[15] = ((float)(maxLineLength)/3000.0)*127.0;
        midiOut.sendControlChange(1, 15, sliders[15]);
//        cout << "maxLineLength:" + std::to_string(maxLineLength) << endl;
    }
}

void MidiController::handleAudioEffectSize1Changed(float & audioEffectSize1) {
    if (audioEffectSize1 != last_audioEffectSize1) {
        last_audioEffectSize1 = audioEffectSize1;
        sliders[16] = (float)(audioEffectSize1)*127.0;
        midiOut.sendControlChange(1, 16, sliders[16]);
//        cout << "audioEffectSize1:" + std::to_string(audioEffectSize1) << endl;
    }
}

void MidiController::handleAudioEffectSize2Changed(float & audioEffectSize2) {
    if (audioEffectSize2 != last_audioEffectSize2) {
        last_audioEffectSize2 = audioEffectSize2;
        sliders[17] = (float)(audioEffectSize2)*127.0;
        midiOut.sendControlChange(1, 17, sliders[17]);
//        cout << "audioEffectSize2:" + std::to_string(audioEffectSize2) << endl;
    }
}

void MidiController::handleAudioEffectSize3Changed(float & audioEffectSize3) {
    if (audioEffectSize3 != last_audioEffectSize3) {
        last_audioEffectSize3 = audioEffectSize3;
        sliders[18] = (float)(audioEffectSize3)*127.0;
        midiOut.sendControlChange(1, 18, sliders[18]);
//        cout << "audioEffectSize3:" + std::to_string(audioEffectSize3) << endl;
    }
}

void MidiController::handleAudioEffectSize4Changed(float & audioEffectSize4) {
    if (audioEffectSize4 != last_audioEffectSize4) {
        last_audioEffectSize4 = audioEffectSize4;
        sliders[19] = (float)(audioEffectSize4)*127.0;
        midiOut.sendControlChange(1, 19, sliders[19]);
//        cout << "audioEffectSize4:" + std::to_string(audioEffectSize4) << endl;
    }
}

void MidiController::handleWanderingChanged(bool & wandering) {
    if (wandering != last_wandering){
        last_wandering = wandering;
//        cout << "handleWanderingChanged" + std::to_string(wandering) << endl;
    }
}

void MidiController::newMidiMessage(ofxMidiMessage& msg) {
//    cout << "Rcv midi: " + std::to_string(msg.status) + " " + std::to_string(msg.channel) + " " + std::to_string(msg.control) + " " + std::to_string(msg.pitch) << endl;
    if (msg.status == 176 && msg.channel == 1 && msg.control >= 0 && msg.control < 16) {
        sliders[msg.control] = (float)msg.value/127.0;
        stateManager->controllerToSceneParams(msg.control, sliders[msg.control]);   
    } else if (msg.status == 144 && msg.channel == 1 && msg.pitch >=0 && msg.pitch <4 && msg.velocity > 0) {
        buttons[msg.pitch] = (float)msg.velocity/127.0;
//         cout << "Rcv note:" + std::to_string(msg.pitch) + std::to_string(msg.velocity) << endl;
        stateManager->noteToSceneParams(msg.pitch);
    } else if (msg.status == 144 && msg.channel == 1 && msg.pitch == 4 && msg.velocity > 0) {
        gui->wandering.set(!gui->wandering.get());
    }
}






