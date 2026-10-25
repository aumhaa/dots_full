//
//  ControlPages.h
//  dots
//
//  dots's parameters in pages of up to 16, each entry with a label, a
//  section (for its colour) and a way to read and set it as 0-1. A controller
//  with a bank of 16 knobs pages through these; it is the start of the one
//  parameter layer every UI is meant to share. The shader pages follow the
//  shader running, so they're rebuilt when it changes.
//

#pragma once

#include "ofMain.h"

class ofApp;

struct ControlEntry {
    string label, section;
    ofAbstractParameter *param = nullptr;
    int component = -1;   // a point's x/y or a colour's r/g/b; -1 for the whole parameter
    shared_ptr<ofAbstractParameter> keepAlive;   // a shader input, which goes when the shader does

    float normalized() const;            // 0-1 across its range
    void setNormalized(float t) const;   // a bool is on from 0.5; a trigger fires from 0.5
    string valueText() const;
};

struct ControlPage {
    string group;   // pages of one group share a button: a press goes to the group, another to its next page
    string name, section;
    vector<ControlEntry> entries;   // at most 16
};

class ControlPages {
public:
    void setup(ofApp *app);
    // Rebuilds the shader pages when the shader changes; true if anything did
    bool update();

    const vector<ControlPage> &get() const { return pages; }

private:
    void build();
    void addShaderInputs(ofParameterGroup &group, vector<ControlEntry> &entries);

    ofApp *app = nullptr;
    vector<ControlPage> pages;
    int shaderVersion = -1;
};
