//
//  SynScene.cpp
//  dots
//

#include "SynScene.h"
#include "SynAudio.h"
#include "SynLibrary.h"
#include "SynScript.h"
#include <regex>

static const char *vertexShader = R"(#version 330
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
void main() {
    gl_Position = modelViewProjectionMatrix * position;
}
)";

static const char *standardUniforms = R"(#version 330
out vec4 dots_FragColor;
uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float FRAMECOUNT;
uniform int PASSINDEX;
uniform vec4 _mouse;
uniform vec3 _click;
uniform vec2 _muv;
uniform vec2 _muvc;
uniform sampler2D syn_FinalPass;
uniform sampler2D syn_UserImage;
uniform float syn_MediaType;
)";

static const char *mainFunction = R"(
void main() {
    dots_FragColor = renderMain();
}
)";

SynScene::SynScene(const SynAudio *audio) : audio(audio) {}
SynScene::~SynScene() {}

string SynScene::getScriptError() const {
    return script ? script->getError() : "";
}

bool SynScene::isScene(const string &folder) {
    return ofFile::doesFileExist(ofFilePath::join(folder, "main.glsl"), false)
        && ofFile::doesFileExist(ofFilePath::join(folder, "scene.json"), false);
}

static float jsonFloat(const ofJson &j, const string &key, float fallback) {
    if (!j.contains(key)) return fallback;
    const ofJson &v = j[key];
    if (v.is_number()) return v.get<float>();
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    if (v.is_array() && !v.empty() && v[0].is_number()) return v[0].get<float>();
    return fallback;
}

// A number or an array, as up to three values (a single number fills them all)
static void jsonValues(const ofJson &j, const string &key, float *out, int n) {
    if (!j.contains(key)) return;
    const ofJson &v = j[key];
    for (int i = 0; i < n; ++i) {
        if (v.is_number()) out[i] = v.get<float>();
        else if (v.is_array() && i < (int)v.size() && v[i].is_number()) out[i] = v[i].get<float>();
    }
}

// The compiler's log, empty if it compiles; ofShader would print the whole source
static string compileLog(GLenum type, const string &source) {
    GLuint id = glCreateShader(type);
    const char *text = source.c_str();
    glShaderSource(id, 1, &text, nullptr);
    glCompileShader(id);
    GLint ok = 0, length = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    string log;
    if (!ok) {
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        log.assign(std::max(length, 1), '\0');
        glGetShaderInfoLog(id, length, nullptr, &log[0]);
        log = ofTrim(log.c_str());
        if (log.empty()) log = "doesn't compile";
    }
    glDeleteShader(id);
    return log;
}

