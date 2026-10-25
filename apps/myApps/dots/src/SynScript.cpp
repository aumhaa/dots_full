//
//  SynScript.cpp
//  dots
//

#include "SynScript.h"
#include "SynScene.h"

static const int maxPrints = 50;

static string jsString(JSContextRef context, JSValueRef value) {
    JSStringRef string = JSValueToStringCopy(context, value, nullptr);
    if (!string) return "";
    std::size_t size = JSStringGetMaximumUTF8CStringSize(string);
    std::string out(size, '\0');
    out.resize(JSStringGetUTF8CString(string, &out[0], size) - 1);
    JSStringRelease(string);
    return out;
}

static JSValueRef property(JSContextRef context, JSObjectRef object, const char *name) {
    JSStringRef key = JSStringCreateWithUTF8CString(name);
    JSValueRef value = JSObjectGetProperty(context, object, key, nullptr);
    JSStringRelease(key);
    return value;
}

static void setProperty(JSContextRef context, JSObjectRef object, const string &name, JSValueRef value) {
    JSStringRef key = JSStringCreateWithUTF8CString(name.c_str());
    JSObjectSetProperty(context, object, key, value, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

static void define(JSContextRef context, JSObjectRef object, const char *name, JSObjectCallAsFunctionCallback function) {
    JSStringRef key = JSStringCreateWithUTF8CString(name);
    JSObjectSetProperty(context, object, key, JSObjectMakeFunctionWithCallback(context, key, function),
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

string SynScript::normalize(const string &name) {
    string out;
    for (char c : name) {
        if (c == ' ' || c == '_' || c == '-') continue;
        out += std::tolower((unsigned char)c);
    }
    return out;
}

SynScript *SynScript::from(JSContextRef context) {
    return static_cast<SynScript *>(JSObjectGetPrivate(JSContextGetGlobalObject(context)));
}

SynScript::~SynScript() {
    if (context) JSGlobalContextRelease(context);
    if (globalClass) JSClassRelease(globalClass);
}

void SynScript::log(const string &text) {
    if (printed > maxPrints) return;
    if (++printed > maxPrints) {
        ofLogNotice("SynScript") << name << ": (more output left out)";
        return;
    }
    ofLogNotice("SynScript") << name << ": " << text;
}

void SynScript::report(JSValueRef exception, const string &where) {
    if (!exception) return;
    string message = where + ": " + jsString(context, exception);
    if (JSValueIsObject(context, exception)) {
        JSValueRef line = property(context, (JSObjectRef)exception, "line");
        if (JSValueIsNumber(context, line)) message += " (line " + ofToString((int)JSValueToNumber(context, line, nullptr)) + ")";
    }
    if (error.empty()) error = message;
    if (reported.insert(message).second) ofLogWarning("SynScript") << name << ": " << message;
}

bool SynScript::load(const string &source, const string &sceneName) {
    name = sceneName;

    // The global object carries this, so native functions can find their script
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    globalClass = JSClassCreate(&definition);
    context = JSGlobalContextCreate(globalClass);
    JSObjectRef global = JSContextGetGlobalObject(context);
    JSObjectSetPrivate(global, this);

    define(context, global, "setUniform", jsSetUniform);
    define(context, global, "setControl", jsSetControl);
    define(context, global, "setControlNormalized", jsSetControlNormalized);
    define(context, global, "randomizeControl", jsRandomizeControl);
    define(context, global, "defaultControl", jsDefaultControl);
    define(context, global, "randomizeGroup", jsRandomizeGroup);
    define(context, global, "defaultGroup", jsDefaultGroup);
    define(context, global, "onChange", jsOnChange);
    define(context, global, "onOffToOn", jsOnOffToOn);
    define(context, global, "onOnToOff", jsOnOnToOff);
    define(context, global, "whileOn", jsWhileOn);
    define(context, global, "whileOff", jsWhileOff);
    define(context, global, "print", jsPrint);
    define(context, global, "printGlobalKeys", jsPrintGlobalKeys);
    define(context, global, "textureFinalPass", jsTextureFinalPass);
    JSObjectRef console = JSObjectMake(context, nullptr, nullptr);
    define(context, console, "log", jsPrint);
    setProperty(context, global, "console", console);

    // The older way: read inputs.x, set uniforms.y
    inputsObject = JSObjectMake(context, nullptr, nullptr);
    uniformsObject = JSObjectMake(context, nullptr, nullptr);
    setProperty(context, global, "inputs", inputsObject);
    setProperty(context, global, "uniforms", uniformsObject);

    // The inputs as they stand, so the top level and setup() can read them
    update(-1);

    JSStringRef script = JSStringCreateWithUTF8CString(source.c_str());
    JSStringRef url = JSStringCreateWithUTF8CString("script.js");
    JSValueRef exception = nullptr;
    JSEvaluateScript(context, script, nullptr, url, 1, &exception);
    JSStringRelease(script);
    JSStringRelease(url);
    if (exception) {
        report(exception, "script.js");
        return false;
    }
    call("setup", {});
    readUniformsObject();
    return error.empty();
}

void SynScript::setInput(const string &inputName, const Value &value) {
    inputs[inputName] = value;
    inputNames[normalize(inputName)] = inputName;
}

float SynScript::inputValue(const string &normalizedName, bool &found) {
    auto named = inputNames.find(normalizedName);
    found = named != inputNames.end();
    return found ? inputs[named->second].v[0] : 0;
}

void SynScript::call(const string &function, std::initializer_list<double> arguments) {
    JSObjectRef global = JSContextGetGlobalObject(context);
    JSValueRef value = property(context, global, function.c_str());
    if (!JSValueIsObject(context, value) || !JSObjectIsFunction(context, (JSObjectRef)value)) return;
    vector<JSValueRef> args;
    for (double argument : arguments) args.push_back(JSValueMakeNumber(context, argument));
    JSValueRef exception = nullptr;
    JSObjectCallAsFunction(context, (JSObjectRef)value, nullptr, args.size(), args.data(), &exception);
    report(exception, function + "()");
}

void SynScript::update(float dt) {
    if (!context) return;

    // Inputs as globals and on `inputs`: numbers, or objects with x y z and r g b for vectors
    JSObjectRef global = JSContextGetGlobalObject(context);
    for (auto &entry : inputs) {
        const Value &value = entry.second;
        JSValueRef js;
        if (value.size == 1) {
            js = JSValueMakeNumber(context, value.v[0]);
        } else {
            JSObjectRef vector = JSObjectMake(context, nullptr, nullptr);
            static const char *xyzw[] = {"x", "y", "z", "w"}, *rgba[] = {"r", "g", "b", "a"};
            for (int i = 0; i < value.size; ++i) {
                JSValueRef component = JSValueMakeNumber(context, value.v[i]);
                setProperty(context, vector, xyzw[i], component);
                setProperty(context, vector, rgba[i], component);
                JSObjectSetPropertyAtIndex(context, vector, i, component, nullptr);
            }
            js = vector;
        }
        setProperty(context, global, entry.first, js);
        setProperty(context, inputsObject, entry.first, js);
    }
    if (dt < 0) return;

    // Event handlers, against last frame's values
    for (Handler &handler : handlers) {
        bool found;
        float value = inputValue(handler.target, found);
        if (!found) continue;
        float previous = handler.primed ? handler.previous : value;
        bool fire = false;
        switch (handler.kind) {
            case ON_CHANGE: fire = handler.primed && value != previous; break;
            case OFF_TO_ON: fire = previous <= 0.5f && value > 0.5f; break;
            case ON_TO_OFF: fire = previous > 0.5f && value <= 0.5f; break;
            case WHILE_ON: fire = value > 0.5f; break;
            case WHILE_OFF: fire = value <= 0.5f; break;
        }
        handler.previous = value;
        handler.primed = true;
        if (fire) call(handler.callback, {value, previous});
    }

    call("update", {dt});
    readUniformsObject();
}

bool SynScript::toValue(JSValueRef value, Value &out) {
    if (JSValueIsNumber(context, value) || JSValueIsBoolean(context, value)) {
        out.size = 1;
        out.v[0] = JSValueToNumber(context, value, nullptr);
        if (!std::isfinite(out.v[0])) out.v[0] = 0;
        return true;
    }
    if (!JSValueIsObject(context, value)) return false;
    JSObjectRef object = (JSObjectRef)value;
    if (JSValueIsArray(context, value)) {
        JSValueRef length = property(context, object, "length");
        int n = std::min(4, (int)JSValueToNumber(context, length, nullptr));
        if (n < 1) return false;
        for (int i = 0; i < n; ++i) out.v[i] = JSValueToNumber(context, JSObjectGetPropertyAtIndex(context, object, i, nullptr), nullptr);
        out.size = n;
    } else {
        // {x, y, z, w} or {r, g, b, a}: as many as are there, in order
        static const char *xyzw[] = {"x", "y", "z", "w"}, *rgba[] = {"r", "g", "b", "a"};
        const char **names = JSValueIsNumber(context, property(context, object, "x")) ? xyzw : rgba;
        int n = 0;
        while (n < 4) {
            JSValueRef component = property(context, object, names[n]);
            if (!JSValueIsNumber(context, component)) break;
            out.v[n++] = JSValueToNumber(context, component, nullptr);
        }
        if (n == 0) return false;
        out.size = n;
    }
    for (int i = 0; i < out.size; ++i) {
        if (!std::isfinite(out.v[i])) out.v[i] = 0;
    }
    return true;
}

void SynScript::readUniformsObject() {
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, uniformsObject);
    for (std::size_t i = 0; i < JSPropertyNameArrayGetCount(names); ++i) {
        JSStringRef key = JSPropertyNameArrayGetNameAtIndex(names, i);
        Value value;
        if (!toValue(JSObjectGetProperty(context, uniformsObject, key, nullptr), value)) continue;
        std::size_t size = JSStringGetMaximumUTF8CStringSize(key);
        string uniform(size, '\0');
        uniform.resize(JSStringGetUTF8CString(key, &uniform[0], size) - 1);
        uniforms[uniform] = value;
    }
    JSPropertyNameArrayRelease(names);
}

// setUniform(name, x, y, z, w), (name, [x, y, ...]) or (name, {x, y} / {r, g, b})
JSValueRef SynScript::jsSetUniform(JSContextRef context, JSObjectRef, JSObjectRef, size_t count, const JSValueRef args[], JSValueRef *) {
    SynScript *script = from(context);
    if (!script || count < 2) return JSValueMakeUndefined(context);
    string uniform = jsString(context, args[0]);
    Value value;
    if (count == 2) {
        if (!script->toValue(args[1], value)) return JSValueMakeUndefined(context);
    } else {
        value.size = std::min<size_t>(4, count - 1);
        for (int i = 0; i < value.size; ++i) {
            value.v[i] = JSValueToNumber(context, args[i + 1], nullptr);
            if (!std::isfinite(value.v[i])) value.v[i] = 0;
        }
    }
    script->uniforms[uniform] = value;
    return JSValueMakeUndefined(context);
}

// The control functions: (name, values, {dimension, bank}) for setting, (name) otherwise
JSValueRef SynScript::controlCall(int change, JSContextRef context, size_t count, const JSValueRef args[]) {
    SynScript *script = from(context);
    if (!script || count < 1) return JSValueMakeUndefined(context);
    string target = jsString(context, args[0]);
    bool meta = target.rfind("meta/", 0) == 0;
    int dimension = -1;
    if (count > 2 && JSValueIsObject(context, args[2])) {
        JSValueRef bank = property(context, (JSObjectRef)args[2], "bank");
        meta |= JSValueIsString(context, bank) && jsString(context, bank) == "meta";
        JSValueRef dim = property(context, (JSObjectRef)args[2], "dimension");
        if (JSValueIsNumber(context, dim)) dimension = JSValueToNumber(context, dim, nullptr);
    }
    if (meta) return JSValueMakeUndefined(context);   // no meta controls in dots

    Value value;
    if (count > 1) script->toValue(args[1], value);
    bool group = change >= 100;
    script->scene.changeControls(normalize(target), group, change % 100, value.v, value.size, dimension);
    return JSValueMakeUndefined(context);
}

JSValueRef SynScript::jsSetControl(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) {
    return controlCall(SynScene::CONTROL_SET, c, n, a);
}
JSValueRef SynScript::jsSetControlNormalized(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) {
    return controlCall(SynScene::CONTROL_SET_NORMALIZED, c, n, a);
}
JSValueRef SynScript::jsRandomizeControl(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) {
    return controlCall(SynScene::CONTROL_RANDOMIZE, c, n, a);
}
JSValueRef SynScript::jsDefaultControl(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) {
    return controlCall(SynScene::CONTROL_DEFAULT, c, n, a);
}
JSValueRef SynScript::jsRandomizeGroup(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) {
    return controlCall(100 + SynScene::CONTROL_RANDOMIZE, c, n, a);
}
JSValueRef SynScript::jsDefaultGroup(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) {
    return controlCall(100 + SynScene::CONTROL_DEFAULT, c, n, a);
}

// Event handlers: (target, "callbackName")
JSValueRef SynScript::addHandler(int kind, JSContextRef context, size_t count, const JSValueRef args[]) {
    SynScript *script = from(context);
    if (script && count >= 2) {
        Handler handler;
        handler.kind = kind;
        handler.target = normalize(jsString(context, args[0]));
        handler.callback = jsString(context, args[1]);
        script->handlers.push_back(handler);
    }
    return JSValueMakeUndefined(context);
}

JSValueRef SynScript::jsOnChange(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) { return addHandler(ON_CHANGE, c, n, a); }
JSValueRef SynScript::jsOnOffToOn(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) { return addHandler(OFF_TO_ON, c, n, a); }
JSValueRef SynScript::jsOnOnToOff(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) { return addHandler(ON_TO_OFF, c, n, a); }
JSValueRef SynScript::jsWhileOn(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) { return addHandler(WHILE_ON, c, n, a); }
JSValueRef SynScript::jsWhileOff(JSContextRef c, JSObjectRef, JSObjectRef, size_t n, const JSValueRef a[], JSValueRef *) { return addHandler(WHILE_OFF, c, n, a); }

JSValueRef SynScript::jsPrint(JSContextRef context, JSObjectRef, JSObjectRef, size_t count, const JSValueRef args[], JSValueRef *) {
    SynScript *script = from(context);
    if (!script) return JSValueMakeUndefined(context);
    string text;
    for (size_t i = 0; i < count; ++i) text += (i ? " " : "") + jsString(context, args[i]);
    script->log(text);
    return JSValueMakeUndefined(context);
}

JSValueRef SynScript::jsPrintGlobalKeys(JSContextRef context, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *) {
    SynScript *script = from(context);
    if (!script) return JSValueMakeUndefined(context);
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, JSContextGetGlobalObject(context));
    vector<string> keys;
    for (std::size_t i = 0; i < JSPropertyNameArrayGetCount(names); ++i) {
        keys.push_back(jsString(context, JSValueMakeString(context, JSPropertyNameArrayGetNameAtIndex(names, i))));
    }
    JSPropertyNameArrayRelease(names);
    script->log(ofJoinString(keys, ", "));
    return JSValueMakeUndefined(context);
}

// textureFinalPass(x, y): the last picture's colour there, as [r, g, b]
JSValueRef SynScript::jsTextureFinalPass(JSContextRef context, JSObjectRef, JSObjectRef, size_t count, const JSValueRef args[], JSValueRef *) {
    SynScript *script = from(context);
    glm::vec3 color(0);
    if (script && count >= 2) {
        color = script->scene.finalPassPixel(JSValueToNumber(context, args[0], nullptr), JSValueToNumber(context, args[1], nullptr));
    }
    JSValueRef rgb[] = {JSValueMakeNumber(context, color.r), JSValueMakeNumber(context, color.g), JSValueMakeNumber(context, color.b)};
    return JSObjectMakeArray(context, 3, rgb, nullptr);
}
