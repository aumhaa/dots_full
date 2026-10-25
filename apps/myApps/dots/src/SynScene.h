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
//  A scene's script.js runs each frame before the passes (SynScript), and its
//  media (syn_UserImage) comes from MediaInput. Not yet: the song-change
//  transitions (each takes a random value when the scene loads).
//

#pragma once

#include "ShaderSource.h"
#include "MediaInput.h"

class SynAudio;
class SynScript;

class SynScene : public ShaderSource {
public:
    // Out of line, where SynScript is complete
    // media is what scenes get as syn_UserImage; null or empty for none
    explicit SynScene(const SynAudio *audio = nullptr, const ShaderMedia *media = nullptr);
    ~SynScene();

    bool load(const string &folder) override;
    void render(ofFbo &target, float time, float timeDelta, int frameIndex) override;

    // A folder with main.glsl and scene.json
    static bool isScene(const string &folder);

    // The first error its script.js hit, if any (the scene runs on regardless)
    string getScriptError() const;

    // For the script: change controls by name, or a whole UI group, as Synesthesia's
    // setControl(), setControlNormalized(), randomizeControl() and defaultControl() do
    enum ControlChange { CONTROL_SET, CONTROL_SET_NORMALIZED, CONTROL_RANDOMIZE, CONTROL_DEFAULT };
    void changeControls(const string &normalizedName, bool group, int change, const float *values, int count, int dimension);
    // The last picture's colour at (x, y), 0 to 1 from the bottom left
    glm::vec3 finalPassPixel(float x, float y);

private:
    struct Control {
        string uniform, type, group;
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
        glm::vec3 defaults{0};         // as target() gives them; a dropdown's index
    };
    void changeControl(Control &control, int change, const float *values, int count, int dimension);
    void feedScript(float time, int frameIndex, float width, float height, const vector<float> &audioValues);
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
    const ShaderMedia *media;
    vector<std::unique_ptr<Control>> controls;
    vector<Pass> passes;               // the scene's PASSES, then the picture
    std::map<string, ofTexture> images;
    vector<std::pair<string, int>> vectorUniforms;   // script.js outputs: name, size
    std::unique_ptr<SynScript> script;
    std::map<string, float> transitionValues;        // HARD_ and SMOOTH_TRANSITIONS

    ofShader shader;
    ofTexture blank;                   // syn_UserImage, with no media
    float mediaType() const { return media && media->texture ? media->type : 0; }
    glm::vec2 quadSize{0};             // the picture size the passes are allocated for
};
