//
//  ShaderEngine.h
//  dots
//
//  The third engine: ISF generator shaders from the system ISF folders and
//  bin/data/shaders, and Synesthesia scenes from Synesthesia's scene folder,
//  the app itself and bin/data/synesthesia, one at a time, drawn into an
//  ofFbo. A shader whose files change is reloaded, so it can be edited live.
//

#pragma once

#include "ofMain.h"
#include "ShaderSource.h"
#include "MediaInput.h"

class SynAudio;

class ShaderEngine {
public:
    // audio feeds the Synesthesia scenes' audio uniforms
    void setup(int width, int height, const SynAudio *audio = nullptr);
    void resize(int width, int height);

    // Advances the shader's clock and draws a frame into getFbo()
    void render();

    // The latest audio for ISF's audio inputs: samples (-1 to 1) and spectrum (0 to 1)
    void setAudioInput(const float *samples, int sampleCount, const float *bins, int binCount);

    // Media for Synesthesia scenes: choose it here, and update it each frame before render()
    MediaInput &getMedia() { return media; }
    void updateMedia(ofFbo &flames, ofFbo &milkdrop);
    ofFbo &getFbo() { return fbo; }

    void next();
    void previous();
    void select(int index);

    // Only the generators that animate on their own, or the static patterns too;
    // and which kinds are on offer
    enum Sources { ALL_SOURCES, ISF_ONLY, SYNESTHESIA_ONLY };
    void setFilter(bool animatedOnly, int sources);

    // Loads a shader by its file, whether or not it's in the current list.
    // With fadeSeconds, the old shader keeps running and fades out under the new one.
    void loadPath(const string &path, float fadeSeconds = 0);
    string getCurrentPath() const { return index >= 0 && index < (int)library.size() ? library[index] : ""; }

    // Every generator found, for the file browser
    struct Entry {
        string path, name;
        bool animated, synesthesia;
        string thumbnail;   // a Synesthesia scene's preview image, if it has one
    };
    const vector<Entry> &getAllShaders() const { return allShaders; }

    // Call about once a second: reloads the shader if its file changed
    void checkForChanges();

    // Loads and runs every Synesthesia scene (and ISF shader, with includeISF)
    // for 30 frames, printing a line for each to stdout
    void testAll(bool includeISF);

    ShaderSource &getShader() { return *shader; }
    int getIndex() const { return index; }
    int getCount() const { return library.size(); }
    string getStatus() const;

    // Changes whenever a different shader (or reloaded one) is in, so the GUI can rebuild its panel
    int parametersVersion = 0;

private:
    void load(int newIndex, float fadeSeconds = 0);
    std::unique_ptr<ShaderSource> makeSource(const string &path);
    static std::filesystem::file_time_type fileTime(const string &path);
    void addSynesthesiaFolder(const string &folder);

    vector<Entry> allShaders;   // every single-pass generator and scene found
    vector<string> library;     // the ones on offer now
    bool animatedOnly = true;
    int sources = ALL_SOURCES;
    int index = -1;
    const SynAudio *audio = nullptr;
    std::unique_ptr<ShaderSource> shader;
    bool shaderOk = false;

    // A shader fading out under the new one
    std::unique_ptr<ShaderSource> outgoing;
    ofFbo outgoingFbo, incomingFbo;
    float fadeStart = 0, fadeSeconds = 0, outgoingTime = 0;
    int outgoingFrame = 0;
    std::filesystem::file_time_type loadedFileTime;

    ofTexture audioWaveform, audioSpectrum;   // one row each, for ISF's audio and audioFFT inputs
    MediaInput media;
    ShaderMedia currentMedia;                 // what scenes see this frame

    ofFbo fbo;
    float time = 0, lastFrameTime = 0;
    int frameIndex = 0;
};