void SynScene::addControl(const ofJson &spec, std::map<string, ofParameterGroup> &groups, vector<string> &groupOrder) {
    auto control = std::make_unique<Control>();
    Control &c = *control;
    c.uniform = spec.value("NAME", "");
    if (c.uniform.empty()) return;
    vector<string> words = ofSplitString(spec.value("TYPE", "slider"), " ", true, true);
    c.type = words.empty() ? "slider" : words[0];
    for (std::size_t i = 1; i < words.size(); ++i) {
        c.smooth |= words[i] == "smooth" || words[i] == "traveler";
        c.speed |= words[i] == "speed" || words[i] == "traveler";
        c.counter |= words[i] == "counter";
    }
    c.rate = ofClamp(jsonFloat(spec, "PARAMS", 0.01f), 0.0001f, 1);
    jsonValues(spec, "MIN", c.minValue, 3);
    jsonValues(spec, "MAX", c.maxValue, 3);

    string label = c.uniform;
    std::replace(label.begin(), label.end(), '_', ' ');
    label = ofTrim(label);
    if (label.empty()) label = c.uniform;

    ofAbstractParameter *parameter = nullptr;
    if (c.type == "toggle") {
        float def = jsonFloat(spec, "DEFAULT", c.minValue[0]);
        parameter = &c.boolValue.set(label, def > (c.minValue[0] + c.maxValue[0]) / 2);
    } else if (c.type == "bang") {
        parameter = &c.bang.set(label);
        Control *raw = &c;
        c.bangListener = c.bang.newListener([raw]() { raw->banged = true; });
    } else if (c.type == "xy") {
        float def[3] = {0, 0, 0};
        jsonValues(spec, "DEFAULT", def, 2);
        parameter = &c.pointValue.set(label, glm::vec2(def[0], def[1]),
                                      glm::vec2(c.minValue[0], c.minValue[1]), glm::vec2(c.maxValue[0], c.maxValue[1]));
    } else if (c.type == "color") {
        float def[3] = {1, 1, 1};
        jsonValues(spec, "DEF_COLOR", def, 3);
        parameter = &c.colorValue.set(label, ofFloatColor(def[0], def[1], def[2]), ofFloatColor(0, 0, 0, 1), ofFloatColor(1, 1, 1, 1));
    } else if (c.type == "dropdown") {
        int n = 0;
        if (spec.contains("VALUES") && spec["VALUES"].is_array()) {
            for (auto &v : spec["VALUES"]) c.dropdownValues.push_back(v.is_number() ? v.get<float>() : 0);
        }
        if (spec.contains("LABELS") && spec["LABELS"].is_array()) n = spec["LABELS"].size();
        while ((int)c.dropdownValues.size() < n) c.dropdownValues.push_back(c.dropdownValues.size());
        if (c.dropdownValues.empty()) c.dropdownValues.push_back(0);
        int last = c.dropdownValues.size() - 1;
        parameter = &c.intValue.set(label, ofClamp((int)jsonFloat(spec, "DEFAULT", 0), 0, last), 0, last);
    } else {
        // slider and knob, and anything unknown
        c.type = "slider";
        float lo = c.minValue[0], hi = c.maxValue[0];
        parameter = &c.floatValue.set(label, ofClamp(jsonFloat(spec, "DEFAULT", lo), std::min(lo, hi), std::max(lo, hi)), lo, hi);
    }

    c.output = c.speed ? glm::vec3(0) : target(c);
    c.defaults = c.type == "dropdown" ? glm::vec3(c.intValue.get()) : target(c);

    string group = spec.value("UI_GROUP", "");
    c.group = group;
    if (group.empty() || group == "ungrouped") {
        parameters.add(*parameter);
    } else {
        if (!groups.count(group)) {
            groups[group].setName(group);
            groupOrder.push_back(group);
        }
        groups[group].add(*parameter);
    }
    controls.push_back(std::move(control));
}

glm::vec3 SynScene::target(Control &c) {
    if (c.type == "toggle") return glm::vec3(c.boolValue ? c.maxValue[0] : c.minValue[0]);
    if (c.type == "xy") return glm::vec3(c.pointValue.get(), 0);
    if (c.type == "color") {
        ofFloatColor col = c.colorValue;
        return glm::vec3(col.r, col.g, col.b);
    }
    if (c.type == "dropdown") return glm::vec3(c.dropdownValues[ofClamp(c.intValue.get(), 0, (int)c.dropdownValues.size() - 1)]);
    if (c.type == "bang") return glm::vec3(c.minValue[0]);
    return glm::vec3(c.floatValue.get());
}

void SynScene::updateControls(float dt) {
    for (auto &control : controls) {
        Control &c = *control;
        // A smooth control moves PARAMS of the way each frame at 60 fps
        float share = 1 - std::pow(1 - c.rate, dt * 60);
        if (c.type == "bang") {
            float lo = c.minValue[0], hi = c.maxValue[0];
            if (c.counter) {
                if (c.banged) c.count += 1;
                c.output.x = c.smooth ? c.output.x + (c.count - c.output.x) * share : c.count;
            } else if (c.smooth) {
                // A brief envelope: up quickly, then down at PARAMS' pace
                if (c.banged) c.rising = true;
                if (c.rising) {
                    c.envelope += (1 - c.envelope) * (1 - std::pow(0.5f, dt * 60));
                    if (c.envelope > 0.98f) c.rising = false;
                } else {
                    c.envelope *= std::pow(1 - c.rate, dt * 60);
                }
                c.output.x = lo + (hi - lo) * c.envelope;
            } else {
                c.output.x = c.banged ? hi : lo;
            }
            c.banged = false;
            continue;
        }

        glm::vec3 goal = target(c);
        if (c.speed) {
            // The control sets a speed, in units a second
            c.velocity = c.smooth ? c.velocity + (goal - c.velocity) * share : goal;
            c.output += c.velocity * dt;
        } else if (c.smooth) {
            c.output += (goal - c.output) * share;
        } else {
            c.output = goal;
        }
    }
}

