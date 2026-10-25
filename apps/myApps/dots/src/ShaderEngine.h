//
//  ShaderEngine.h
//  dots
//
//  The third engine: ISF generator shaders from the system ISF folders and
//  bin/data/shaders, one at a time, drawn into an ofFbo. A shader whose file
//  changes is reloaded, so shaders can be edited live.
//

#pragma once

#include "ofMain.h"
#include "ISFShader.h"

class ShaderEngine {
public:
    void setup(int width, int height);
    void resize(int width, int height);

    // Advances the shader's clock and draws a frame into getFbo()
    void render();
    ofFbo &getFbo() { return fbo; }

    void next();
    void previous();
    void select(int index);

    // Only the generators that animate on their own, or the static patterns too
    void setAnimatedOnly(bool animatedOnly);

    // Loads a shader by its file, whether or not it's in the current list
    void loadPath(const string &path);
    string getCurrentPath() const { return index >= 0 && index < (int)library.size() ? library[index] : ""; }

    // Every generator found, for the file browser
    struct Entry {
        string path;
        bool animated;
    };
    const vector<Entry> &getAllShaders() const { return allShaders; }

    // Call about once a second: reloads the shader if its file changed
    void checkForChanges();

    ISFShader &getShader() { return shader; }
    int getIndex() const { return index; }
    int getCount() const { return library.size(); }
    string getStatus() const;

    // Changes whenever a different shader (or reloaded one) is in, so the GUI can rebuild its panel
    int parametersVersion = 0;

private:
    void load(int newIndex);

    vector<Entry> allShaders;   // every single-pass generator found
    vector<string> library;     // the ones on offer now
    bool animatedOnly = true;
    int index = -1;
    ISFShader shader;
    bool shaderOk = false;
    std::filesystem::file_time_type loadedFileTime;

    ofFbo fbo;
    float time = 0, lastFrameTime = 0;
    int frameIndex = 0;
};
