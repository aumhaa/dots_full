//
//  ISFShader.cpp
//  dots
//

#include "ISFShader.h"

static const char *vertexShader = R"(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 isf_FragNormCoord;
void main() {
    isf_FragNormCoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

// Old-style ISF GLSL, as core profile GLSL 1.50
static const char *fragmentPreamble = R"(#version 150
#define gl_FragColor isf_FragColor
#define texture2D texture
#define varying in
#define vv_FragNormCoord isf_FragNormCoord
#define IMG_NORM_PIXEL(image, coord) texture(image, coord)
#define IMG_THIS_NORM_PIXEL(image) texture(image, isf_FragNormCoord)
#define IMG_PIXEL(image, coord) texture(image, (coord) / vec2(textureSize(image, 0)))
#define IMG_THIS_PIXEL(image) texture(image, isf_FragNormCoord)
#define IMG_SIZE(image) vec2(textureSize(image, 0))
out vec4 isf_FragColor;
in vec2 isf_FragNormCoord;
uniform float TIME;
uniform float TIMEDELTA;
uniform vec2 RENDERSIZE;
uniform int FRAMEINDEX;
uniform int PASSINDEX;
uniform vec4 DATE;
)";

// The JSON header between the first /* and */, and where the GLSL starts after it
static bool splitISF(const string &source, ofJson &header, string &body) {
    size_t start = source.find("/*");
    size_t end = source.find("*/", start == string::npos ? 0 : start + 2);
    if (start == string::npos || end == string::npos) return false;
    header = ofJson::parse(source.substr(start + 2, end - start - 2), nullptr, false);
    if (header.is_discarded()) return false;
    body = source.substr(end + 2);
    return true;
}

ISFShader::Kind ISFShader::classify(const string &path) {
    ofJson header;
    string body;
    if (!splitISF(ofBufferFromFile(path).getText(), header, body)) return UNUSABLE;

    auto passes = header.value("PASSES", ofJson::array());
    if (passes.size() > 1) return UNUSABLE;
    for (auto &pass : passes) {
        if (pass.contains("TARGET") || pass.value("PERSISTENT", false)) return UNUSABLE;
    }

    bool start = false, end = false, image = false, audio = false;
    for (auto &input : header.value("INPUTS", ofJson::array())) {
        string name = input.value("NAME", "");
        if (name == "inputImage") return FILTER;
        start |= name == "startImage";
        end |= name == "endImage";
        image |= input.value("TYPE", "") == "image";
        audio |= input.value("TYPE", "") == "audio" || input.value("TYPE", "") == "audioFFT";
    }
    for (auto &category : header.value("CATEGORIES", ofJson::array())) {
        if (category.is_string() && category.get<string>() == "Transition") return TRANSITION;
    }
    if (start && end) return TRANSITION;
    if (image) return NEEDS_IMAGE;

    // Animated: it reads the clock or the frame count, or follows the audio
    bool animated = audio || body.find("TIME") != string::npos || body.find("FRAMEINDEX") != string::npos;
    return animated ? ANIMATED_GENERATOR : STATIC_GENERATOR;
}

static float jsonFloat(const ofJson &j, const string &key, float fallback) {
    if (!j.contains(key)) return fallback;
    const ofJson &v = j[key];
    if (v.is_number()) return v.get<float>();
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    return fallback;
}

static glm::vec2 jsonPoint(const ofJson &j, const string &key, glm::vec2 fallback) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 2) return fallback;
    return glm::vec2(j[key][0].get<float>(), j[key][1].get<float>());
}