// The script's inputs: the standard uniforms, the audio, the controls and the transitions
void SynScene::feedScript(float time, int frameIndex, float width, float height, const vector<float> &audioValues) {
    auto feed = [this](const string &input, std::initializer_list<float> values) {
        SynScript::Value value;
        value.size = values.size();
        std::copy(values.begin(), values.end(), value.v);
        script->setInput(input, value);
    };
    feed("TIME", {time});
    feed("FRAMECOUNT", {(float)frameIndex});
    feed("RENDERSIZE", {width, height});
    feed("_mouse", {0, 0, 0, 0});
    feed("_click", {0, 0, 0});
    feed("_muv", {0.5f, 0.5f});
    feed("_muvc", {0, 0});
    feed("syn_MediaType", {0});
    const vector<string> &audioNames = SynAudio::names();
    for (std::size_t i = 0; i < audioNames.size(); ++i) feed(audioNames[i], {i < audioValues.size() ? audioValues[i] : 0});
    for (auto &control : controls) {
        const Control &c = *control;
        if (c.type == "xy") feed(c.uniform, {c.output.x, c.output.y});
        else if (c.type == "color") feed(c.uniform, {c.output.x, c.output.y, c.output.z});
        else feed(c.uniform, {c.output.x});
    }
    for (auto &entry : transitionValues) feed(entry.first, {entry.second});
}

void SynScene::changeControls(const string &normalizedName, bool group, int change, const float *values, int count, int dimension) {
    for (auto &control : controls) {
        const string &controlName = group ? control->group : control->uniform;
        if (SynScript::normalize(controlName) == normalizedName) changeControl(*control, change, values, count, dimension);
    }
}

void SynScene::changeControl(Control &c, int change, const float *values, int count, int dimension) {
    if (c.type == "dropdown") {
        int last = c.dropdownValues.size() - 1;
        if (change == CONTROL_DEFAULT) c.intValue = (int)c.defaults.x;
        else if (change == CONTROL_RANDOMIZE) c.intValue = (int)ofRandom(last + 1) % (last + 1);
        else if (count > 0 && change == CONTROL_SET_NORMALIZED) c.intValue = ofClamp((int)std::round(values[0] * last), 0, last);
        else if (count > 0) {
            // The option whose value is nearest
            int best = 0;
            for (int i = 1; i <= last; ++i) {
                if (std::abs(c.dropdownValues[i] - values[0]) < std::abs(c.dropdownValues[best] - values[0])) best = i;
            }
            c.intValue = best;
        }
        return;
    }

    int dimensions = c.type == "xy" ? 2 : c.type == "color" ? 3 : 1;
    glm::vec3 next = target(c);
    for (int d = 0; d < dimensions; ++d) {
        if (dimension >= 0 && d != dimension) continue;
        int from = dimension >= 0 ? 0 : d;   // with a dimension given, the one value is for it
        float lo = c.type == "color" ? 0 : c.minValue[d], hi = c.type == "color" ? 1 : c.maxValue[d];
        if (change == CONTROL_SET && from < count) next[d] = values[from];
        else if (change == CONTROL_SET_NORMALIZED && from < count) next[d] = lo + values[from] * (hi - lo);
        else if (change == CONTROL_RANDOMIZE) next[d] = ofRandom(lo, hi);
        else if (change == CONTROL_DEFAULT) next[d] = c.defaults[d];
    }
    float lo = c.minValue[0], hi = c.maxValue[0];
    if (c.type == "toggle") c.boolValue = next.x > (lo + hi) / 2;
    else if (c.type == "bang") c.banged |= (change == CONTROL_SET || change == CONTROL_SET_NORMALIZED) && next.x > (lo + hi) / 2;
    else if (c.type == "xy") c.pointValue = glm::vec2(next.x, next.y);
    else if (c.type == "color") c.colorValue = ofFloatColor(next.x, next.y, next.z);
    else c.floatValue = ofClamp(next.x, std::min(lo, hi), std::max(lo, hi));
}

