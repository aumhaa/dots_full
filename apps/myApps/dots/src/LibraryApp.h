//
//  LibraryApp.h
//  dots
//
//  The "dots library" window (ImGui): presets for each module and global ones,
//  and a browser for the files dots uses: flames tracks, MilkDrop presets,
//  shaders and light snapshots. Anything that changes the running app is posted
//  to ofApp, which does it in its own update (and GL context).
//

#pragma once

#include "ofMain.h"
#include "ofxImGui.h"
#include "PresetManager.h"

class ofApp;
class GuiApp;
struct GLFWwindow;

class LibraryApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);

    shared_ptr<ofApp> app;
    shared_ptr<GuiApp> gui;

private:
    void drawPresets();
    void drawModulePresets(int module, const string &title);
    void drawFiles();
    void drawSets();
    void refreshLists();
    void scanMilkdropPresets();

    ofxImGui::Gui imgui;
    GLFWwindow *window = nullptr;
    bool shown = true;

    // Presets
    vector<string> presetNames[PRESET_N_MODULES];
    int selectedPreset[PRESET_N_MODULES];
    char newName[PRESET_N_MODULES][64];
    bool globalIncludesLights = false;
    float lastRefresh = -10;

    // Sets
    vector<string> setNames;
    char newSetName[64] = "";
    int globalToAdd = 0;

    // Files
    vector<string> milkdropPresets;   // relative to the presets folder
    ImGuiTextFilter milkdropFilter, shaderFilter;
    vector<int> lightSnapshots;
};
