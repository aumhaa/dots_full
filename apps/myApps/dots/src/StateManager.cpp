//
//  StateManager.cpp
//  dotsLocal
//
//  Created by Christopher Anderson on 4/9/16.
//
//

#include "StateManager.h"

void interpolateParameters(const ofParameterGroup &from, const ofParameterGroup &to,
                           const ofParameterGroup &ret, float t) {
    for(std::size_t i = 0; i < from.size(); i++){
        string type = from.getType(i);
        if(type == typeid(ofParameter <int32_t> ).name()){
            auto fromParam = from.getInt(i);
            auto toParam = to.getInt(i);
            auto retParam = ret.getInt(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <uint32_t> ).name()){
            auto fromParam = from.get<uint32_t>(i);
            auto toParam = to.get<uint32_t>(i);
            auto retParam = ret.get<uint32_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <int64_t> ).name()){
            auto fromParam = from.get<int64_t>(i);
            auto toParam = to.get<int64_t>(i);
            auto retParam = ret.get<int64_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <uint64_t> ).name()){
            auto fromParam = from.get<uint64_t>(i);
            auto toParam = to.get<uint64_t>(i);
            auto retParam = ret.get<uint64_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <int8_t> ).name()){
            auto fromParam = from.get<int8_t>(i);
            auto toParam = to.get<int8_t>(i);
            auto retParam = ret.get<int8_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <uint8_t> ).name()){
            auto fromParam = from.get<uint8_t>(i);
            auto toParam = to.get<uint8_t>(i);
            auto retParam = ret.get<uint8_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <int16_t> ).name()){
            auto fromParam = from.get<int16_t>(i);
            auto toParam = to.get<int16_t>(i);
            auto retParam = ret.get<int16_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <uint16_t> ).name()){
            auto fromParam = from.get<uint16_t>(i);
            auto toParam = to.get<uint16_t>(i);
            auto retParam = ret.get<uint16_t>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <float> ).name()){
            auto fromParam = from.getFloat(i);
            auto toParam = to.getFloat(i);
            auto retParam = ret.getFloat(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <double> ).name()){
            auto fromParam = from.get<double>(i);
            auto toParam = to.get<double>(i);
            auto retParam = ret.get<double>(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <bool> ).name()){
            auto fromParam = from.getBool(i);
            auto toParam = to.getBool(i);
            auto retParam = ret.get<bool>(i);
            retParam.set(t > 0.5 ? toParam.get() : fromParam.get());
        }else if(type == typeid(ofParameter <ofVec2f> ).name()){
            auto fromParam = from.getVec2f(i);
            auto toParam = to.getVec2f(i);
            auto retParam = ret.getVec2f(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <ofVec3f> ).name()){
            auto fromParam = from.getVec3f(i);
            auto toParam = to.getVec3f(i);
            auto retParam = ret.getVec3f(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <ofVec4f> ).name()){
            auto fromParam = from.getVec4f(i);
            auto toParam = to.getVec4f(i);
            auto retParam = ret.getVec4f(i);
            retParam.set((toParam.get() - fromParam.get()) * t + fromParam.get());
        }else if(type == typeid(ofParameter <ofColor> ).name()){
            auto fromParam = from.getColor(i);
            auto toParam = to.getColor(i);
            auto retParam = ret.getColor(i);
            retParam.set(fromParam.get().getLerped(toParam.get(), t));
        }else if(type == typeid(ofParameter <ofShortColor> ).name()){
            auto fromParam = from.getShortColor(i);
            auto toParam = to.getShortColor(i);
            auto retParam = ret.getShortColor(i);
            retParam.set(fromParam.get().getLerped(toParam.get(), t));
        }else if(type == typeid(ofParameter <ofFloatColor> ).name()){
            auto fromParam = from.getFloatColor(i);
            auto toParam = to.getFloatColor(i);
            auto retParam = ret.getFloatColor(i);
            retParam.set(fromParam.get().getLerped(toParam.get(), t));
        }else if(type == typeid(ofParameter <string> ).name()){
            auto fromParam = from.getString(i);
            auto toParam = to.getString(i);
            auto retParam = ret.getString(i);
            retParam.set(t > 0.5 ? toParam.get() : fromParam.get());
        }else if(type == typeid(ofParameterGroup).name()){
            auto fromParam = from.getGroup(i);
            auto toParam = to.getGroup(i);
            auto retParam = ret.getGroup(i);
            interpolateParameters(fromParam, toParam, retParam, t);
        }else{
            ofLogWarning() << "ofxBaseGroup; no control for parameter of type " << type;
        }
    }
}

