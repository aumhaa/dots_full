#include "GuiApp.h"
#include "DotsSections.h"

// Tint each section the colour its knobs light up on the Twister
static void colorSections(ofxPanel &panel) {
    for (std::size_t i = 0; i < panel.getNumControls(); ++i) {
        ofxGuiGroup *group = dynamic_cast<ofxGuiGroup *>(panel.getControl(i));
        if (!group) continue;
        int twisterColor = twisterColorForSection(group->getName());
        if (!twisterColor) continue;

        group->setHeaderBackgroundColor(guiColorForTwister(twisterColor, 110));
        for (std::size_t j = 0; j < group->getNumControls(); ++j) {
            group->getControl(j)->setFillColor(guiColorForTwister(twisterColor, 170));
        }
    }
}

void GuiApp::setup() {
    ofSetEscapeQuitsApp(false);
    
//    midi.setup();

    ofSoundStream ss;
    soundDevices = ss.getDeviceList();
    nSoundDevices = soundDevices.size();

    audioInputParameters.setName("Audio Input");
    audioInputParameters.add(audioMode.set("audioMode", AUDIO_MODE_MIC, 0, N_AUDIO_MODES-1));
    audioInputParameters.add(soundStreamDevice.set("soundStreamDevice", 0, 0, nSoundDevices-1));
    inputGui.setup(audioInputParameters);

    // Start on the MacBook's built-in mic, else the system's default input
    int macbookMic = -1, defaultInput = -1;
    for (int i = 0; i < soundDevices.size(); ++i) {
        const ofSoundDevice &sd = soundDevices[i];
        if (sd.inputChannels > 0 && sd.name.find("MacBook") != string::npos && sd.name.find("Microphone") != string::npos) {
            macbookMic = i;
        }
        if (sd.isDefaultInput) {
            defaultInput = i;
        }
    }
    if (macbookMic >= 0) {
        soundStreamDevice.set(macbookMic);
    } else if (defaultInput >= 0) {
        soundStreamDevice.set(defaultInput);
    }

    // Auto select loopback if available
    for (int i = 0; i < soundDevices.size(); ++i) {
        ofSoundDevice &sd = soundDevices[i];
        if (sd.name.find("Soundflower") != string::npos && sd.name.find("2ch") != string::npos) {
            soundStreamDevice.set(i);
        }
    }

    // Debug
    debugParameters.setName("Debug");
    debugParameters.add(drawMode.set("drawMode", 0, 0, 1));
    debugParameters.add(nFlameSequences.set("nFlameSequences", 5, 1, 10));
    debugParameters.add(maxPixels.set("maxPixels", 5000000, 10000, 100000000));
    debugParameters.add(pctToAllowRandom.set("pctToAllowRandom", 0.02, 0, 0.1));
    debugParameters.add(useSavedParams.set("Use saved params", true));
    debugGui.setup(debugParameters);

    // Meta
    metaParams.setName("Meta");
    metaParams.add(wandering.set("wandering", false));
    metaParams.add(fadeFrames.set("fadeFrames", 100, 0, 200));
    metaParams.add(showViewer.set("show viewer (v)", false));
    metaParams.add(showLibrary.set("show library (l)", true));
    metaParams.add(flamesRunning.set("flames running", true));
    metaParams.add(maxFps.set("max fps", 30, 10, 60));
    metaGui.setup(metaParams);

    // MilkDrop
    milkdropParams.setName("MilkDrop");
    milkdropParams.add(milkdropRunning.set("running", true));
    milkdropParams.add(milkdropPresetDuration.set("presetSeconds", 30, 5, 120));
    milkdropParams.add(milkdropLockPreset.set("lockPreset", false));
    milkdropParams.add(milkdropShuffle.set("shuffle", true));
    milkdropGui.setup(milkdropParams);
    milkdropGui.add(prevPresetBtn.setup("Prev preset  ["));
    milkdropGui.add(nextPresetBtn.setup("Next preset  ]"));
    prevPresetBtn.addListener(this, &GuiApp::prevPresetPressed);
    nextPresetBtn.addListener(this, &GuiApp::nextPresetPressed);

    trackControlGui.setup("Tracks");

    trackControls.setup();
    trackControls.setName("< >");
    trackControls.add(prevTrackBtn.setup("Prev track"));
    trackControls.add(nextTrackBtn.setup("Next track"));
    trackControls.add(saveTrackBtn.setup("Save track"));
    trackControls.add(newTrackBtn.setup("New track"));
    trackControlGui.add(&trackControls);

    prevTrackBtn.addListener(stateManager.get(), &StateManager::regressTrack);
    nextTrackBtn.addListener(stateManager.get(), &StateManager::advanceTrack);
    saveTrackBtn.addListener(stateManager.get(), &StateManager::saveTrack);
    newTrackBtn.addListener(stateManager.get(),  &StateManager::createTrack);

    sceneControls.setup();
    sceneControls.setName("Scenes");
    sceneControls.add(duplicateSceneBtn.setup("Duplicate scene"));
    sceneControls.add(reloadSceneBtn.setup("Reload scene"));
    sceneControls.add(deleteSceneBtn.setup("Remove scene"));
    trackControlGui.add(&sceneControls);

    duplicateSceneBtn.addListener(stateManager.get(), &StateManager::duplicateScene);
    reloadSceneBtn.addListener(stateManager.get(), &StateManager::reloadScene);
    deleteSceneBtn.addListener(stateManager.get(), &StateManager::deleteScene);

    // State manager must be set up first
    displayGui.setup(stateManager->activeScene.displayParameters);
    trackParamsGui.setup(stateManager->activeTrack.displayParameters);
    colorSections(displayGui);
    colorSections(trackParamsGui);
    int milkdropColor = twisterColorForSection("MilkDrop");
    milkdropGui.setHeaderBackgroundColor(guiColorForTwister(milkdropColor, 110));
    for (std::size_t i = 0; i < milkdropGui.getNumControls(); ++i) {
        milkdropGui.getControl(i)->setFillColor(guiColorForTwister(milkdropColor, 170));
    }

    // Mixer
    mixerParams.setName("Mixer");
    mixerParams.add(engineA.set("A (flames/milk/shader)", ENGINE_FLAMES, ENGINE_FLAMES, ENGINE_SHADER));
    mixerParams.add(engineB.set("B (flames/milk/shader)", ENGINE_MILKDROP, ENGINE_FLAMES, ENGINE_SHADER));
    mixerParams.add(crossfade.set("crossfade A-B", 0, 0, 1));
    mixerParams.add(presetFadeSeconds.set("preset fade (s)", 2, 0, 10));
    mixerGui.setup(mixerParams);

    // Shader engine
    shaderParams.setName("Shader");
    shaderParams.add(shaderRunning.set("running", true));
    shaderParams.add(shaderAnimatedOnly.set("animated only", true));
    shaderGui.setup(shaderParams);
    shaderGui.add(prevShaderBtn.setup("Prev shader  <"));
    shaderGui.add(nextShaderBtn.setup("Next shader  >"));
    prevShaderBtn.addListener(this, &GuiApp::prevShaderPressed);
    nextShaderBtn.addListener(this, &GuiApp::nextShaderPressed);
    int shaderColor = twisterColorForSection("Shader");
    shaderGui.setHeaderBackgroundColor(guiColorForTwister(shaderColor, 110));
    for (std::size_t i = 0; i < shaderGui.getNumControls(); ++i) {
        shaderGui.getControl(i)->setFillColor(guiColorForTwister(shaderColor, 170));
    }

    // Spotlights: one panel, lights 2-4 folded until needed
    lightsGui.setup(lighting->parameters);
    lightsGui.add(saveLightsBtn.setup("Save snapshot"));
    lightsGui.add(loadLightsBtn.setup("Load snapshot"));
    saveLightsBtn.addListener(this, &GuiApp::saveLightsPressed);
    loadLightsBtn.addListener(this, &GuiApp::loadLightsPressed);
    for (int i = 1; i < LIGHTING_N_LIGHTS; ++i) {
        lightsGui.getGroup(lighting->lights[i].group.getName()).minimize();
    }
    int lightsColor = twisterColorForSection("Lights");
    lightsGui.setHeaderBackgroundColor(guiColorForTwister(lightsColor, 110));
    for (std::size_t i = 0; i < lightsGui.getNumControls(); ++i) {
        ofxBaseGui *control = lightsGui.getControl(i);
        control->setFillColor(guiColorForTwister(lightsColor, 170));
        if (ofxGuiGroup *group = dynamic_cast<ofxGuiGroup *>(control)) {
            group->setHeaderBackgroundColor(guiColorForTwister(lightsColor, 110));
            for (std::size_t j = 0; j < group->getNumControls(); ++j) {
                group->getControl(j)->setFillColor(guiColorForTwister(lightsColor, 170));
            }
        }
    }

    inputGui.setPosition(10, 450);
    milkdropGui.setPosition(inputGui.getPosition().x, inputGui.getPosition().y + inputGui.getHeight() + 10);
    debugGui.setPosition(inputGui.getPosition().x + inputGui.getWidth() + 10, inputGui.getPosition().y);
    metaGui.setPosition(debugGui.getPosition().x, debugGui.getPosition().y + debugGui.getHeight() + 10);
    trackParamsGui.setPosition(510, 75);
    trackControlGui.setPosition(trackParamsGui.getPosition().x, trackParamsGui.getPosition().y + trackParamsGui.getHeight() + 10);
    displayGui.setPosition(trackControlGui.getPosition().x + trackControlGui.getWidth() + 10, 15);
    lightsGui.setPosition(displayGui.getPosition().x + displayGui.getWidth() + 10, 15);
    mixerGui.setPosition(metaGui.getPosition().x, metaGui.getPosition().y + metaGui.getHeight() + 10);
    shaderGui.setPosition(lightsGui.getPosition().x + lightsGui.getWidth() + 10, 15);


    float height = MAX(MAX(MAX(MAX(milkdropGui.getPosition().y + milkdropGui.getHeight() + 20,
                       debugGui.getPosition().y + inputGui.getHeight()),
                       metaGui.getPosition().y + metaGui.getHeight()),
                       displayGui.getPosition().y + displayGui.getHeight()),
                       trackControlGui.getPosition().y + trackControlGui.getHeight());
    // Room for the lights panel with one light open
    height = MAX(height, lightsGui.getPosition().y + lightsGui.getHeight() + 150);

    height = MAX(height, mixerGui.getPosition().y + mixerGui.getHeight());

    ofSetWindowShape(shaderGui.getPosition().x + shaderGui.getWidth() + 10,
                     height + 10);

    visuals = NULL;
    audioBuckets = NULL;
    nAudioBuckets = 0;
    frameRate = 0;
    pctParticles = 0;
    mpx = 0;
    mpy = 0;

    ofBackground(0);
    ofSetVerticalSync(false);
}

