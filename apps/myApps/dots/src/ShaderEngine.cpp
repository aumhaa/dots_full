//
//  ShaderEngine.cpp
//  dots
//

#include "ShaderEngine.h"

void ShaderEngine::setup(int width, int height) {
    vector<string> folders = {
        "/Library/Graphics/ISF",
        ofFilePath::join(ofFilePath::getUserHomeDir(), "Library/Graphics/ISF"),
        ofToDataPath("shaders", true),
    };
    for (const string &folder : folders) {
        ofDirectory dir(folder);
        if (!dir.exists()) continue;
        dir.allowExt("fs");
        dir.listDir();
        for (auto &file : dir.getFiles()) {
            ISFShader::Kind kind = ISFShader::classify(file.getAbsolutePath());
            if (kind == ISFShader::ANIMATED_GENERATOR || kind == ISFShader::STATIC_GENERATOR) {
                allShaders.push_back({file.getAbsolutePath(), kind == ISFShader::ANIMATED_GENERATOR});
            }
        }
    }
    std::sort(allShaders.begin(), allShaders.end(), [](const Entry &a, const Entry &b) {
        return ofToLower(ofFilePath::getFileName(a.path)) < ofToLower(ofFilePath::getFileName(b.path));
    });

    resize(width, height);
    setAnimatedOnly(animatedOnly);
}

void ShaderEngine::setAnimatedOnly(bool only) {
    animatedOnly = only;
    string current = index >= 0 && index < (int)library.size() ? library[index] : "";
    library.clear();
    for (const Entry &entry : allShaders) {
        if (entry.animated || !animatedOnly) library.push_back(entry.path);
    }
    ofLogNotice("ShaderEngine") << library.size() << " ISF generators" << (animatedOnly ? " (animated only)" : "");

    // Stay on the current shader if it's still on offer
    auto found = std::find(library.begin(), library.end(), current);
    if (found != library.end()) {
        index = found - library.begin();
    } else if (!library.empty()) {
        load(0);
    }
}

void ShaderEngine::resize(int width, int height) {
    for (ofFbo *buffer : {&fbo, &outgoingFbo, &incomingFbo}) {
        buffer->allocate(width, height, GL_RGBA);
        buffer->begin();
        ofClear(0, 255);
        buffer->end();
    }
}

void ShaderEngine::render() {
    float now = ofGetElapsedTimef();
    float delta = lastFrameTime > 0 ? std::min(now - lastFrameTime, 0.25f) : 0;
    lastFrameTime = now;
    time += delta;

    if (outgoing) {
        // Mid-fade: both shaders run, the new one over the old
        float t = ofClamp((now - fadeStart) / fadeSeconds, 0, 1);
        outgoingTime += delta;
        outgoing->render(outgoingFbo, outgoingTime, delta, outgoingFrame++);
        if (shaderOk) shader->render(incomingFbo, time, delta, frameIndex++);
        fbo.begin();
        ofClear(0, 255);
        ofPushStyle();
        ofEnableAlphaBlending();
        ofSetColor(255);
        outgoingFbo.draw(0, 0);
        ofSetColor(255, 255 * t * t * (3 - 2 * t));
        incomingFbo.draw(0, 0);
        ofPopStyle();
        fbo.end();
        if (t >= 1) outgoing.reset();
        return;
    }

    if (!shaderOk) return;
    shader->render(fbo, time, delta, frameIndex++);
}

void ShaderEngine::load(int newIndex, float fade) {
    if (library.empty()) return;
    if (fade > 0 && shaderOk) {
        // The current shader carries on underneath while the new one fades in
        outgoing = std::move(shader);
        shader = std::make_unique<ISFShader>();
        outgoingTime = time;
        outgoingFrame = frameIndex;
        fadeStart = ofGetElapsedTimef();
        fadeSeconds = fade;
    }
    index = (newIndex % (int)library.size() + library.size()) % library.size();
    shaderOk = shader->load(library[index]);
    std::error_code ignored;
    loadedFileTime = std::filesystem::last_write_time(library[index], ignored);
    if (!shaderOk) {
        ofLogWarning("ShaderEngine") << shader->getName() << ": " << shader->getError();
        fbo.begin();
        ofClear(0, 255);
        fbo.end();
    }
    time = 0;
    frameIndex = 0;
    parametersVersion++;
}

void ShaderEngine::loadPath(const string &path, float fade) {
    auto found = std::find(library.begin(), library.end(), path);
    if (found == library.end()) {
        if (!ofFile::doesFileExist(path, false)) return;
        library.push_back(path);
        found = library.end() - 1;
    }
    load(found - library.begin(), fade);
}

void ShaderEngine::next() { load(index + 1); }
void ShaderEngine::previous() { load(index - 1); }
void ShaderEngine::select(int newIndex) { load(newIndex); }

void ShaderEngine::checkForChanges() {
    if (index < 0) return;
    std::error_code ignored;
    auto fileTime = std::filesystem::last_write_time(library[index], ignored);
    if (!ignored && fileTime != loadedFileTime) {
        ofLogNotice("ShaderEngine") << "Reloading " << shader->getName();
        load(index);
    }
}

string ShaderEngine::getStatus() const {
    if (library.empty()) return "no ISF generators found";
    string status = ofToString(index + 1) + "/" + ofToString(library.size()) + " " + shader->getName();
    if (!shaderOk) status += " (" + shader->getError() + ")";
    return status;
}