template <typename ParameterType>
void setParam(ofParameter<ParameterType>& param, float t) {
    param.set(ofLerp(param.getMin(), param.getMax(), t));
}

void StateManager::setup() {
    loadGenebanks();

    seed = 0;
    initrc(seed);
    fading = false;
    wasFading = false;
    fadeFrames = 100;

    locks.setName("Locks");
    locks.add(lockAudioAnalysis.set("audio analysis", false));
    locks.add(lockDrawing.set("drawing", false));
    locks.add(lockSpeed.set("speed", false));
    locks.add(lockDots.set("dots", false));
    locks.add(lockLines.set("lines", false));
    locks.add(lockAudioEffects.set("audio effects", false));

    // No genome in activeScene yet
    activeScene.setupParams();
    activeTrack.setupParams();

    // Build default scene
    DotsTrack defaultTrack;
    defaultTrack.trackId = 0;
    defaultTrack.setupParams();

    for (int cpIdx = 0; cpIdx < ncps; ++cpIdx) {
        DotsScene *scene = new DotsScene;
        scene->setupParams();
        scene->motionId = cpIdx;
        scene->setupGenome(&cps[cpIdx]);

        defaultTrack.scenes.push_back(scene);
    }
    defaultTrack.setGenomesFromScenes();
    defaultTrack.copyParamsTo(activeTrack);

    memset(&wanderGenome, 0, sizeof(flam3_genome));

    tracks.push_back(std::move(defaultTrack));

    maxTrackId = 0;

    // Now load tracks from file
    loadAllParamsFromFile();

    wandering = false;
    trackIdx = 0;
    sceneIdx = -1;
    advanceScene();
}

void StateManager::loadGenebanks() {
    {
        FILE *in;
        string path = ofToDataPath("live.flam3", true);
        char *inf = (char *)path.c_str();
        in = fopen(inf, "rb");
        if (!in) {
            cout << "couldn't open flame3, exiting" << endl;
            std::exit(1);
        }
        cps = flam3_parse_from_file(in, inf, flam3_defaults_on, &ncps);
        for (int i = 0; i < ncps; i++) {
            cps[i].time = (double) i;
        }
        if (!cps) {
            cout << "couldn't read flame3, exiting" << endl;
            std::exit(1);
        }
    }
    {
        FILE *in;
        string path = ofToDataPath("genebank2.flam3", true);
        char *inf = (char *)path.c_str();
        in = fopen(inf, "rb");
        if (!in) {
            cout << "couldn't open genebank, exiting" << endl;
            std::exit(1);
        }
        genebank = flam3_parse_from_file(in, inf, flam3_defaults_on, &ngenebank);
    }
}

void StateManager::genomeModified(int & newGenomeIdx) {
//    if (newGenomeIdx != currScene->genomeIdx)
//        trackDirty = true;
}

void StateManager::update() {
    if (currScene && nextScene) {
        if (wandering) {
            applyParameterInterpolation(activeTrack.interpAmt);
        } else {
            float t = getGenomeInterpolation();

            if (fading && fadeForward && t > 0.999) {
                if (fadeTargetScene >= 0) {
                    selectScene(fadeTargetScene);
                    fadeTargetScene = -1;
                } else {
                    advanceScene();
                }
                fading = false;
            } else if (fading && !fadeForward && t < 0.001) {
                fading = false;
            } else if (t > 0.001) {
                applyParameterInterpolation(t);
            }
        }
    }
}

void StateManager::flameWanderUpdate() {
    DotsTrack &t = getTrack();

    activeTrack.wanderPos += activeTrack.getWanderSpeed();

    if (activeTrack.wanderPos >= t.nGenomes)
        activeTrack.wanderPos = 0;

    int currCP = (int)activeTrack.wanderPos;

    if (lastCP != currCP) {
        advanceScene();
        lastCP = currCP;
    }

    activeTrack.interpAmt = activeTrack.wanderPos - currCP;

    // cpTrackOrder has n+1 - last is a copy of first
    flam3_interpolate(t.genomes, t.nGenomes + 1, activeTrack.wanderPos, 0, &wanderGenome);
}

