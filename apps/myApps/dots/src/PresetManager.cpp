//
//  PresetManager.cpp
//  dots
//

#include "PresetManager.h"
#include "ofApp.h"

static const int PRESET_FORMAT = 1;

string PresetManager::moduleName(int module) {
    switch (module) {
        case PRESET_GLOBAL:   return "global";
        case PRESET_FLAMES:   return "flames";
        case PRESET_MILKDROP: return "milkdrop";
        case PRESET_SHADER:   return "shader";
        case PRESET_LIGHTS:   return "lights";
    }
    return "unknown";
}

string PresetManager::folder(int module) {
    return ofToDataPath("presets/" + moduleName(module), true);
}

string PresetManager::path(int module, const string &name) {
    return ofFilePath::join(folder(module), cleanName(name) + ".json");
}

string PresetManager::cleanName(const string &name) {
    string clean;
    for (char c : ofTrim(name)) {
        clean += (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') ? c : '_';
    }
    return clean.empty() ? "untitled" : clean;
}

vector<string> PresetManager::list(int module) {
    vector<string> names;
    ofDirectory dir(folder(module));
    if (!dir.exists()) return names;
    dir.allowExt("json");
    dir.listDir();
    for (auto &file : dir.getFiles()) {
        names.push_back(file.getBaseName());
    }
    std::sort(names.begin(), names.end(), [](const string &a, const string &b) { return ofToLower(a) < ofToLower(b); });
    return names;
}

bool PresetManager::save(int module, const string &name, bool globalIncludesLights) {
    ofDirectory::createDirectory(folder(module), false, true);
    ofJson state = capture(module, globalIncludesLights);
    state["module"] = moduleName(module);
    state["format"] = PRESET_FORMAT;
    bool saved = ofSavePrettyJson(path(module, name), state);
    ofLogNotice("PresetManager") << (saved ? "Saved " : "Couldn't save ") << moduleName(module) << " preset " << cleanName(name);
    return saved;
}

bool PresetManager::recall(int module, const string &name) {
    ofJson state = ofLoadJson(path(module, name));
    if (state.is_null() || state.empty()) {
        ofLogWarning("PresetManager") << "No " << moduleName(module) << " preset " << name;
        return false;
    }
    apply(module, state, app.gui->presetFadeSeconds);
    ofLogNotice("PresetManager") << "Recalled " << moduleName(module) << " preset " << name;
    return true;
}

bool PresetManager::remove(int module, const string &name) {
    string from = path(module, name);
    if (!ofFile::doesFileExist(from, false)) return false;
    string deleted = ofFilePath::join(folder(module), "deleted");
    ofDirectory::createDirectory(deleted, false, true);
    string to = ofFilePath::join(deleted, cleanName(name) + "-" + ofGetTimestampString("%Y%m%d-%H%M%S") + ".json");
    return ofFile::moveFromTo(from, to, false, false);
}

// Paths inside a folder are stored relative to it, so presets survive moving dots
static string relativeTo(const string &folder, const string &path) {
    if (!folder.empty() && path.rfind(folder, 0) == 0) return ofFilePath::makeRelative(folder, path);
    return path;
}

static string resolveFrom(const string &folder, const string &path) {
    return ofFilePath::isAbsolute(path) ? path : ofFilePath::join(folder, path);
}

ofJson PresetManager::capture(int module, bool globalIncludesLights) {
    ofJson state;
    StateManager &sm = *app.stateManager;
    GuiApp &gui = *app.gui;

    switch (module) {
        case PRESET_FLAMES:
            state["track"] = sm.getTrack().trackId;
            state["scene"] = sm.sceneIdx;
            state["wandering"] = gui.wandering.get();
            ofSerialize(state, sm.activeScene.displayParameters);
            ofSerialize(state, sm.activeTrack.displayParameters);
            break;

        case PRESET_MILKDROP:
            state["preset"] = relativeTo(app.projectM.getPresetsFolder(), app.projectM.getPresetPath());
            ofSerialize(state, gui.milkdropParams);
            break;

        case PRESET_SHADER: {
            state["shader"] = app.shaderEngine->getCurrentPath();
            ofJson settings, inputs;
            ofSerialize(settings, gui.shaderParams);
            ofSerialize(inputs, app.shaderEngine->getShader().parameters);
            state["settings"] = settings;
            state["inputs"] = inputs;
            // The settings hold the kind of media; which file or Syphon feed is here
            MediaInput &media = app.shaderEngine->getMedia();
            state["media"] = {{"file", media.getFile()}, {"syphonServer", media.getSyphonServer()}, {"syphonApp", media.getSyphonApp()}};
            break;
        }

        case PRESET_LIGHTS: {
            LightingLayer &lighting = *app.lighting;
            ofSerialize(state, lighting.ambient);
            ofSerialize(state, lighting.feedAIncludesLights);
            ofSerialize(state, lighting.feedBSource);
            for (Light &light : lighting.lights) {
                ofSerialize(state, light.group);
            }
            break;
        }

        case PRESET_GLOBAL:
            ofSerialize(state, gui.mixerParams);
            state["flames"] = capture(PRESET_FLAMES);
            state["milkdrop"] = capture(PRESET_MILKDROP);
            state["shader"] = capture(PRESET_SHADER);
            if (globalIncludesLights) state["lights"] = capture(PRESET_LIGHTS);
            break;
    }
    return state;
}

void PresetManager::apply(int module, const ofJson &state, float fadeSeconds) {
    app.fader.begin(fadeSeconds);
    applyModule(module, state, fadeSeconds);
}

void PresetManager::applyModule(int module, const ofJson &state, float fade) {
    StateManager &sm = *app.stateManager;
    GuiApp &gui = *app.gui;
    Fader &fader = app.fader;

    switch (module) {
        case PRESET_FLAMES: {
            int trackIndex = sm.trackIndexForId(state.value("track", -1));
            int scene = state.value("scene", -1);
            bool sameScene = (trackIndex < 0 || trackIndex == sm.trackIdx) && (scene < 0 || scene == sm.sceneIdx);
            if (trackIndex >= 0 && trackIndex != sm.trackIdx) {
                // Another track: the old flames fade out from a frozen frame
                if (fade > 0) app.freezeFlames(fade);
                sm.selectTrack(trackIndex);
                if (scene >= 0) sm.selectScene(scene);
            } else if (scene >= 0 && scene != sm.sceneIdx) {
                // Same track: the flames' own scene fade, a little shorter so it lands first
                int frames = fade * std::max(10.0f, ofGetFrameRate()) * 0.9f;
                sm.fadeToScene(scene, frames);
            }
            gui.wandering = state.value("wandering", false);
            fader.apply(state, sm.activeTrack.displayParameters);
            // Locked blocks keep their values
            string sceneKey = sm.activeScene.displayParameters.getEscapedName();
            ofJson sceneValues = state.value(sceneKey, ofJson::object());
            auto applyUnlocked = [&sm](const ofJson &values, std::function<void(const ofJson &, ofAbstractParameter &)> set) {
                ofParameterGroup &scene = sm.activeScene.displayParameters;
                for (std::size_t i = 0; i < scene.size(); ++i) {
                    if (scene.getType(i) != typeid(ofParameterGroup).name()) continue;
                    ofParameterGroup block = scene.getGroup(i);
                    if (!sm.isLocked(block.getName())) set(values, block);
                }
            };
            if (sameScene) {
                applyUnlocked(sceneValues, [&fader](const ofJson &values, ofAbstractParameter &block) { fader.apply(values, block); });
            } else {
                // The scene fade carries the scene's own values; the preset's land at the end
                fader.atEnd([&sm, sceneValues, applyUnlocked]() {
                    applyUnlocked(sceneValues, [](const ofJson &values, ofAbstractParameter &block) { ofDeserialize(values, block); });
                });
            }
            break;
        }

        case PRESET_MILKDROP: {
            fader.apply(state, gui.milkdropParams);
            string preset = state.value("preset", "");
            if (preset.empty()) break;
            string path = resolveFrom(app.projectM.getPresetsFolder(), preset);
            if (path == app.projectM.getPresetPath()) break;
            // projectM's own blend, stretched to the fade
            if (fade > 0) {
                app.projectM.setSoftCutDuration(fade);
                fader.atEnd([this]() { app.projectM.setSoftCutDuration(app.gui->milkdropTransitionSeconds); });
            }
            app.projectM.loadPresetFile(path, fade > 0);
            break;
        }

        case PRESET_SHADER: {
            if (state.contains("media") && state["media"].is_object()) {
                MediaInput &media = app.shaderEngine->getMedia();
                const ofJson &saved = state["media"];
                string file = saved.value("file", ""), server = saved.value("syphonServer", ""), syphonApp = saved.value("syphonApp", "");
                if (!file.empty() && file != media.getFile()) media.loadFile(file);
                if ((!server.empty() || !syphonApp.empty()) && (server != media.getSyphonServer() || syphonApp != media.getSyphonApp())) {
                    media.useSyphon(server, syphonApp);
                }
            }
            // The settings then pick which kind is in use
            if (state.contains("settings")) fader.apply(state["settings"], gui.shaderParams);
            string shader = state.value("shader", "");
            if (!shader.empty() && shader != app.shaderEngine->getCurrentPath()) {
                // Another shader: it fades in over the old one, which keeps running
                app.shaderEngine->loadPath(shader, fade);
                if (state.contains("inputs")) ofDeserialize(state["inputs"], app.shaderEngine->getShader().parameters);
            } else if (state.contains("inputs")) {
                fader.apply(state["inputs"], app.shaderEngine->getShader().parameters);
            }
            break;
        }

        case PRESET_LIGHTS: {
            LightingLayer &lighting = *app.lighting;
            fader.apply(state, lighting.ambient);
            fader.apply(state, lighting.feedAIncludesLights);
            fader.apply(state, lighting.feedBSource);
            for (Light &light : lighting.lights) {
                string name = light.group.getEscapedName();
                if (!state.contains(name)) continue;
                ofJson target = state[name];
                bool turnOn = target.value("on", "0") == "1";
                if (turnOn && !light.on) {
                    // Switching on: up from nothing
                    light.intensity = 0;
                    fader.apply(state, light.group);
                } else if (!turnOn && light.on) {
                    // Switching off: down to nothing, then off, holding the stored intensity
                    float storedIntensity = ofToFloat(target.value("intensity", "0"));
                    ofJson rest = state;
                    rest[name].erase("on");
                    rest[name].erase("intensity");
                    fader.apply(rest, light.group);
                    fader.to(light.intensity, 0.0f);
                    fader.atEnd([&light, storedIntensity]() {
                        light.on = false;
                        light.intensity = storedIntensity;
                    });
                } else {
                    fader.apply(state, light.group);
                }
            }
            break;
        }

        case PRESET_GLOBAL: {
            // A different engine on A or B: the whole picture fades from a frozen frame
            int engineA = gui.engineA, engineB = gui.engineB;
            fader.apply(state, gui.mixerParams);
            if (fade > 0 && (gui.engineA != engineA || gui.engineB != engineB)) app.freezeOutput(fade);
            if (state.contains("flames")) applyModule(PRESET_FLAMES, state["flames"], fade);
            if (state.contains("milkdrop")) applyModule(PRESET_MILKDROP, state["milkdrop"], fade);
            if (state.contains("shader")) applyModule(PRESET_SHADER, state["shader"], fade);
            if (state.contains("lights")) applyModule(PRESET_LIGHTS, state["lights"], fade);
            break;
        }
    }
}

// Sets

static string setsFolder() {
    return ofToDataPath("sets", true);
}

vector<string> PresetManager::listSets() {
    vector<string> names;
    ofDirectory dir(setsFolder());
    if (!dir.exists()) return names;
    dir.allowExt("json");
    dir.listDir();
    for (auto &file : dir.getFiles()) names.push_back(file.getBaseName());
    std::sort(names.begin(), names.end(), [](const string &a, const string &b) { return ofToLower(a) < ofToLower(b); });
    return names;
}

bool PresetManager::openSet(const string &name) {
    ofJson json = ofLoadJson(ofFilePath::join(setsFolder(), cleanName(name) + ".json"));
    if (json.is_null()) return false;
    setName = cleanName(name);
    setEntries.clear();
    for (auto &entry : json.value("entries", ofJson::array())) {
        setEntries.push_back({entry.value("preset", ""), entry.value("fade", -1.0f)});
    }
    setPosition = -1;
    return true;
}

bool PresetManager::newSet(const string &name) {
    setName = cleanName(name);
    setEntries.clear();
    setPosition = -1;
    return saveSet();
}

bool PresetManager::saveSet() {
    if (setName.empty()) return false;
    ofDirectory::createDirectory(setsFolder(), false, true);
    ofJson json;
    json["format"] = PRESET_FORMAT;
    json["entries"] = ofJson::array();
    for (const SetEntry &entry : setEntries) {
        json["entries"].push_back({{"preset", entry.preset}, {"fade", entry.fade}});
    }
    return ofSavePrettyJson(ofFilePath::join(setsFolder(), setName + ".json"), json);
}

bool PresetManager::removeSet(const string &name) {
    string from = ofFilePath::join(setsFolder(), cleanName(name) + ".json");
    if (!ofFile::doesFileExist(from, false)) return false;
    string deleted = ofFilePath::join(setsFolder(), "deleted");
    ofDirectory::createDirectory(deleted, false, true);
    if (cleanName(name) == setName) {
        setName.clear();
        setEntries.clear();
        setPosition = -1;
    }
    return ofFile::moveFromTo(from, ofFilePath::join(deleted, cleanName(name) + "-" + ofGetTimestampString("%Y%m%d-%H%M%S") + ".json"), false, false);
}

void PresetManager::goTo(int position) {
    if (position < 0 || position >= (int)setEntries.size()) return;
    const SetEntry &entry = setEntries[position];
    ofJson state = ofLoadJson(path(PRESET_GLOBAL, entry.preset));
    if (state.is_null() || state.empty()) {
        ofLogWarning("PresetManager") << "Set " << setName << ": no global preset " << entry.preset;
        return;
    }
    setPosition = position;
    apply(PRESET_GLOBAL, state, entry.fade >= 0 ? entry.fade : app.gui->presetFadeSeconds.get());
    ofLogNotice("PresetManager") << "Set " << setName << " " << position + 1 << "/" << setEntries.size() << ": " << entry.preset;
}

void PresetManager::nextInSet() {
    if (!setEntries.empty()) goTo(std::min(setPosition + 1, (int)setEntries.size() - 1));
}

void PresetManager::previousInSet() {
    if (!setEntries.empty()) goTo(std::max(setPosition - 1, 0));
}
