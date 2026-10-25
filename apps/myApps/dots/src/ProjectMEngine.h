//
//  ProjectMEngine.h
//  dots
//
//  MilkDrop presets through libprojectM, drawn into an ofFbo so dots can mix
//  them with the flames. Fed from the same audio input as the flames.
//

#pragma once

#include "ofMain.h"
#include <mutex>

#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>

class ProjectMEngine {
public:
    ~ProjectMEngine();

    // dataPath holds presets/ and textures/ (bin/data/projectM). Needs the GL context current.
    void setup(int width, int height, const string &dataPath);
    void resize(int width, int height);

    // Any thread: queue mono samples for the next frame
    void addSamples(const float *samples, int count);

    // Main thread: hand queued audio to projectM and draw a frame into getFbo()
    void render();
    ofFbo &getFbo() { return fbo; }

    void nextPreset(bool hardCut = false);
    void previousPreset(bool hardCut = false);
    void setPresetDuration(double seconds);
    void setPresetLocked(bool locked);
    void setShuffle(bool shuffle);
    string getPresetName();
    string getPresetPath();
    const string &getPresetsFolder() const { return presetsFolder; }
    // Loads one preset file; smooth blends it in over the soft-cut duration
    void loadPresetFile(const string &path, bool smooth);
    void setSoftCutDuration(double seconds);
    void setBeatSensitivity(float sensitivity);
    void setHardCuts(bool enabled, float sensitivity, double seconds);
    void setMeshSize(int across);   // 3/4 of it down
    void setAspectCorrection(bool enabled);
    bool isReady() { return projectM != nullptr; }

private:
    static void presetSwitched(bool isHardCut, unsigned int index, void *userData);

    projectm_handle projectM = nullptr;
    projectm_playlist_handle playlist = nullptr;
    ofFbo fbo;

    // openFrameworks caches which shader it has bound; projectM changes it behind
    // its back, so a bind and unbind through ofShader resets that cache
    ofShader resetShader;

    std::mutex audioMutex;
    vector<float> pendingSamples, renderSamples;

    std::mutex nameMutex;
    string presetName, presetPath;
    string presetsFolder;
};