void StateManager::flameUpdate(float audioRMS) {
    // Rotate both the same amount
    float speed = activeScene.baseSpeed + audioRMS * activeScene.rmsSpeedMult;
    // A bad value here would be written into the genomes and kill the flames for good
    if (!std::isfinite(speed)) return;
    flam3_rotate(currScene->genome, speed, flam3_inttype_log);
    flam3_rotate(nextScene->genome, speed, flam3_inttype_log);
}

int StateManager::randomi(int n) {
    int i = flam3_random_isaac_01(&rc) * ncps;
    return i % n;
}

float StateManager::getGenomeInterpolation() {
//    return activeTrack.wanderSpeed;

    if (!fading) return 0;

    long currFrame = ofGetFrameNum();
    long delta = currFrame - fadeStartFrame;
    int fadeFrames = fadeTargetScene >= 0 ? fadeTargetFrames : this->fadeFrames;

    // TODO: make this a param
    float split = (float)delta / fadeFrames;

    // We might be fading backwards
    if (fadeForward) {
        return ofClamp(split, 0, 1);
    } else {
        return ofClamp(1.0f - split, 0, 1);
    }
}

void StateManager::loadGenome(flam3_genome *dest) {
    if (!currScene) return;

    flam3_copy(dest, currScene->genome);
}

void StateManager::killCurrent() {
    flam3_genome result;
    memset(&result, 0, sizeof(flam3_genome));
    initrc(time(NULL));
    if (randomi(2)) {
        cout << "Crossing random with random" << endl;
        int parent0 = randomi(ncps);
        int parent1 = randomi(ncps);
        flam3_cross(&cps[parent0], &cps[parent1], &result, CROSS_NOT_SPECIFIED, &rc, NULL);
    } else {
        cout << "Crossing random with genebank" << endl;
        int parent0 = randomi(ncps);
        int parent1 = randomi(ngenebank);
        flam3_cross(&cps[parent0], &genebank[parent1], &result, CROSS_NOT_SPECIFIED, &rc, NULL);
    }

    flam3_copy(getScene().genome, &result);

    getTrack().setGenomesFromScenes();
    ofNotifyEvent(onTrackUpdate, trackIdx);
}

void StateManager::mateCurrent() {
    flam3_genome result;
    memset(&result, 0, sizeof(flam3_genome));
    initrc(time(NULL));
    int parent1 = random()%ncps;
    flam3_cross(getScene().genome, &cps[parent1], &result, CROSS_NOT_SPECIFIED, &rc, NULL);
    flam3_copy(getScene().genome, &result);

    getTrack().setGenomesFromScenes();
    ofNotifyEvent(onTrackUpdate, trackIdx);
}

void StateManager::mutateCurrent() {
    flam3_genome result;
    memset(&result, 0, sizeof(flam3_genome));
    initrc(time(NULL));
    int ivars[flam3_nvariations];
    for (int i = 0; i < flam3_nvariations; i++)
        ivars[i] = i;

    flam3_mutate(getScene().genome, MUTATE_NOT_SPECIFIED, ivars, flam3_nvariations, 0, 0.2, &rc, NULL);

    getTrack().setGenomesFromScenes();
    ofNotifyEvent(onTrackUpdate, trackIdx);
}

void StateManager::initrc(long sed) {
    /* Set up the isaac rng */
    for (int lp = 0; lp < RANDSIZ; lp++)
        rc.randrsl[lp] = sed;
    irandinit(&rc,1);
}


