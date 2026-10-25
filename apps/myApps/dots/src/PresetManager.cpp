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
    apply(module, state);
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

void PresetManager::apply(int module, const ofJson &state) {
    StateManager &sm = *app.stateManager;
    GuiApp &gui = *app.gui;

    switch (module) {
        case PRESET_FLAMES: {
            int trackIndex = sm.trackIndexForId(state.value("track", -1));
            if (trackIndex >= 0 && trackIndex != sm.trackIdx) sm.selectTrack(trackIndex);
            int scene = state.value("scene", -1);
            if (scene >= 0 && scene != sm.sceneIdx) sm.selectScene(scene);
            ofDeserialize(state, sm.activeScene.displayParameters);
            ofDeserialize(state, sm.activeTrack.displayParameters);
            gui.wandering = state.value("wandering", false);
            break;
        }

        case PRESET_MILKDROP: {
            ofDeserialize(state, gui.milkdropParams);
            string preset = state.value("preset", "");
            if (!preset.empty()) app.projectM.loadPresetFile(resolveFrom(app.projectM.getPresetsFolder(), preset), false);
            break;
        }

        case PRESET_SHADER: {
            if (state.contains("settings")) ofDeserialize(state["settings"], gui.shaderParams);
            string shader = state.value("shader", "");
            if (!shader.empty() && shader != app.shaderEngine->getCurrentPath()) app.shaderEngine->loadPath(shader);
            if (state.contains("inputs")) ofDeserialize(state["inputs"], app.shaderEngine->getShader().parameters);
            break;
        }

        case PRESET_LIGHTS: {
            LightingLayer &lighting = *app.lighting;
            ofDeserialize(state, lighting.ambient);
            ofDeserialize(state, lighting.feedAIncludesLights);
            ofDeserialize(state, lighting.feedBSource);
            for (Light &light : lighting.lights) {
                ofDeserialize(state, light.group);
            }
            break;
        }

        case PRESET_GLOBAL:
            ofDeserialize(state, gui.mixerParams);
            if (state.contains("flames")) apply(PRESET_FLAMES, state["flames"]);
            if (state.contains("milkdrop")) apply(PRESET_MILKDROP, state["milkdrop"]);
            if (state.contains("shader")) apply(PRESET_SHADER, state["shader"]);
            if (state.contains("lights")) apply(PRESET_LIGHTS, state["lights"]);
            break;
    }
}