void GuiApp::update(){
    // A new (or reloaded) shader: a fresh panel of its inputs
    if (shaderEngine->parametersVersion != shownShaderVersion) {
        shownShaderVersion = shaderEngine->parametersVersion;
        shaderInputsGui.setup(shaderEngine->getShader().parameters);
        shaderInputsGui.setPosition(shaderGui.getPosition().x, shaderGui.getPosition().y + shaderGui.getHeight() + 50);
        int shaderColor = twisterColorForSection("Shader");
        shaderInputsGui.setHeaderBackgroundColor(guiColorForTwister(shaderColor, 110));
        for (std::size_t i = 0; i < shaderInputsGui.getNumControls(); ++i) {
            shaderInputsGui.getControl(i)->setFillColor(guiColorForTwister(shaderColor, 170));
        }
    }

    if (ofGetFrameNum() % 30 == 0) {
        // Every 30 frames, refresh some data...
        ofSoundStream ss;
        soundDevices = ss.getDeviceList();
        if (soundDevices.size() != nSoundDevices) {
            if (soundStreamDevice.get() >= soundDevices.size()) {
                soundStreamDevice.set(0);
            }
            nSoundDevices = soundDevices.size();
            soundStreamDevice.setMax(nSoundDevices);
        }
    }

    stateManager->wandering = wandering.get();
    stateManager->fadeFrames = fadeFrames.get();
}