void StateManager::controllerToSceneParams(int n, float v){
    cout << "controllerToSceneParams: " + std::to_string(n) + " - " + std::to_string(v) << endl;
    switch(n) {
        case 0:
            setParam(activeTrack.wanderSpeed, v);
            break;
        case 1:
            setParam(activeScene.fftDecayRate, v);
            break;
        case 2:
            setParam(activeScene.centroidMaxBucket, v);
            break;
        case 3:
            setParam(activeScene.rmsMultiple, v);
            break;
        case 4:
            setParam(activeScene.mpxSmoothingFactor, v);
            break;
        case 5:
            setParam(activeScene.mpySmoothingFactor, v);
            break;
        case 6:
            setParam(activeScene.clearSpeed, v);
            break;
        case 7:
            setParam(activeScene.particleAlpha, v);
            break;
        case 8:
            setParam(activeScene.overallScale, v);
            break;
        case 9:
            setParam(activeScene.saturationPct, v);
            break;
        case 10:
            setParam(activeScene.baseSpeed, v);
            break;
        case 11:
            setParam(activeScene.rmsSpeedMult, v);
            break;
        case 12:
            setParam(activeScene.pointRadiusAudioScaleAmt, v);
            break;
        case 13:
            setParam(activeScene.pointRadiusAudioScale, v);
            break;
        case 14:
            setParam(activeScene.basePointRadius, v);
            break;
        case 15:
            setParam(activeScene.maxLineLength, v);
            break;
        case 16:
//            cout << "activeSceine.motionId" + std::to_string(static_cast<int>(v*36.0)) + " " +  to_string(static_cast<int>(activeScene.motionId)) << endl;
//            if (static_cast<int>(v*36.0)!=static_cast<int>(activeScene.motionId)) {
            setParam(activeScene.motionId, v);
//            }
            break;
        case 17:
            setParam(activeScene.audioEffectSize1, v);
            break;
        case 18:
            setParam(activeScene.audioEffectSize2, v);
            break;
        case 19:
            setParam(activeScene.audioEffectSize3, v);
            break;
        case 20:
            setParam(activeScene.audioEffectSize4, v);
            break;

    }
}

void StateManager::noteToSceneParams(int n){
//    cout << "controllerToSceneParams: " + std::to_string(n) + " - " + std::to_string(v) << endl;
    switch(n) {
        case 0:
            regressTrack();
            break;
        case 1:
            advanceTrack();
            break;
        case 2:
            regressScene();
            break;
        case 3:
            advanceScene();
            break;
        case 4:
            wandering = !wandering;
            break;
//        case 5:
//            setParam(activeScene.audioEffectSize2, v);
//            break;
        default:
            cout << "noteToSceneParams" << endl;
    }
}

DotsTrack& StateManager::getTrack() {
    return tracks[trackIdx];
}

DotsScene& StateManager::getScene() {
    return *currScene;
}

void StateManager::regressTrack() {
    cout << "Regress track" << endl;
    if (trackIdx < 0) return;

    activeTrack.copyParamsTo(getTrack());

    trackIdx--;
    if (trackIdx < 0) {
        trackIdx = tracks.size() - 1;
    }

    getTrack().copyParamsTo(activeTrack);

    sceneIdx = -1;
    advanceScene();

    ofNotifyEvent(onTrackChange, trackIdx);
}

void StateManager::advanceTrack() {
    cout << "Advance track" << endl;
    if (trackIdx < 0) return;

    activeTrack.copyParamsTo(getTrack());

    trackIdx++;
    if (trackIdx >= tracks.size()) {
        trackIdx = 0;
    }

    getTrack().copyParamsTo(activeTrack);

    sceneIdx = -1;
    advanceScene();

    ofNotifyEvent(onTrackChange, trackIdx);
}

bool StateManager::isLocked(const string &groupName) {
    if (groupName == "Audio Analysis") return lockAudioAnalysis;
    if (groupName == "Drawing") return lockDrawing;
    if (groupName == "Speed") return lockSpeed;
    if (groupName == "Dots") return lockDots;
    if (groupName == "Lines") return lockLines;
    if (groupName == "Audio Effects") return lockAudioEffects;
    return false;
}

static void copyUnlockedBlocks(StateManager &sm, const ofParameterGroup &from, const ofParameterGroup &to) {
    for (std::size_t i = 0; i < from.size(); ++i) {
        if (from.getType(i) != typeid(ofParameterGroup).name()) continue;
        const ofParameterGroup &block = from.getGroup(i);
        if (!sm.isLocked(block.getName())) copyParameters(block, to.getGroup(i));
    }
}

void StateManager::loadActiveScene(DotsScene &scene) {
    copyUnlockedBlocks(*this, scene.displayParameters, activeScene.displayParameters);
}

void StateManager::storeActiveScene(DotsScene &scene) {
    copyUnlockedBlocks(*this, activeScene.displayParameters, scene.displayParameters);
}

