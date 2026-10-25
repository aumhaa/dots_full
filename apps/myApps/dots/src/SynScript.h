//
//  SynScript.h
//  dots
//
//  A Synesthesia scene's script.js, run with JavaScriptCore (part of macOS).
//  Each frame the scene's inputs (TIME, the syn_ audio uniforms, its controls)
//  are set as globals and as properties of `inputs`; event handlers fire, then
//  update(dt) runs; what the script sets with setUniform() or on `uniforms`
//  becomes uniforms for the shader. setControl() and its relatives change the
//  scene's controls. Synesthesia's meta controls ("meta/...") aren't here, so
//  setting them does nothing.
//

#pragma once

#include "ofMain.h"
#include <JavaScriptCore/JavaScriptCore.h>

class SynScene;

class SynScript {
public:
    explicit SynScript(SynScene &scene) : scene(scene) {}
    ~SynScript();

    // Runs the script's top level and its setup(); false, with getError(), if it can't
    bool load(const string &source, const string &sceneName);

    // Up to four floats, as a shader would see them
    struct Value {
        float v[4] = {0, 0, 0, 0};
        int size = 1;
    };
    // What the script sees as a global (and on `inputs`) from the next update on
    void setInput(const string &name, const Value &value);

    // Fires event handlers, then calls update(dt)
    void update(float dt);

    // Everything the script has set as a uniform so far
    const std::map<string, Value> &getUniforms() const { return uniforms; }
    const string &getError() const { return error; }

    // Lower case with spaces, _ and - dropped, as Synesthesia matches names
    static string normalize(const string &name);

private:
    static SynScript *from(JSContextRef context);
    void report(JSValueRef exception, const string &where);
    void log(const string &text);
    bool toValue(JSValueRef value, Value &out);
    void readUniformsObject();
    void call(const string &function, std::initializer_list<double> arguments);
    float inputValue(const string &normalizedName, bool &found);

    // Native functions the script can call
    static JSValueRef jsSetUniform(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsSetControl(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsSetControlNormalized(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsRandomizeControl(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsDefaultControl(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsRandomizeGroup(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsDefaultGroup(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsOnChange(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsOnOffToOn(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsOnOnToOff(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsWhileOn(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsWhileOff(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsPrint(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsPrintGlobalKeys(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef jsTextureFinalPass(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef controlCall(int kind, JSContextRef, size_t, const JSValueRef[]);
    static JSValueRef addHandler(int kind, JSContextRef, size_t, const JSValueRef[]);

    SynScene &scene;
    string name, error;
    JSClassRef globalClass = nullptr;
    JSGlobalContextRef context = nullptr;
    JSObjectRef inputsObject = nullptr, uniformsObject = nullptr;

    std::map<string, Value> inputs;          // by name, as set
    std::map<string, string> inputNames;     // normalized name -> name
    std::map<string, Value> uniforms;

    enum HandlerKind { ON_CHANGE, OFF_TO_ON, ON_TO_OFF, WHILE_ON, WHILE_OFF };
    struct Handler {
        int kind;
        string target, callback;            // target normalized
        float previous = 0;
        bool primed = false;
    };
    vector<Handler> handlers;

    int printed = 0;                         // print() is capped, so a script can't flood the log
    std::set<string> reported;               // each error once
};
