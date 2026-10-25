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
    // Recalls over the GUI's preset fade time
    bool recall(int module, const string &name);
    // Moves the file into a deleted/ folder beside it, so nothing is lost
    bool remove(int module, const string &name);

    ofJson capture(int module, bool globalIncludesLights = false);
    // With fadeSeconds, values glide and each engine changes over the fade
    void apply(int module, const ofJson &state, float fadeSeconds = 0);

    // Letters, digits, space, - and _; anything else becomes _
    static string cleanName(const string &name);

    // Sets: ordered lists of global presets (bin/data/sets/<name>.json),
    // stepped through with fades. Changes to the current set save at once.
    struct SetEntry {
        string preset;      // a global preset's name
        float fade = -1;    // seconds; -1 uses the GUI's preset fade
    };
    vector<string> listSets();
    bool openSet(const string &name);     // becomes the current set, nothing recalled yet
    bool newSet(const string &name);
    bool removeSet(const string &name);   // into sets/deleted/
    bool saveSet();
    void goTo(int position);              // recalls that entry over its fade
    void nextInSet();
    void previousInSet();

    string setName;
    vector<SetEntry> setEntries;
    int setPosition = -1;

private:
    string path(int module, const string &name);
    void applyModule(int module, const ofJson &state, float fadeSeconds);

    ofApp &app;
};