void StateManager::selectTrack(int index) {
    if (index < 0 || index >= tracks.size()) return;

    activeTrack.copyParamsTo(getTrack());
    trackIdx = index;
    getTrack().copyParamsTo(activeTrack);

    sceneIdx = -1;
    advanceScene();

    ofNotifyEvent(onTrackChange, trackIdx);
}

void StateManager::selectScene(int index) {
    if (index < 0 || index >= getTrack().scenes.size()) return;
    // advanceScene() moves on one from sceneIdx, saving the scene we leave
    sceneIdx = index - 1;
    advanceScene();
}

void StateManager::fadeToScene(int index, int frames) {
    if (index < 0 || index >= getTrack().scenes.size() || index == sceneIdx) return;
    if (wandering || fading || frames <= 0) {
        selectScene(index);
        return;
    }
    // The usual forward fade, with the target scene standing in as the next one
    storeActiveScene(*currScene);
    nextScene = getTrack().scenes[index];
    fadeTargetScene = index;
    fadeTargetFrames = frames;
    fading = true;
    fadeForward = true;
    fadeStartFrame = ofGetFrameNum();
    ofNotifyEvent(onSceneUpdate, sceneIdx);
}

int StateManager::trackIndexForId(int trackId) {
    for (int i = 0; i < tracks.size(); ++i) {
        if (tracks[i].trackId == trackId) return i;
    }
    return -1;
}

void StateManager::saveTrack() {
    cout << "Save track" << endl;
    storeActiveScene(*currScene);
    activeTrack.copyParamsTo(getTrack());
    getTrack().setGenomesFromScenes();
    serializeCurrentTrackToFile();
}

void StateManager::fadeSceneFwd() {
    fadeTargetScene = -1;
    if (wandering) return;

    if (!fading) {
        storeActiveScene(*currScene);

        fading = true;
        fadeStartFrame = ofGetFrameNum();
        fadeForward = true;
    } else {
        advanceScene();
        fading = false;
    }
}

void StateManager::fadeSceneRev() {
    fadeTargetScene = -1;
    if (wandering) return;

    if (!fading) {
        regressScene();

        fading = true;
        fadeStartFrame = ofGetFrameNum();
        fadeForward = false;
    } else {
        loadActiveScene(*currScene);
        fading = false;
    }
}

void StateManager::regressScene() {
    if (trackIdx < 0) return;

    sceneIdx--;
    int nextIdx = sceneIdx + 1;

    int nTracks = getTrack().scenes.size();
    if (sceneIdx < 0) {
        sceneIdx = nTracks-1;
        nextIdx = 0;
    }

    if (nextIdx >= nTracks) {
        nextIdx = 0;
    }

    // Save what we're coming from
    if (currScene && !wandering && !fading) {
        storeActiveScene(*currScene);
    }

    currScene = tracks[trackIdx].scenes[sceneIdx];
    nextScene = tracks[trackIdx].scenes[nextIdx];

    // Load what we're going to
    loadActiveScene(*currScene);

    ofNotifyEvent(onSceneChange, sceneIdx);
}

void StateManager::advanceScene() {
    if (trackIdx < 0) return;

    sceneIdx++;
    int nextIdx = sceneIdx + 1;

    int nTracks = getTrack().scenes.size();
    if (sceneIdx >= nTracks) {
        sceneIdx = 0;
        nextIdx = 1;
    }

    if (nextIdx >= nTracks) {
        nextIdx = 0;
    }

    // Save what we're coming from
    if (currScene && !wandering && !fading) {
        storeActiveScene(*currScene);
    }

    currScene = tracks[trackIdx].scenes[sceneIdx];
    nextScene = tracks[trackIdx].scenes[nextIdx];

    // Load what we're going to
    loadActiveScene(*currScene);

    ofNotifyEvent(onSceneChange, sceneIdx);
}

void StateManager::reloadScene() {
    cout << "Reload scene" << endl;
    loadActiveScene(*currScene);
    getTrack().setGenomesFromScenes();

    ofNotifyEvent(onSceneChange, sceneIdx);
}

void StateManager::deleteScene() {
    cout << "Delete scene" << endl;

    vector<DotsScene *> &scenes = tracks[trackIdx].scenes;
    scenes.erase(scenes.begin() + sceneIdx);
    getTrack().setGenomesFromScenes();

    // TODO: less ham-fisted way to do this
    regressScene();
    advanceScene();

    ofNotifyEvent(onSceneChange, sceneIdx);
}

