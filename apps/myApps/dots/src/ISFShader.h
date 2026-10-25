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

#include "ShaderSource.h"

class ISFShader : public ShaderSource {
public:
    bool load(const string &path) override;
    void render(ofFbo &target, float time, float timeDelta, int frameIndex) override;

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

    vector<Input> inputs;
    ofShader shader;
    ofMesh quad;
    ofFbo raw;          // rendered GL-style (y up), flipped into the target
    ofTexture blank;    // stands in for image and audio inputs for now
};
