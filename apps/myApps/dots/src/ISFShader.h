//
//  ISFShader.h
//  dots
//
//  One ISF (Interactive Shader Format) generator: a GLSL fragment shader with
//  a JSON header declaring its inputs. The header becomes ofParameters, and
//  the old-style GLSL ISF uses (gl_FragColor, isf_FragNormCoord, IMG_*) is
//  translated for openFrameworks' core-profile context. Single pass only.
//

#pragma once

#include "ofMain.h"

class ISFShader {
public:
    // False, with getError(), if the file can't be read, parsed or compiled
    bool load(const string &path);

    // Draws a frame into target; time is the shader's own clock in seconds
    void render(ofFbo &target, float time, float timeDelta, int frameIndex);

    const string &getName() const { return name; }
    const string &getError() const { return error; }
    const string &getPath() const { return path; }

    // The shader's inputs as parameters, for the GUI and MIDI
    ofParameterGroup parameters;

    // What a file is, from its header: dots's shader engine runs single-pass
    // generators; transitions (start and end image) and filters are left out
    enum Kind { UNUSABLE, FILTER, TRANSITION, NEEDS_IMAGE, STATIC_GENERATOR, ANIMATED_GENERATOR };
    static Kind classify(const string &path);

private:
    struct Input {
        string name, type;
        ofParameter<float> floatValue;
        ofParameter<bool> boolValue;
        ofParameter<int> intValue;           // long: an index into values
        vector<int> values;
        ofParameter<ofFloatColor> colorValue;
        ofParameter<glm::vec2> pointValue;
    };

    string path, name, error;
    vector<Input> inputs;
    ofShader shader;
    ofMesh quad;
    ofFbo raw;          // rendered GL-style (y up), flipped into the target
    ofTexture blank;    // stands in for image and audio inputs for now
};