// TODO: unify with the below
void StateManager::duplicateScene() {
    cout << "Duplicate scene" << endl;

    if (!currScene) return;

    DotsScene *cpy = new DotsScene;
    cpy->setupParams();
    activeScene.copyParamsTo(*cpy);
    currScene->copyGenomeTo(*cpy);

    vector<DotsScene *> &destScenes = tracks[trackIdx].scenes;
    destScenes.insert(destScenes.begin() + sceneIdx + 1, cpy);
    getTrack().setGenomesFromScenes();

    ofNotifyEvent(onTrackUpdate, trackIdx);
    ofNotifyEvent(onSceneChange, sceneIdx);
}

void StateManager::createTrack() {
    DotsTrack track;
    track.trackId = nextTrackId();
    track.setupParams();
    activeTrack.copyParamsTo(getTrack());

    DotsScene *scene = new DotsScene;
    scene->setupParams();
    activeScene.copyParamsTo(*scene);
    getScene().copyGenomeTo(*scene);

    track.scenes.push_back(scene);
    track.setGenomesFromScenes();

    tracks.push_back(std::move(track));
}

// TODO: clean up if possible
void StateManager::applyParameterInterpolation(float t) {
    if (t < 0.001) return;
    
    if (!lockDots) activeScene.pointRadiusAudioScaleAmt.set(ofLerp(currScene->pointRadiusAudioScaleAmt.get(), nextScene->pointRadiusAudioScaleAmt.get(), t));
    if (!lockDots) activeScene.pointRadiusAudioScale.set(ofLerp(currScene->pointRadiusAudioScale.get(), nextScene->pointRadiusAudioScale.get(), t));
    if (!lockAudioAnalysis) activeScene.fftDecayRate.set(ofLerp(currScene->fftDecayRate.get(), nextScene->fftDecayRate.get(), t));
    if (!lockAudioAnalysis) activeScene.rmsMultiple.set(ofLerp(currScene->rmsMultiple.get(), nextScene->rmsMultiple.get(), t));
    if (!lockAudioAnalysis) activeScene.centroidMaxBucket.set(ofLerp(currScene->centroidMaxBucket.get(), nextScene->centroidMaxBucket.get(), t));
    if (!lockAudioAnalysis) activeScene.mpxSmoothingFactor.set(ofLerp(currScene->mpxSmoothingFactor.get(), nextScene->mpxSmoothingFactor.get(), t));
    if (!lockAudioAnalysis) activeScene.mpySmoothingFactor.set(ofLerp(currScene->mpySmoothingFactor.get(), nextScene->mpySmoothingFactor.get(), t));
    if (!lockSpeed) activeScene.baseSpeed.set(ofLerp(currScene->baseSpeed.get(), nextScene->baseSpeed.get(), t));
    if (!lockSpeed) activeScene.rmsSpeedMult.set(ofLerp(currScene->rmsSpeedMult.get(), nextScene->rmsSpeedMult.get(), t));
    if (!lockDrawing) activeScene.clearSpeed.set(ofLerp(currScene->clearSpeed.get(), nextScene->clearSpeed.get(), t));
    if (!lockDrawing) activeScene.particleAlpha.set(ofLerp(currScene->particleAlpha.get(), nextScene->particleAlpha.get(), t));
    if (!lockDrawing) activeScene.overallScale.set(ofLerp(currScene->overallScale.get(), nextScene->overallScale.get(), t));
    if (!lockDrawing) activeScene.saturationPct.set(ofLerp(currScene->saturationPct.get(), nextScene->saturationPct.get(), t));
    if (!lockDots) activeScene.basePointRadius.set(ofLerp(currScene->basePointRadius.get(), nextScene->basePointRadius.get(), t));
    if (!lockLines) activeScene.maxLineLength.set(ofLerp(currScene->maxLineLength.get(), nextScene->maxLineLength.get(), t));
    if (!lockAudioEffects) activeScene.audioEffectSize1.set(ofLerp(currScene->audioEffectSize1.get(), nextScene->audioEffectSize1.get(), t));
    if (!lockAudioEffects) activeScene.audioEffectSize2.set(ofLerp(currScene->audioEffectSize2.get(), nextScene->audioEffectSize2.get(), t));
    if (!lockAudioEffects) activeScene.audioEffectSize3.set(ofLerp(currScene->audioEffectSize3.get(), nextScene->audioEffectSize3.get(), t));
    if (!lockAudioEffects) activeScene.audioEffectSize4.set(ofLerp(currScene->audioEffectSize4.get(), nextScene->audioEffectSize4.get(), t));
}

