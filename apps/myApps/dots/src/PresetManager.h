//
//  PresetManager.h
//  dots
//
//  Presets for each module (flames, MilkDrop, shader, lights) and global ones
//  (the mixer plus every module), as JSON in bin/data/presets/<module>/<name>.json.
//  A global preset holds each module's state itself, so it doesn't depend on
//  any module preset file. Recalling changes the running app, so it happens in
//  ofApp's update, in the main window's GL context.
//

#pragma once

#include "ofMain.h"

class ofApp;

enum PresetModule {
    PRESET_GLOBAL,
    PRESET_FLAMES,
    PRESET_MILKDROP,
    PRESET_SHADER,
    PRESET_LIGHTS,
    PRESET_N_MODULES
};

class PresetManager {
public:
    PresetManager(ofApp &app) : app(app) {}

    static string moduleName(int module);   // also its folder: global, flames, milkdrop, shader, lights
    string folder(int module);
    vector<string> list(int module);        // preset names, sorted

    // A global preset leaves the lights out unless asked: they're usually per venue
    bool save(int module, const string &name, bool globalIncludesLights = false);
    bool recall(int module, const string &name);
    // Moves the file into a deleted/ folder beside it, so nothing is lost
    bool remove(int module, const string &name);

    ofJson capture(int module, bool globalIncludesLights = false);
    void apply(int module, const ofJson &state);

    // Letters, digits, space, - and _; anything else becomes _
    static string cleanName(const string &name);

private:
    string path(int module, const string &name);

    ofApp &app;
};