glm::vec3 SynScene::finalPassPixel(float x, float y) {
    if (passes.empty()) return glm::vec3(0);
    ofFbo &picture = passes.back().buffers[passes.back().front];
    if (!picture.isAllocated()) return glm::vec3(0);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, picture.getId());
    float rgba[4] = {0, 0, 0, 0};
    glReadPixels(ofClamp(x, 0, 1) * (picture.getWidth() - 1), ofClamp(y, 0, 1) * (picture.getHeight() - 1), 1, 1, GL_RGBA, GL_FLOAT, rgba);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, previous);
    return glm::vec3(rgba[0], rgba[1], rgba[2]);
}

void SynScene::allocatePass(Pass &pass, int width, int height) {
    ofFboSettings settings;
    settings.width = std::max(1, width);
    settings.height = std::max(1, height);
    settings.internalformat = pass.isFloat ? GL_RGBA32F : GL_RGBA;
    settings.textureTarget = GL_TEXTURE_2D;
    settings.minFilter = GL_LINEAR;
    settings.maxFilter = GL_LINEAR;
    settings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
    settings.wrapModeVertical = GL_CLAMP_TO_EDGE;
    for (ofFbo &buffer : pass.buffers) {
        buffer.allocate(settings);
        buffer.begin();
        ofClear(0, 0, 0, 0);
        buffer.end();
    }
    pass.front = 0;
}