void StateManager::serializeCurrentTrackToFile() {
    cout << "Saving track idx " << trackIdx << " / id " << getTrack().trackId << " to file." << endl;

    char paramFilename[255];
    sprintf(paramFilename, "params/track-%03d.xml", getTrack().trackId);

    char flameFilename[255];
    sprintf(flameFilename, "genomes/track-%03d.xml", getTrack().trackId);

    ofFile flameFile(ofToDataPath(flameFilename), ofFile::WriteOnly);
    flameFile << "<genomes>" << endl;

    settings.clear();
    settings.addTag("track");
    settings.pushTag("track");

    ofSerialize(settings, getTrack().displayParameters);

    settings.addTag("scenes");
    settings.pushTag("scenes");
    int nScenes = getTrack().scenes.size();
    for (int i = 0; i < nScenes; ++i) {
        DotsScene *scene = getTrack().scenes[i];
        settings.addTag("scene");
        settings.pushTag("scene", i);
        ofSerialize(settings, scene->displayParameters);
        settings.popTag();

        flameFile << flam3_print_to_string(scene->genome) << endl;
    }
    settings.popTag();

    settings.popTag();
    settings.save(paramFilename);

    flameFile << "</genomes>" << endl;
    flameFile.close();
}

void StateManager::loadAllParamsFromFile() {
    map<int, DotsTrack> trackMap;

    ofDirectory dir("params");
    for (auto &file : dir.getFiles()) {
        settings.clear();
        settings.load(file.path());

        string filename = file.getFileName();
        if (filename.find("track") != 0) continue;

        // Load track id from filename
        int trackId = stoi(filename.substr(6, 9));

        // Skip 0th, even if it's saved.
        if (trackId == 0) continue;

        DotsTrack track;
        track.trackId = trackId;
        track.setupParams();

        settings.clear();
        settings.loadFile(file.getAbsolutePath());
        settings.pushTag("track");

        ofDeserialize(settings, track.displayParameters);

        settings.pushTag("scenes");
        int nScenes = settings.getNumTags("scene");
        for (int i = 0; i < nScenes; ++i) {
            DotsScene *scene = new DotsScene;
            scene->setupParams();

            settings.pushTag("scene", i);
            ofDeserialize(settings, scene->displayParameters);
            settings.popTag();

            track.scenes.push_back(scene);
        }

        settings.popTag();
        settings.popTag();

        if (track.scenes.size() == 0) continue;

        cout << "Loading track id " << trackId << " from file." << endl;
        trackMap[trackId] = track;
    }

    dir.open("genomes");
    for (auto &file : dir.getFiles()) {
        string filename = file.getFileName();

        if (filename.find("track") != 0) continue;

        // Load track idx from filename
        int trackId = stoi(filename.substr(6, 9));

        // Skip 0th, even if it's saved.
        if (trackId == 0) continue;

        // Somehow we're missing genomes!
        if (trackMap.count(trackId) == 0) continue;

        DotsTrack &track = trackMap.at(trackId);

        char *fn = strdup(file.getAbsolutePath().c_str());
        FILE *f = fopen(fn, "rb");
        track.genomes = flam3_parse_from_file(f, fn, flam3_defaults_on, &track.nGenomes);

        int i = 0;
        for (auto &scene : track.scenes) {
            scene->genome = (flam3_genome *)malloc(sizeof(flam3_genome));
            memset(scene->genome, 0, sizeof(flam3_genome));
            flam3_copy(scene->genome, track.genomes + i);
            i++;
        }
        track.setGenomesFromScenes();
    }

    for (auto &pair : trackMap) {
        tracks.push_back(std::move(pair.second));
        if (pair.second.trackId > maxTrackId)
            maxTrackId = pair.second.trackId;
    }

    // Sort tracks by idx
    std::sort(tracks.begin(), tracks.end(),
              [](const DotsTrack & a, const DotsTrack & b) -> bool
              {
                  return a.trackId < b.trackId;
              });
}
