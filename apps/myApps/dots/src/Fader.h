//
//  Fader.h
//  dots
//
//  Glides parameters to new values over a fade: floats, colours and points
//  ease from where they are to their target; ints, bools and strings change
//  straight away. Starting a new fade first snaps the old one to its end.
//

#pragma once

#include "ofMain.h"
#include <functional>

class Fader {
public:
    void begin(float seconds);

    // Like ofDeserialize(json, parameter), but gliding what can glide
    void apply(const ofJson &json, ofAbstractParameter &parameter);
    // One parameter to one value (its toString() form); keepAlive holds a
    // parameter that may be dropped from its group mid-fade (a shader's inputs)
    void to(ofAbstractParameter &parameter, const string &value, shared_ptr<ofAbstractParameter> keepAlive = nullptr);
    void to(ofParameter<float> &parameter, float value);

    // Runs when the fade ends (or at once when there's no fade)
    void atEnd(std::function<void()> action);

    void update();
    bool isActive() const { return active; }
    float getSeconds() const { return seconds; }

private:
    struct Tween {
        shared_ptr<ofAbstractParameter> keepAlive;
        ofAbstractParameter *parameter;
        int kind;   // 0 float, 1 colour, 2 point
        float fromFloat, toFloat;
        ofFloatColor fromColor, toColor;
        glm::vec2 fromPoint, toPoint;
    };
    void applyTo(const ofJson &json, ofAbstractParameter &parameter, shared_ptr<ofAbstractParameter> keepAlive);
    void finish();
    void set(Tween &tween, float t);

    vector<Tween> tweens;
    vector<std::function<void()>> endActions;
    float seconds = 0, startTime = 0;
    bool active = false;
};
