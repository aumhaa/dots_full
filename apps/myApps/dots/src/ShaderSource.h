//
//  ShaderSource.h
//  dots
//
//  What the shader engine runs: an ISF generator (ISFShader) or a Synesthesia
//  scene (SynScene). Each draws frames into an ofFbo and shows its inputs as
//  ofParameters.
//

#pragma once

#include "ofMain.h"

class ShaderSource {
public:
    virtual ~ShaderSource() {}

    // False, with getError(), if it can't be read, parsed or compiled
    virtual bool load(const string &path) = 0;

    // Draws a frame into target; time is the shader's own clock in seconds
    virtual void render(ofFbo &target, float time, float timeDelta, int frameIndex) = 0;

    const string &getName() const { return name; }
    const string &getError() const { return error; }
    const string &getPath() const { return path; }

    // The inputs as parameters, for the GUI, MIDI and presets
    ofParameterGroup parameters;

protected:
    string path, name, error;
};