bool SynScene::load(const string &folder) {
    path = folder;
    name = ofFilePath::getBaseName(folder);
    error.clear();
    controls.clear();
    passes.clear();
    images.clear();
    vectorUniforms.clear();
    transitionValues.clear();
    script.reset();
    parameters.clear();
    parameters.setName("Shader");
    quadSize = glm::vec2(0);

    ofJson json = ofLoadJson(ofFilePath::join(folder, "scene.json"));
    if (json.is_null() || !json.is_object()) {
        error = "no readable scene.json";
        return false;
    }
    string source = ofBufferFromFile(ofFilePath::join(folder, "main.glsl")).getText();
    if (source.empty()) {
        error = "no main.glsl";
        return false;
    }
    if (json.contains("TITLE") && json["TITLE"].is_string() && !json["TITLE"].get<string>().empty()) {
        name = json["TITLE"].get<string>();
    }

    // Whatever the scene declares itself, dots leaves out
    auto declares = [&source](const string &uniform) {
        return std::regex_search(source, std::regex("uniform\\s+\\w+\\s+" + uniform + "\\b"));
    };
    string uniforms;
    std::set<string> declared;
    auto declare = [&](const string &type, const string &uniform) {
        if (declares(uniform) || !declared.insert(uniform).second) return;
        uniforms += "uniform " + type + " " + uniform + ";\n";
    };
    // The audio uniforms a scene reads, unless it has a variable of that name
    for (const string &uniform : SynAudio::names()) {
        if (std::regex_search(source, std::regex("\\b" + uniform + "\\b"))
            && !std::regex_search(source, std::regex("\\b(float|int|vec[234])\\s+" + uniform + "\\b"))) {
            declare("float", uniform);
        }
    }

    // Controls, in their UI groups
    std::map<string, ofParameterGroup> groups;
    vector<string> groupOrder;
    for (auto &spec : json.value("CONTROLS", ofJson::array())) {
        if (spec.is_object()) addControl(spec, groups, groupOrder);
    }
    for (const string &group : groupOrder) parameters.add(groups[group]);
    for (auto &c : controls) {
        declare(c->type == "xy" ? "vec2" : c->type == "color" ? "vec3" : "float", c->uniform);
    }

    // Passes, then the picture itself
    for (auto &spec : json.value("PASSES", ofJson::array())) {
        Pass pass;
        pass.target = spec.value("TARGET", "");
        pass.width = jsonFloat(spec, "WIDTH", 0);
        pass.height = jsonFloat(spec, "HEIGHT", 0);
        pass.isFloat = jsonFloat(spec, "FLOAT", 1) > 0.5f || spec.value("TYPE", "") == "float";
        if (!pass.target.empty()) declare("sampler2D", pass.target);
        passes.push_back(std::move(pass));
    }
    passes.emplace_back();
    passes.back().isFloat = true;

    // Bundled images, the right way up (_uv has y = 0 at the bottom)
    for (auto &spec : json.value("IMAGES", ofJson::array())) {
        string imageName = spec.value("NAME", ""), imagePath = spec.value("PATH", "");
        if (imageName.empty()) continue;
        ofPixels pixels;
        ofTexture &texture = images[imageName];
        if (!imagePath.empty() && ofLoadImage(pixels, ofFilePath::join(folder, imagePath))) {
            pixels.mirror(true, false);
            texture.allocate(pixels, false);
            texture.loadData(pixels);
        } else {
            ofLogWarning("SynScene") << name << ": can't load image " << imagePath;
            ofPixels black;
            black.allocate(1, 1, OF_PIXELS_RGBA);
            black.setColor(ofColor(0, 0));
            texture.allocate(black, false);
            texture.loadData(black);
        }
        texture.setTextureWrap(GL_REPEAT, GL_REPEAT);
        texture.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
        declare("sampler2D", imageName);
    }

    // Song-change transitions: no song changes here yet, so each picks a value now
    for (auto &spec : json.value("HARD_TRANSITIONS", ofJson::array())) {
        string uniform = spec.value("UNIFORM", "");
        auto values = spec.value("VALUES", ofJson::array());
        if (uniform.empty()) continue;
        float value = 0;
        if (!values.empty()) {
            auto &v = values[(int)ofRandom(values.size()) % values.size()];
            if (v.is_number()) value = v.get<float>();
        }
        transitionValues[uniform] = value;
        declare("float", uniform);
    }
    for (auto &spec : json.value("SMOOTH_TRANSITIONS", ofJson::array())) {
        auto family = spec.value("UNIFORMS", ofJson::array());
        int on = family.empty() ? 0 : (int)ofRandom(family.size()) % family.size();
        for (int i = 0; i < (int)family.size(); ++i) {
            if (!family[i].is_string()) continue;
            transitionValues[family[i].get<string>()] = i == on ? 1 : 0;
            declare("float", family[i].get<string>());
        }
    }

    // script.js: run its top level, setup() and one update, to learn the uniforms it sets
    // and their sizes; then, for any it only sets later, go by its text and the GLSL
    string scriptSource = ofBufferFromFile(ofFilePath::join(folder, "script.js")).getText();
    if (!scriptSource.empty()) {
        script = std::make_unique<SynScript>(*this);
        vector<float> silence(SynAudio::names().size(), 0);
        feedScript(0, 0, 1920, 1080, silence);
        if (!script->load(scriptSource, name)) ofLogWarning("SynScene") << name << ": " << script->getError();
        script->update(1 / 60.0f);

        std::map<string, int> found;
        for (auto &entry : script->getUniforms()) found[entry.first] = entry.second.size;
        std::regex setUniform("setUniform\\s*\\(\\s*[\"'](\\w+)[\"']\\s*,([^;]*)");
        for (auto it = std::sregex_iterator(scriptSource.begin(), scriptSource.end(), setUniform); it != std::sregex_iterator(); ++it) {
            string args = (*it)[2];
            int size;
            size_t bracket = args.find_first_of("[{");
            if (bracket != string::npos) {
                size_t close = args.find_first_of("]}", bracket);
                string inside = args.substr(bracket + 1, close == string::npos ? string::npos : close - bracket - 1);
                size = ofSplitString(inside, ",", true, true).size();
            } else {
                size = ofSplitString(args, ",", true, true).size();
            }
            found[(*it)[1]] = std::max(found[(*it)[1]], (int)ofClamp(size, 1, 4));
        }
        std::regex assigned("uniforms\\s*\\.\\s*(\\w+)\\s*=");
        for (auto it = std::sregex_iterator(scriptSource.begin(), scriptSource.end(), assigned); it != std::sregex_iterator(); ++it) {
            if (!found.count((*it)[1])) found[(*it)[1]] = 1;
        }
        // A script often sets a whole object, so its size shows in how the GLSL uses it:
        // the furthest component it reads, or the vector it's assigned to
        for (auto &entry : found) {
            const string &uniform = entry.first;
            std::regex swizzle("\\b" + uniform + "\\s*\\.\\s*([xyzwrgba]+)\\b");
            for (auto it = std::sregex_iterator(source.begin(), source.end(), swizzle); it != std::sregex_iterator(); ++it) {
                for (char component : string((*it)[1])) {
                    int size = string("xrygzbwa").find(component) / 2 + 1;
                    entry.second = std::max(entry.second, size);
                }
            }
            std::regex assignedTo("vec([234])\\s+\\w+\\s*=\\s*" + uniform + "\\s*;");
            std::smatch vec;
            if (std::regex_search(source, vec, assignedTo)) entry.second = std::max(entry.second, std::stoi(vec[1]));
        }
        static const char *types[] = {"float", "vec2", "vec3", "vec4"};
        for (auto &entry : found) {
            // A name the GLSL has as a global of its own isn't the script's to set
            std::regex global("^(float|int|bool|vec[234]|mat[234])\\s+" + entry.first + "\\b", std::regex::multiline);
            if (std::regex_search(source, global)) continue;
            vectorUniforms.push_back(entry);
            declare(types[entry.second - 1], entry.first);
        }
    }

    // #version must come first, so any the scene has is dropped
    vector<string> lines = ofSplitString(source, "\n");
    for (string &line : lines) {
        if (ofTrim(line).rfind("#version", 0) == 0) line = "";
    }
    // Synesthesia also takes ISF's image macros, for imported ISF shaders
    string isfMacros;
    if (source.find("#define IMG_") == string::npos) {
        isfMacros = "#define IMG_NORM_PIXEL(image, coord) texture(image, coord)\n"
                    "#define IMG_THIS_NORM_PIXEL(image) texture(image, _uv)\n"
                    "#define IMG_PIXEL(image, coord) texture(image, (coord) / vec2(textureSize(image, 0)))\n"
                    "#define IMG_THIS_PIXEL(image) texture(image, _uv)\n"
                    "#define IMG_SIZE(image) vec2(textureSize(image, 0))\n";
    }
    string fragment = string(standardUniforms) + uniforms + synLibrary + isfMacros + "#line 1\n"
        + ofJoinString(lines, "\n") + mainFunction;

    string log = compileLog(GL_FRAGMENT_SHADER, fragment);
    // A script can make up uniform names as it goes (uniforms['spectrum_' + i]), so
    // with a script, anything left undeclared is taken to be one of its floats
    std::regex undeclared("undeclared identifier '(\\w+)'");
    std::smatch missing;
    for (int tries = 0; tries < 16 && script && std::regex_search(log, missing, undeclared); ++tries) {
        string uniform = missing[1];
        vectorUniforms.push_back({uniform, 1});
        fragment.insert(fragment.find(synLibrary), "uniform float " + uniform + ";\n");
        log = compileLog(GL_FRAGMENT_SHADER, fragment);
    }
    if (!log.empty()) {
        // The first error is enough to go on
        vector<string> logLines = ofSplitString(log, "\n", true, true);
        error = logLines.empty() ? log : logLines[0];
        ofStringReplace(error, "ERROR: 0:", "line ");
        return false;
    }
    shader.unload();
    shader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragment);
    shader.bindDefaults();
    if (!shader.linkProgram()) {
        error = "doesn't link";
        return false;
    }

    if (!blank.isAllocated()) {
        ofPixels black;
        black.allocate(1, 1, OF_PIXELS_RGBA);
        black.setColor(ofColor(0, 255));
        blank.allocate(black, false);
        blank.loadData(black);
    }
    return true;
}

