//
//  SynScene.h
//  dots
//
//  One Synesthesia scene (a .synScene folder: main.glsl and scene.json, with
//  optional images). The scene's renderMain() runs once for each pass in
//  PASSES and once more for the picture; each pass keeps its last frame, so
//  scenes can feed back on themselves. Synesthesia's built-in uniforms and
//  functions are supplied by dots (SynLibrary.h), and its audio uniforms
//  come from SynAudio. Controls become ofParameters, with Synesthesia's
//  smooth, speed, bang and counter behaviours applied each frame.
//
//  Not yet: script.js (its uniforms are declared and stay at 0), media input
//  (syn_UserImage is empty, so scenes take their no-media path), and the
//  song-change transitions (each takes a random value when the scene loads).
//

#pragma once

#include "ShaderSource.h"

class SynAudio;

class SynScene : public ShaderSource {
public:
    explicit SynScene(const SynAudio *audio = nullptr) : audio(audio) {}

    bool load(const string &folder) override;
    void render(ofFbo &target, float time, float timeDelta, int frameIndex) override;

    // A folder with main.glsl and scene.json
    static bool isScene(const string &folder);

private:
    struct Control {
        string uniform, type;
        bool smooth = false, speed = false, counter = false;
        float rate = 0.01f;            // PARAMS: the share of the way a smooth control moves each frame
        float minValue[3] = {0, 0, 0}, maxValue[3] = {1, 1, 1};
        vector<float> dropdownValues;

        ofParameter<float> floatValue;
        ofParameter<bool> boolValue;
        ofParameter<int> intValue;
        ofParameter<glm::vec2> pointValue;
        ofParameter<ofFloatColor> colorValue;
        ofParameter<void> bang;
        ofEventListener bangListener;
        bool banged = false;

        glm::vec3 output{0};           // what the shader gets
        glm::vec3 velocity{0};         // speed controls, eased when smooth
        float envelope = 0;            // bang smooth
        bool rising = false;
        float count = 0;               // bang counter
    };
    void addControl(const ofJson &spec, std::map<string, ofParameterGroup> &groups, vector<string> &groupOrder);
    glm::vec3 target(Control &control);
    void updateControls(float dt);

    struct Pass {
        string target;
        float width = 0, height = 0;   // in pixels; 0 means the picture's size
        bool isFloat = true;
        ofFbo buffers[2];
        int front = 0;                 // the finished frame; the other is drawn into
    };
    void allocatePass(Pass &pass, int width, int height);

    const SynAudio *audio;
    vector<std::unique_ptr<Control>> controls;
    vector<Pass> passes;               // the scene's PASSES, then the picture
    std::map<string, ofTexture> images;
    vector<std::pair<string, int>> vectorUniforms;   // script.js outputs: name, size
    std::map<string, float> transitionValues;        // HARD_ and SMOOTH_TRANSITIONS

    ofShader shader;
    ofTexture blank;                   // syn_UserImage, with no media
    glm::vec2 quadSize{0};             // the picture size the passes are allocated for
};