void GuiApp::draw() {
    char s[512];
    float margin = 10, padding = 10;
    
    ofPushMatrix();
    
    if (frameRate) {
        sprintf(s, "%.2f fps", frameRate);
        ofDrawBitmapString(s, 10, 15);
    }

    if (flamesPaused) {
        ofDrawBitmapString("flames paused", 150, 15);
    } else if (pctParticles) {
        sprintf(s, "%d%% particles", (int)(pctParticles*100));
        ofDrawBitmapString(s, 150, 15);
    }

    if (stateManager->trackIdx >= 0) {
        sprintf(s, "track: %d / %lu", stateManager->trackIdx, stateManager->tracks.size()-1);
        ofDrawBitmapString(s, 510, 25);
        sprintf(s, "scene: %d / %lu", stateManager->sceneIdx, stateManager->getTrack().scenes.size()-1);
        ofDrawBitmapString(s, 510, 40);

        if (wandering) {
            ofNoFill();
            ofDrawRectangle(510, 50, 100, 15);
            ofFill();
            ofDrawRectangle(510, 50, 100 * stateManager->activeTrack.interpAmt, 15);
        } else if (stateManager->getGenomeInterpolation() > 0.001) {
            ofNoFill();
            ofDrawRectangle(510, 50, 100, 15);
            ofFill();
            ofDrawRectangle(510, 50, 100 * stateManager->getGenomeInterpolation(), 15);
        } else {
            ofDrawBitmapString("Not wandering or fading", 510, 60);
        }
    }



    ofTranslate(0, 17);

    if (visuals) {
        ofTranslate(0, margin);
        // TODO fit visuals to a bounding box
        
        float scale = std::min(480.0 / visuals->getWidth(), 270.0 / visuals->getHeight());
        float width = visuals->getWidth() * scale;
        float height = visuals->getHeight() * scale;

        ofFill();
        ofSetColor(255, 255);
        ofDrawRectangle(margin, 0, width + padding * 2, height + padding * 2);
        ofSetColor(0, 255);
        ofDrawRectangle(margin + padding, padding, width, height);
        ofSetColor(255, 255);
        visuals->draw(margin + padding, padding, width, height);
        previewRect.set(margin + padding, 17 + margin + padding, width, height);

        // Spotlight markers: beam outline and number, grey when off; the selected one gets a cross
        ofPushStyle();
        ofNoFill();
        for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
            Light &light = lighting->lights[i];
            float lx = margin + padding + light.x * width;
            float ly = padding + light.y * height;
            ofSetColor(light.on ? ofFloatColor::fromHsb(light.hue, std::max(0.4f, light.saturation.get()), 1) : ofFloatColor(0.5));
            ofSetLineWidth(i + 1 == lighting->selected ? 2 : 1);
            ofDrawCircle(lx, ly, light.size * height);
            if (i + 1 == lighting->selected) {
                ofDrawLine(lx - 5, ly, lx + 5, ly);
                ofDrawLine(lx, ly - 5, lx, ly + 5);
            }
            ofDrawBitmapString(ofToString(i + 1), lx + 4, ly - 4);
        }
        ofPopStyle();

        ofTranslate(0, 270 + padding * 2);
    }
    
    if (audioBuckets) {
        ofTranslate(0, 100 + margin);

        ofPushStyle();

        ofSetColor(255);
        ofNoFill();
        ofSetLineWidth(1);
        
        float fftWidth = 415.0, fftHeight = 100;
        float fftBucketWidth = fftWidth / nAudioBuckets;
        
        ofPushMatrix();
        
        ofTranslate(margin, 0);
        ofDrawRectangle(0, 0, fftWidth, -fftHeight);
        ofBeginShape();
        for (int i = 0; i < nAudioBuckets; i++) {
            ofVertex(i * fftBucketWidth, -audioBuckets[i] * fftHeight);
        }
        ofEndShape();
        
        string audioModeDesc;
        if (audioMode == AUDIO_MODE_MIC) {
            audioModeDesc = "Microphone Input";
        } else if (audioMode == AUDIO_MODE_NOISE) {
            audioModeDesc = "White Noise";
        } else {
            audioModeDesc = "No Audio";
        }
        ofDrawBitmapString(audioModeDesc, 5, 15-fftHeight);

        if (soundStreamDevice < soundDevices.size()) {
            ofDrawBitmapString(soundDevices[soundStreamDevice].name, 5, 30-fftHeight);
        }
        
        // Draw audio centroid
        float mappedCentroid = fftWidth * audioCentroid;
        ofSetColor(200, 0, 0);
        ofDrawLine(mappedCentroid, 0, mappedCentroid, -fftHeight);
        ofDrawBitmapString("centroid", mappedCentroid + 4, -30);
        float mappedCentroidMax = fftWidth * stateManager->activeScene.centroidMaxBucket;
        ofSetColor(125, 0, 0);
        ofDrawLine(mappedCentroidMax, 0, mappedCentroidMax, -fftHeight);
        ofDrawBitmapString("centroidMax", mappedCentroidMax + 4, -30);
        
        // Draw MPX, MPY
        float mpxHeight = 90 * mpx;
        float mpyHeight = 90 * mpy;
        ofFill();
        ofSetColor(255);
        ofDrawBitmapString("mpx", fftWidth + 10, -90);
        ofDrawRectangle(fftWidth + 10, 0, 25, -mpxHeight);
        ofDrawBitmapString("mpy", fftWidth + 40, -90);
        ofDrawRectangle(fftWidth + 40, 0, 25, -mpyHeight);
        
        ofPopMatrix();
        ofPopStyle();
    }
    
    ofPopMatrix();

    inputGui.draw();
    lightsGui.draw();
    mixerGui.draw();
    shaderGui.draw();
    ofSetColor(255);
    float statusY = shaderGui.getPosition().y + shaderGui.getHeight() + 14;
    // The shader's number and name, over two lines if needed
    string status = shaderEngine->getStatus();
    ofDrawBitmapString(status.substr(0, 26), shaderGui.getPosition().x, statusY);
    if (status.size() > 26) {
        ofDrawBitmapString(status.substr(26, 26), shaderGui.getPosition().x, statusY + 14);
    }
    if (shaderPaused) ofDrawBitmapString("shader paused", shaderGui.getPosition().x, statusY + 28);
    shaderInputsGui.draw();
    milkdropGui.draw();
    if (!milkdropPresetName.empty()) {
        ofSetColor(255);
        ofDrawBitmapString(milkdropPresetName.substr(0, 38),
                           milkdropGui.getPosition().x, milkdropGui.getPosition().y + milkdropGui.getHeight() + 14);
    }
    debugGui.draw();
    displayGui.draw();
    metaGui.draw();
    trackParamsGui.draw();
    trackControlGui.draw();
}