bool ISFShader::load(const string &filePath) {
    path = filePath;
    name = ofFilePath::getBaseName(path);
    error.clear();
    inputs.clear();
    parameters.clear();
    parameters.setName("Shader");

    ofJson header;
    string body;
    if (!splitISF(ofBufferFromFile(path).getText(), header, body)) {
        error = "no readable ISF header";
        return false;
    }

    // Inputs: one parameter and one uniform each
    string uniforms;
    for (auto &spec : header.value("INPUTS", ofJson::array())) {
        Input input;
        input.name = spec.value("NAME", "");
        input.type = spec.value("TYPE", "");
        if (input.name.empty()) continue;
        string label = spec.value("LABEL", input.name);

        if (input.type == "float") {
            float lo = jsonFloat(spec, "MIN", 0), hi = jsonFloat(spec, "MAX", 1);
            parameters.add(input.floatValue.set(label, jsonFloat(spec, "DEFAULT", lo), lo, hi));
            uniforms += "uniform float " + input.name + ";\n";
        } else if (input.type == "bool" || input.type == "event") {
            parameters.add(input.boolValue.set(label, jsonFloat(spec, "DEFAULT", 0) > 0.5));
            uniforms += "uniform bool " + input.name + ";\n";
        } else if (input.type == "long") {
            auto values = spec.value("VALUES", ofJson::array());
            if (values.empty()) {
                for (int v = (int)jsonFloat(spec, "MIN", 0); v <= (int)jsonFloat(spec, "MAX", 1); ++v) input.values.push_back(v);
            } else {
                for (auto &v : values) input.values.push_back(v.get<int>());
            }
            int def = (int)jsonFloat(spec, "DEFAULT", input.values.empty() ? 0 : input.values[0]);
            int index = std::max(0, (int)(std::find(input.values.begin(), input.values.end(), def) - input.values.begin()));
            parameters.add(input.intValue.set(label, std::min(index, (int)input.values.size() - 1), 0, std::max(0, (int)input.values.size() - 1)));
            uniforms += "uniform int " + input.name + ";\n";
        } else if (input.type == "color") {
            ofFloatColor def(1, 1, 1, 1);
            if (spec.contains("DEFAULT") && spec["DEFAULT"].is_array() && spec["DEFAULT"].size() >= 3) {
                auto &d = spec["DEFAULT"];
                def.set(d[0].get<float>(), d[1].get<float>(), d[2].get<float>(), d.size() > 3 ? d[3].get<float>() : 1);
            }
            parameters.add(input.colorValue.set(label, def, ofFloatColor(0, 0, 0, 0), ofFloatColor(1, 1, 1, 1)));
            uniforms += "uniform vec4 " + input.name + ";\n";
        } else if (input.type == "point2D") {
            glm::vec2 def = jsonPoint(spec, "DEFAULT", glm::vec2(0));
            // Points are in pixels unless the shader says otherwise
            bool normalized = def.x <= 1 && def.y <= 1;
            glm::vec2 hi = jsonPoint(spec, "MAX", normalized ? glm::vec2(1) : glm::vec2(1920, 1080));
            parameters.add(input.pointValue.set(label, def, jsonPoint(spec, "MIN", glm::vec2(0)), hi));
            uniforms += "uniform vec2 " + input.name + ";\n";
        } else if (input.type == "image" || input.type == "audio" || input.type == "audioFFT") {
            uniforms += "uniform sampler2D " + input.name + ";\n";
        } else {
            continue;
        }
        inputs.push_back(input);
    }

    // #version must be the first line, so any the shader has is dropped
    vector<string> lines = ofSplitString(body, "\n");
    for (string &line : lines) {
        if (ofTrim(line).rfind("#version", 0) == 0) line = "";
    }
    string fragment = string(fragmentPreamble) + uniforms + "#line 1\n" + ofJoinString(lines, "\n");

    shader.unload();
    shader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    if (!shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragment)) {
        error = "doesn't compile";
        return false;
    }
    shader.bindDefaults();
    if (!shader.linkProgram()) {
        error = "doesn't link";
        return false;
    }

    if (!blank.isAllocated()) {
        ofPixels black;
        black.allocate(1, 1, OF_PIXELS_RGBA);
        black.setColor(ofColor(0, 0, 0, 255));
        blank.allocate(1, 1, GL_RGBA, false);   // GL_TEXTURE_2D, as ISF's sampler2D expects
        blank.loadData(black);
    }
    return true;
}

void ISFShader::render(ofFbo &target, float time, float timeDelta, int frameIndex) {
    if (!shader.isLoaded()) return;
    float w = target.getWidth(), h = target.getHeight();

    if (!raw.isAllocated() || raw.getWidth() != w || raw.getHeight() != h) {
        raw.allocate(w, h, GL_RGBA);
        // isf_FragNormCoord runs (0,0)-(1,1) along with gl_FragCoord
        quad.clear();
        quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
        quad.addVertex(glm::vec3(0, 0, 0)); quad.addTexCoord(glm::vec2(0, 0));
        quad.addVertex(glm::vec3(w, 0, 0)); quad.addTexCoord(glm::vec2(1, 0));
        quad.addVertex(glm::vec3(w, h, 0)); quad.addTexCoord(glm::vec2(1, 1));
        quad.addVertex(glm::vec3(0, h, 0)); quad.addTexCoord(glm::vec2(0, 1));
    }

    raw.begin();
    ofClear(0, 255);
    ofPushStyle();
    ofDisableAlphaBlending();
    shader.begin();
    shader.setUniform1f("TIME", time);
    shader.setUniform1f("TIMEDELTA", timeDelta);
    shader.setUniform2f("RENDERSIZE", w, h);
    shader.setUniform1i("FRAMEINDEX", frameIndex);
    shader.setUniform1i("PASSINDEX", 0);
    shader.setUniform4f("DATE", ofGetYear(), ofGetMonth(), ofGetDay(),
                        ofGetHours() * 3600 + ofGetMinutes() * 60 + ofGetSeconds());
    int textureUnit = 1;
    for (Input &input : inputs) {
        if (input.type == "float") {
            shader.setUniform1f(input.name, input.floatValue);
        } else if (input.type == "bool" || input.type == "event") {
            shader.setUniform1i(input.name, input.boolValue ? 1 : 0);
        } else if (input.type == "long") {
            int index = ofClamp(input.intValue, 0, (int)input.values.size() - 1);
            shader.setUniform1i(input.name, input.values.empty() ? 0 : input.values[index]);
        } else if (input.type == "color") {
            ofFloatColor c = input.colorValue;
            shader.setUniform4f(input.name, c.r, c.g, c.b, c.a);
        } else if (input.type == "point2D") {
            glm::vec2 p = input.pointValue;
            shader.setUniform2f(input.name, p.x, p.y);
        } else {
            const ofTexture *texture = &blank;
            if (input.type == "audio" && waveform && waveform->isAllocated()) texture = waveform;
            if (input.type == "audioFFT" && spectrum && spectrum->isAllocated()) texture = spectrum;
            shader.setUniformTexture(input.name, *texture, textureUnit++);
        }
    }
    quad.draw();
    shader.end();
    ofPopStyle();
    raw.end();

    // Events last one frame
    for (Input &input : inputs) {
        if (input.type == "event" && input.boolValue) input.boolValue = false;
    }

    // The quad's (0,0) lands on the framebuffer's first row, which openFrameworks shows
    // at the top; ISF puts y = 0 at the bottom, so flip it the right way up
    target.begin();
    ofClear(0, 255);
    ofSetColor(255);
    raw.draw(0, h, w, -h);
    target.end();
}
