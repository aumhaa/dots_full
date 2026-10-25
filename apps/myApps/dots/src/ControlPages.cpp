//
//  ControlPages.cpp
//  dots
//

#include "ControlPages.h"
#include "ofApp.h"

static bool isType(const ofAbstractParameter *p, const char *name) { return p->type() == name; }
#define IS(p, T) isType(p, typeid(ofParameter<T>).name())

float ControlEntry::normalized() const {
    if (!param) return 0;
    if (IS(param, float)) {
        ofParameter<float> &p = param->cast<float>();
        return ofMap(p.get(), p.getMin(), p.getMax(), 0, 1, true);
    }
    if (IS(param, int)) {
        ofParameter<int> &p = param->cast<int>();
        return ofMap(p.get(), p.getMin(), p.getMax(), 0, 1, true);
    }
    if (IS(param, bool)) return param->cast<bool>().get() ? 1 : 0;
    if (IS(param, glm::vec2)) {
        ofParameter<glm::vec2> &p = param->cast<glm::vec2>();
        int c = ofClamp(component, 0, 1);
        return ofMap(p.get()[c], p.getMin()[c], p.getMax()[c], 0, 1, true);
    }
    if (IS(param, ofFloatColor)) {
        ofFloatColor col = param->cast<ofFloatColor>().get();
        return ofClamp(col[ofClamp(component, 0, 3)], 0, 1);
    }
    return 0;
}

void ControlEntry::setNormalized(float t) const {
    if (!param) return;
    t = ofClamp(t, 0, 1);
    if (IS(param, float)) {
        ofParameter<float> &p = param->cast<float>();
        p.set(ofLerp(p.getMin(), p.getMax(), t));
    } else if (IS(param, int)) {
        ofParameter<int> &p = param->cast<int>();
        p.set((int)std::round(ofLerp(p.getMin(), p.getMax(), t)));
    } else if (IS(param, bool)) {
        param->cast<bool>().set(t >= 0.5f);
    } else if (IS(param, glm::vec2)) {
        ofParameter<glm::vec2> &p = param->cast<glm::vec2>();
        glm::vec2 v = p.get();
        int c = ofClamp(component, 0, 1);
        v[c] = ofLerp(p.getMin()[c], p.getMax()[c], t);
        p.set(v);
    } else if (IS(param, ofFloatColor)) {
        ofParameter<ofFloatColor> &p = param->cast<ofFloatColor>();
        ofFloatColor col = p.get();
        col[ofClamp(component, 0, 3)] = t;
        p.set(col);
    } else if (IS(param, void)) {
        if (t >= 0.5f) param->cast<void>().trigger();
    }
}

string ControlEntry::valueText() const {
    if (!param) return "";
    if (IS(param, float)) {
        ofParameter<float> &p = param->cast<float>();
        float range = std::abs(p.getMax() - p.getMin());
        return ofToString(p.get(), range >= 100 ? 0 : range >= 10 ? 1 : range >= 1 ? 2 : 3);
    }
    if (IS(param, int)) return ofToString(param->cast<int>().get());
    if (IS(param, bool)) return param->cast<bool>().get() ? "on" : "off";
    if (IS(param, void)) return "trigger";
    if (component >= 0) {
        float value = IS(param, glm::vec2) ? param->cast<glm::vec2>().get()[ofClamp(component, 0, 1)]
                                           : param->cast<ofFloatColor>().get()[ofClamp(component, 0, 3)];
        return ofToString(value, 3);
    }
    return "";
}

bool ControlEntry::isSwitch() const {
    return param && component < 0 && (IS(param, bool) || IS(param, void));
}

void ControlPages::setup(ofApp *dots) {
    app = dots;
    build();
}

bool ControlPages::update() {
    if (app->shaderEngine->parametersVersion == shaderVersion) return false;
    build();
    return true;
}

// A shader's inputs, flattened: a point is two entries, a colour three. A Synesthesia scene's groups
// are ofParameterGroups, whose type() isn't an ofParameter<>'s, so they're found by cast.
void ControlPages::addShaderInputs(ofParameterGroup &group, vector<ControlEntry> &entries) {
    for (auto &child : group) {
        ofAbstractParameter *p = child.get();
        if (auto inner = dynamic_cast<ofParameterGroup *>(p)) {
            addShaderInputs(*inner, entries);
            continue;
        }
        string name = p->getName();
        if (IS(p, glm::vec2)) {
            entries.push_back({name + " x", "Shader", p, 0, child});
            entries.push_back({name + " y", "Shader", p, 1, child});
        } else if (IS(p, ofFloatColor)) {
            entries.push_back({name + " r", "Shader", p, 0, child});
            entries.push_back({name + " g", "Shader", p, 1, child});
            entries.push_back({name + " b", "Shader", p, 2, child});
        } else if (IS(p, float) || IS(p, int) || IS(p, bool) || IS(p, void)) {
            entries.push_back({name, "Shader", p, -1, child});
        }
    }
}