void SynScene::render(ofFbo &target, float time, float timeDelta, int frameIndex) {
    if (!shader.isLoaded()) return;
    float w = target.getWidth(), h = target.getHeight();

    // Passes are the size the scene gives them (simulations count in pixels),
    // or the picture's size when it gives none
    if (quadSize != glm::vec2(w, h)) {
        quadSize = glm::vec2(w, h);
        for (Pass &pass : passes) {
            allocatePass(pass, pass.width > 0 ? pass.width : w, pass.height > 0 ? pass.height : h);
        }
    }

    updateControls(timeDelta);
    vector<float> audioValues = audio ? audio->values() : vector<float>();
    const vector<string> &audioNames = SynAudio::names();
    if (script) {
        feedScript(time, frameIndex, w, h, audioValues);
        script->update(timeDelta);
    }

    ofPushStyle();
    ofDisableAlphaBlending();
    ofSetColor(255);
    for (int p = 0; p < (int)passes.size(); ++p) {
        Pass &pass = passes[p];
        ofFbo &out = pass.buffers[1 - pass.front];
        out.begin();
        shader.begin();
        shader.setUniform2f("RENDERSIZE", out.getWidth(), out.getHeight());
        shader.setUniform1f("TIME", time);
        shader.setUniform1f("FRAMECOUNT", frameIndex);
        shader.setUniform1i("PASSINDEX", p);
        shader.setUniform4f("_mouse", 0, 0, 0, 0);
        shader.setUniform3f("_click", 0, 0, 0);
        shader.setUniform2f("_muv", 0.5, 0.5);
        shader.setUniform2f("_muvc", 0, 0);
        shader.setUniform1f("syn_MediaType", 0);
        for (std::size_t i = 0; i < audioValues.size() && i < audioNames.size(); ++i) {
            shader.setUniform1f(audioNames[i], audioValues[i]);
        }
        for (auto &control : controls) {
            const Control &c = *control;
            if (c.type == "xy") shader.setUniform2f(c.uniform, c.output.x, c.output.y);
            else if (c.type == "color") shader.setUniform3f(c.uniform, c.output.x, c.output.y, c.output.z);
            else shader.setUniform1f(c.uniform, c.output.x);
        }
        // What the script set, at the size the shader declares (0 until it sets it)
        for (auto &entry : vectorUniforms) {
            float v[4] = {0, 0, 0, 0};
            if (script) {
                auto set = script->getUniforms().find(entry.first);
                if (set != script->getUniforms().end()) std::copy(set->second.v, set->second.v + 4, v);
            }
            if (entry.second == 1) shader.setUniform1f(entry.first, v[0]);
            else if (entry.second == 2) shader.setUniform2f(entry.first, v[0], v[1]);
            else if (entry.second == 3) shader.setUniform3f(entry.first, v[0], v[1], v[2]);
            else shader.setUniform4f(entry.first, v[0], v[1], v[2], v[3]);
        }
        for (auto &entry : transitionValues) shader.setUniform1f(entry.first, entry.second);

        // Every pass's last finished frame; this pass's own is the one it isn't drawing into
        int unit = 1;
        for (Pass &other : passes) {
            if (!other.target.empty()) shader.setUniformTexture(other.target, other.buffers[other.front].getTexture(), unit++);
        }
        Pass &picture = passes.back();
        shader.setUniformTexture("syn_FinalPass", picture.buffers[picture.front].getTexture(), unit++);
        shader.setUniformTexture("syn_UserImage", blank, unit++);
        for (auto &image : images) shader.setUniformTexture(image.first, image.second, unit++);

        ofDrawRectangle(0, 0, out.getWidth(), out.getHeight());
        shader.end();
        out.end();
        pass.front = 1 - pass.front;
    }
    ofPopStyle();

    // The passes are GL-style, y up; openFrameworks shows a framebuffer's first row at the top.
    // Synesthesia shows the colour whatever the alpha, so only the colour is copied.
    Pass &picture = passes.back();
    target.begin();
    ofClear(0, 255);
    ofPushStyle();
    ofDisableAlphaBlending();
    ofSetColor(255);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    picture.buffers[picture.front].draw(0, h, w, -h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    ofPopStyle();
    target.end();
}
