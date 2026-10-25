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
    fbo.allocate(width, height, GL_RGBA);
    fbo.begin();
    ofClear(0, 255);
    fbo.end();
}

void ShaderEngine::render() {
    float now = ofGetElapsedTimef();
    float delta = lastFrameTime > 0 ? std::min(now - lastFrameTime, 0.25f) : 0;
    lastFrameTime = now;
    if (!shaderOk) return;
    time += delta;
    shader.render(fbo, time, delta, frameIndex++);
}

void ShaderEngine::load(int newIndex) {
    if (library.empty()) return;
    index = (newIndex % (int)library.size() + library.size()) % library.size();
    shaderOk = shader.load(library[index]);
    std::error_code ignored;
    loadedFileTime = std::filesystem::last_write_time(library[index], ignored);
    if (!shaderOk) {
        ofLogWarning("ShaderEngine") << shader.getName() << ": " << shader.getError();
        fbo.begin();
        ofClear(0, 255);
        fbo.end();
    }
    time = 0;
    frameIndex = 0;
    parametersVersion++;
}

void ShaderEngine::loadPath(const string &path) {
    auto found = std::find(library.begin(), library.end(), path);
    if (found == library.end()) {
        if (!ofFile::doesFileExist(path, false)) return;
        library.push_back(path);
        found = library.end() - 1;
    }
    load(found - library.begin());
}

void ShaderEngine::next() { load(index + 1); }
void ShaderEngine::previous() { load(index - 1); }
void ShaderEngine::select(int newIndex) { load(newIndex); }

void ShaderEngine::checkForChanges() {
    if (index < 0) return;
    std::error_code ignored;
    auto fileTime = std::filesystem::last_write_time(library[index], ignored);
    if (!ignored && fileTime != loadedFileTime) {
        ofLogNotice("ShaderEngine") << "Reloading " << shader.getName();
        load(index);
    }
}

string ShaderEngine::getStatus() const {
    if (library.empty()) return "no ISF generators found";
    string status = ofToString(index + 1) + "/" + ofToString(library.size()) + " " + shader.getName();
    if (!shaderOk) status += " (" + shader.getError() + ")";
    return status;
}