void ControlPages::build() {
    pages.clear();
    shaderVersion = app->shaderEngine->parametersVersion;
    StateManager &sm = *app->stateManager;
    DotsScene &scene = sm.activeScene;
    GuiApp &gui = *app->gui;
    LightingLayer &lighting = *app->lighting;

    pages.push_back({"flames", "Flames", "Drawing", {
        {"wander speed", "Wander", &sm.activeTrack.wanderSpeed},
        {"fft decay", "Audio Analysis", &scene.fftDecayRate},
        {"centroid max", "Audio Analysis", &scene.centroidMaxBucket},
        {"rms mult", "Audio Analysis", &scene.rmsMultiple},
        {"mpx smoothing", "Audio Analysis", &scene.mpxSmoothingFactor},
        {"mpy smoothing", "Audio Analysis", &scene.mpySmoothingFactor},
        {"clear speed", "Drawing", &scene.clearSpeed},
        {"particle alpha", "Drawing", &scene.particleAlpha},
        {"overall scale", "Drawing", &scene.overallScale},
        {"saturation", "Drawing", &scene.saturationPct},
        {"base speed", "Speed", &scene.baseSpeed},
        {"rms speed mult", "Speed", &scene.rmsSpeedMult},
        {"radius audio amt", "Dots", &scene.pointRadiusAudioScaleAmt},
        {"radius audio scale", "Dots", &scene.pointRadiusAudioScale},
        {"base dot radius", "Dots", &scene.basePointRadius},
        {"max line length", "Lines", &scene.maxLineLength},
    }});
    pages.push_back({"flames", "Flames 2", "Audio Effects", {
        {"motion id", "Audio Effects", &scene.motionId},
        {"effect size 1", "Audio Effects", &scene.audioEffectSize1},
        {"effect size 2", "Audio Effects", &scene.audioEffectSize2},
        {"effect size 3", "Audio Effects", &scene.audioEffectSize3},
        {"effect size 4", "Audio Effects", &scene.audioEffectSize4},
        {"lock audio", "Audio Analysis", &sm.lockAudioAnalysis},
        {"lock drawing", "Drawing", &sm.lockDrawing},
        {"lock speed", "Speed", &sm.lockSpeed},
        {"lock dots", "Dots", &sm.lockDots},
        {"lock lines", "Lines", &sm.lockLines},
        {"lock effects", "Audio Effects", &sm.lockAudioEffects},
        {"wandering", "Wander", &gui.wandering},
        {"fade frames", "Wander", &gui.fadeFrames},
    }});
    // The engines' on/off switches are buttons
    pages.push_back({"mixer", "Mixer", "Mixer", {
        {"crossfade A-B", "Mixer", &gui.crossfade},
        {"engine A", "Mixer", &gui.engineA},
        {"engine B", "Mixer", &gui.engineB},
        {"xfade centre", "Mixer", &gui.crossfadeCentre},
        {"preset fade", "Mixer", &gui.presetFadeSeconds},
        {"max fps", "Mixer", &gui.maxFps},
        {"audio mode", "Audio Analysis", &gui.audioMode},
        {"lights on feed A", "Lights", &lighting.feedAIncludesLights},
        {"feed B source", "Lights", &lighting.feedBSource},
    }});
    pages.push_back({"milk", "MilkDrop", "MilkDrop", {
        {"preset seconds", "MilkDrop", &gui.milkdropPresetDuration},
        {"lock preset", "MilkDrop", &gui.milkdropLockPreset},
        {"shuffle", "MilkDrop", &gui.milkdropShuffle},
        {"transition secs", "MilkDrop", &gui.milkdropTransitionSeconds},
        {"beat sensitivity", "MilkDrop", &gui.milkdropBeatSensitivity},
        {"hard cuts", "MilkDrop", &gui.milkdropHardCuts},
        {"hard cut sens", "MilkDrop", &gui.milkdropHardCutSensitivity},
        {"hard cut secs", "MilkDrop", &gui.milkdropHardCutSeconds},
        {"mesh size", "MilkDrop", &gui.milkdropMeshSize},
        {"aspect correct", "MilkDrop", &gui.milkdropAspectCorrection},
    }});
    SheepEngine &sheep = *app->sheepEngine;
    pages.push_back({"sheep", "Sheep", "Sheep", {
        {"sheep", "Sheep", &sheep.sheep},
        {"loop seconds", "Sheep", &sheep.loopSeconds},
        {"morph seconds", "Sheep", &sheep.morphSeconds},
        {"loops a sheep", "Sheep", &sheep.loopsPerSheep},
        {"points (k)", "Sheep", &sheep.pointsK},
        {"passes", "Sheep", &sheep.passes},
        {"trail", "Sheep", &sheep.trail},
        {"exposure", "Sheep", &sheep.exposure},
        {"audio speed", "Audio Analysis", &sheep.audioSpeed},
        {"audio twist", "Audio Analysis", &sheep.audioTwist},
        {"audio zoom", "Audio Analysis", &sheep.audioZoom},
        {"audio bright", "Audio Analysis", &sheep.audioBright},
        {"audio colour", "Audio Analysis", &sheep.audioColour},
        {"gamma", "Sheep", &sheep.gammaScale},
        {"running", "Sheep", &gui.sheepRunning},
        {"auto advance", "Sheep", &sheep.autoAdvance},
        {"shuffle", "Sheep", &sheep.shuffle},
    }});
    // Sets have no settings of their own: the fade between entries and what's on the mixer
    pages.push_back({"sets", "Sets", "Sets", {
        {"preset fade", "Sets", &gui.presetFadeSeconds},
        {"engine A", "Mixer", &gui.engineA},
        {"engine B", "Mixer", &gui.engineB},
        {"crossfade A-B", "Mixer", &gui.crossfade},
    }});

    // The shader's inputs, as many pages as they take: 16 dials and 7 switches to a page
    vector<ControlEntry> inputs, dials, switches;
    addShaderInputs(app->shaderEngine->getShader().parameters, inputs);
    for (const ControlEntry &input : inputs) (input.isSwitch() ? switches : dials).push_back(input);
    const std::size_t perPage = ControlPage::SWITCHES_PER_PAGE;
    std::size_t count = std::max({(dials.size() + 15) / 16, (switches.size() + perPage - 1) / perPage, (std::size_t)1});
    for (std::size_t n = 0; n < count; ++n) {
        ControlPage page{"shader", n == 0 ? "Shader" : "Shader " + ofToString(n + 1), "Shader", {}};
        for (std::size_t i = n * 16; i < dials.size() && i < n * 16 + 16; ++i) page.entries.push_back(dials[i]);
        for (std::size_t i = n * perPage; i < switches.size() && i < (n + 1) * perPage; ++i) page.switches.push_back(switches[i]);
        pages.push_back(page);
    }
    pages.push_back({"shader", "Shader set", "Shader", {
        {"animated only", "Shader", &gui.shaderAnimatedOnly},
        {"kinds all/ISF/syn", "Shader", &gui.shaderSources},
        {"media 0-4", "Shader", &gui.shaderMedia},
        {"alpha out", "Shader", &gui.shaderAlpha},
    }});

    // The lights (their on/off switches are buttons): the shared settings and each light's
    // intensity, hue and size, then each one's saturation, position and focus
    ControlPage lights{"lights", "Lights", "Lights", {
        {"ambient", "Lights", &lighting.ambient},
        {"lights on feed A", "Lights", &lighting.feedAIncludesLights},
        {"feed B source", "Lights", &lighting.feedBSource},
    }};
    ControlPage lights2{"lights", "Lights 2", "Lights", {}};
    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        Light &light = lighting.lights[i];
        string n = string(1, 'A' + i);
        lights.entries.push_back({n + " intensity", "Lights", &light.intensity});
        lights.entries.push_back({n + " hue", "Lights", &light.hue});
        lights.entries.push_back({n + " size", "Lights", &light.size});
        lights2.entries.push_back({n + " saturation", "Lights", &light.saturation});
        lights2.entries.push_back({n + " x", "Lights", &light.x});
        lights2.entries.push_back({n + " y", "Lights", &light.y});
        lights2.entries.push_back({n + " focus", "Lights", &light.focus});
    }
    pages.push_back(lights);
    pages.push_back(lights2);

    // Every page's bools and triggers go on its buttons, as many as there are buttons for
    for (ControlPage &page : pages) {
        for (auto entry = page.entries.begin(); entry != page.entries.end();) {
            if (entry->isSwitch() && (int)page.switches.size() < ControlPage::SWITCHES_PER_PAGE) {
                page.switches.push_back(*entry);
                entry = page.entries.erase(entry);
            } else {
                ++entry;
            }
        }
    }
}