void GuiApp::prevShaderPressed() {
    keyPresses.push('<');
}

void GuiApp::nextShaderPressed() {
    keyPresses.push('>');
}

void GuiApp::exit() {
    ofExit();
}

void GuiApp::saveLightsPressed() {
    lighting->saveSnapshot(lighting->snapshot);
}

void GuiApp::loadLightsPressed() {
    lighting->loadSnapshot(lighting->snapshot);
}

void GuiApp::moveLightTo(int index, int x, int y) {
    Light &light = lighting->lights[index];
    light.x.set(ofClamp((x - previewRect.x) / previewRect.width, 0, 1));
    light.y.set(ofClamp((y - previewRect.y) / previewRect.height, 0, 1));
}

// On the preview, a click near a light's centre grabs that light; anywhere
// else it moves the selected light there. Dragging keeps moving it.
void GuiApp::mousePressed(int x, int y, int button) {
    if (!previewRect.inside(x, y)) return;

    draggedLight = lighting->selected - 1;
    float nearest = 12;   // pixels
    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        Light &light = lighting->lights[i];
        float d = ofDist(x, y, previewRect.x + light.x * previewRect.width, previewRect.y + light.y * previewRect.height);
        if (d < nearest) {
            nearest = d;
            draggedLight = i;
        }
    }
    lighting->selected.set(draggedLight + 1);
    moveLightTo(draggedLight, x, y);
}

void GuiApp::mouseDragged(int x, int y, int button) {
    if (draggedLight >= 0) moveLightTo(draggedLight, x, y);
}

void GuiApp::mouseReleased(int x, int y, int button) {
    draggedLight = -1;
}

// ofApp owns projectM, so the buttons go through the same key queue as the keyboard
void GuiApp::prevPresetPressed() {
    keyPresses.push('[');
}

void GuiApp::nextPresetPressed() {
    keyPresses.push(']');
}

void GuiApp::handleKey(int key) {
    // None for now
}

void GuiApp::keyPressed(int key) {
    keyPresses.push(key);
}

void GuiApp::keyReleased(int key) {
    if (key == OF_KEY_COMMAND) {
        keyPresses.push(DOTS_KEY_COMMAND_UP);
    }
}
