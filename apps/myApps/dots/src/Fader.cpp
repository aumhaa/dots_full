//
//  Fader.cpp
//  dots
//

#include "Fader.h"

void Fader::begin(float fadeSeconds) {
    finish();
    seconds = std::max(0.0f, fadeSeconds);
    startTime = ofGetElapsedTimef();
    active = seconds > 0;
}

void Fader::to(ofAbstractParameter &parameter, const string &value, shared_ptr<ofAbstractParameter> keepAlive) {
    string type = parameter.type();
    Tween tween;
    tween.parameter = &parameter;
    tween.keepAlive = keepAlive;

    if (type == typeid(ofParameter<float>).name()) {
        tween.kind = 0;
        tween.fromFloat = parameter.cast<float>().get();
        tween.toFloat = ofToFloat(value);
    } else if (type == typeid(ofParameter<ofFloatColor>).name()) {
        tween.kind = 1;
        tween.fromColor = parameter.cast<ofFloatColor>().get();
        std::istringstream in(value);
        in >> tween.toColor;
    } else if (type == typeid(ofParameter<glm::vec2>).name()) {
        tween.kind = 2;
        tween.fromPoint = parameter.cast<glm::vec2>().get();
        std::istringstream in(value);
        in >> tween.toPoint;
    } else {
        // Ints, bools, strings: no in-between, so they change now
        parameter.fromString(value);
        return;
    }

    if (!active) {
        set(tween, 1);
        return;
    }
    tweens.push_back(tween);
}

void Fader::to(ofParameter<float> &parameter, float value) {
    to(static_cast<ofAbstractParameter &>(parameter), ofToString(value));
}

void Fader::apply(const ofJson &json, ofAbstractParameter &parameter) {
    applyTo(json, parameter, nullptr);
}

void Fader::applyTo(const ofJson &json, ofAbstractParameter &parameter, shared_ptr<ofAbstractParameter> keepAlive) {
    if (!parameter.isSerializable()) return;
    string name = parameter.getEscapedName();
    if (json.find(name) == json.end()) return;

    if (parameter.type() == typeid(ofParameterGroup).name()) {
        ofParameterGroup &group = static_cast<ofParameterGroup &>(parameter);
        for (auto &child : group) {
            applyTo(json[name], *child, child);
        }
        return;
    }
    const ofJson &value = json[name];
    to(parameter, value.is_string() ? value.get<string>() : value.dump(), keepAlive);
}

void Fader::atEnd(std::function<void()> action) {
    if (active) {
        endActions.push_back(action);
    } else {
        action();
    }
}

// Ease in and out
static float ease(float t) {
    return t * t * (3 - 2 * t);
}

void Fader::set(Tween &tween, float t) {
    switch (tween.kind) {
        case 0:
            tween.parameter->cast<float>() = ofLerp(tween.fromFloat, tween.toFloat, t);
            break;
        case 1:
            tween.parameter->cast<ofFloatColor>() = tween.fromColor.getLerped(tween.toColor, t);
            break;
        case 2:
            tween.parameter->cast<glm::vec2>() = glm::mix(tween.fromPoint, tween.toPoint, t);
            break;
    }
}

void Fader::update() {
    if (!active) return;
    float t = ofClamp((ofGetElapsedTimef() - startTime) / seconds, 0, 1);
    for (Tween &tween : tweens) {
        set(tween, ease(t));
    }
    if (t >= 1) finish();
}

void Fader::finish() {
    for (Tween &tween : tweens) {
        set(tween, 1);
    }
    tweens.clear();
    active = false;
    vector<std::function<void()>> actions;
    actions.swap(endActions);
    for (auto &action : actions) action();
}
