//
//  ShaderEngine.cpp
//  dots
//

#include "ShaderEngine.h"
#include "ISFShader.h"
#include "SynScene.h"
#include "SynAudio.h"

void ShaderEngine::setup(int width, int height, const SynAudio *synAudio) {
    audio = synAudio;
    shader = makeSource("");
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
                string path = file.getAbsolutePath();
                allShaders.push_back({path, ofFilePath::getBaseName(path), kind == ISFShader::ANIMATED_GENERATOR, false});
            }
        }
    }

    // Synesthesia's own scene folder (from its preferences), then the scenes that come with it
    string synData = ofFilePath::join(ofFilePath::getUserHomeDir(), "Library/Application Support/com.gravitycurrent.synesthesia");
    ofJson preferences = ofLoadJson(ofFilePath::join(synData, "Data/preferences.json"));
    if (preferences.is_object() && preferences.contains("PRIMARY_SCENE_FOLDER") && preferences["PRIMARY_SCENE_FOLDER"].is_string()) {
        addSynesthesiaFolder(preferences["PRIMARY_SCENE_FOLDER"].get<string>());
    }
    addSynesthesiaFolder(ofFilePath::join(synData, "Scenes"));
    addSynesthesiaFolder("/Applications/Synesthesia.app/Contents/Resources/release");
    addSynesthesiaFolder(ofToDataPath("synesthesia", true));

    std::sort(allShaders.begin(), allShaders.end(), [](const Entry &a, const Entry &b) {
        return ofToLower(a.name) < ofToLower(b.name);
    });

    resize(width, height);
    setFilter(animatedOnly, sources);
}

void ShaderEngine::addSynesthesiaFolder(const string &folder) {
    ofDirectory dir(folder);
    if (!dir.exists()) return;
    dir.listDir();
    for (auto &file : dir.getFiles()) {
        string path = file.getAbsolutePath();
        if (!file.isDirectory() || ofToLower(file.getExtension()) != "synscene" || !SynScene::isScene(path)) continue;
        ofJson json = ofLoadJson(ofFilePath::join(path, "scene.json"));
        string title = json.is_object() && json.contains("TITLE") && json["TITLE"].is_string() ? json["TITLE"].get<string>() : "";
        string thumbnail = json.is_object() && json.contains("IMAGE_PATH") && json["IMAGE_PATH"].is_string() ? json["IMAGE_PATH"].get<string>() : "";
        if (!thumbnail.empty()) thumbnail = ofFilePath::join(path, thumbnail);
        if (!ofFile::doesFileExist(thumbnail, false)) thumbnail.clear();
        allShaders.push_back({path, title.empty() ? ofFilePath::getBaseName(path) : title, true, true, thumbnail});
    }
}

std::unique_ptr<ShaderSource> ShaderEngine::makeSource(const string &path) {
    if (!path.empty() && SynScene::isScene(path)) return std::make_unique<SynScene>(audio);
    return std::make_unique<ISFShader>();
}

// A scene is a folder: the newest of its files
std::filesystem::file_time_type ShaderEngine::fileTime(const string &path) {
    std::error_code ignored;
    if (!std::filesystem::is_directory(path, ignored)) return std::filesystem::last_write_time(path, ignored);
    std::filesystem::file_time_type newest{};
    for (const char *file : {"main.glsl", "scene.json", "script.js"}) {
        auto time = std::filesystem::last_write_time(ofFilePath::join(path, file), ignored);
        if (!ignored && time > newest) newest = time;
    }
    return newest;
}

void ShaderEngine::setFilter(bool only, int which) {
    animatedOnly = only;
    sources = which;
    string current = index >= 0 && index < (int)library.size() ? library[index] : "";
    library.clear();
    int isf = 0, syn = 0;
    for (const Entry &entry : allShaders) {
        if (!entry.animated && animatedOnly) continue;
        if (entry.synesthesia ? sources == ISF_ONLY : sources == SYNESTHESIA_ONLY) continue;
        library.push_back(entry.path);
        (entry.synesthesia ? syn : isf)++;
    }
    ofLogNotice("ShaderEngine") << isf << " ISF generators" << (animatedOnly ? " (animated only)" : "")
                                << ", " << syn << " Synesthesia scenes";

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
        outgoingTime = time;
        outgoingFrame = frameIndex;
        fadeStart = ofGetElapsedTimef();
        fadeSeconds = fade;
    }
    index = (newIndex % (int)library.size() + library.size()) % library.size();
    shader = makeSource(library[index]);
    shaderOk = shader->load(library[index]);
    loadedFileTime = fileTime(library[index]);
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
        std::error_code ignored;
        if (!std::filesystem::exists(path, ignored)) return;
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
    if (fileTime(library[index]) != loadedFileTime) {
        ofLogNotice("ShaderEngine") << "Reloading " << shader->getName();
        load(index);
    }
}

string ShaderEngine::getStatus() const {
    if (library.empty()) return "no shaders found";
    string status = ofToString(index + 1) + "/" + ofToString(library.size()) + " " + shader->getName();
    if (!shaderOk) status += " (" + shader->getError() + ")";
    return status;
}

// Test audio: a kick on each beat at 120 BPM, a hi-hat between, and a chord underneath
static void testSignal(SynAudio &audio, float startSeconds, float seconds) {
    const float rate = 48000;
    vector<float> samples(seconds * rate);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        float t = startSeconds + i / rate;
        float beat = std::fmod(t, 0.5f), offbeat = std::fmod(t + 0.25f, 0.5f);
        float kick = std::sin(TWO_PI * (50 + 80 * std::exp(-beat * 30)) * beat) * std::exp(-beat * 12);
        float hat = ofRandom(-1, 1) * std::exp(-offbeat * 60) * 0.3f;
        float chord = 0.1f * (std::sin(TWO_PI * 220 * t) + std::sin(TWO_PI * 277 * t) + std::sin(TWO_PI * 330 * t));
        samples[i] = 0.6f * kick + hat + chord;
    }
    for (std::size_t i = 0; i < samples.size(); i += 512) {
        audio.process(samples.data() + i, std::min<std::size_t>(512, samples.size() - i), rate);
    }
}

void ShaderEngine::testAll(bool includeISF) {
    ofFbo target;
    target.allocate(640, 360, GL_RGBA);
    int ok = 0, failed = 0, black = 0;
    // DOTS_TEST_SHOTS=folder saves the last frame of each, and contact sheets of them all
    const char *shots = getenv("DOTS_TEST_SHOTS");
    vector<std::pair<string, ofImage>> thumbnails;
    SynAudio testAudio;
    const SynAudio *liveAudio = audio;
    audio = &testAudio;
    testSignal(testAudio, 0, 8);   // warm the analysis up
    {
        // What the analysis makes of it (120 BPM, a beat every half second)
        vector<float> values = testAudio.values();
        string line;
        for (std::size_t i = 0; i < values.size(); ++i) line += SynAudio::names()[i] + "=" + ofToString(values[i], 2) + " ";
        std::cout << "[scene-test] audio: " << line << std::endl;
    }
    for (const Entry &entry : allShaders) {
        if (!entry.synesthesia && !includeISF) continue;
        std::unique_ptr<ShaderSource> source = makeSource(entry.path);
        if (!source->load(entry.path)) {
            std::cout << "[scene-test] FAIL  " << entry.name << "  (" << entry.path << "): " << source->getError() << std::endl;
            failed++;
            continue;
        }
        uint64_t start = ofGetElapsedTimeMicros(), audioMicros = 0;
        for (int frame = 0; frame < 30; ++frame) {
            uint64_t audioStart = ofGetElapsedTimeMicros();
            testSignal(testAudio, 8 + frame / 30.0f, 1 / 30.0f);
            audioMicros += ofGetElapsedTimeMicros() - audioStart;
            source->render(target, frame / 30.0f, 1 / 30.0f, frame);
        }
        glFinish();
        float msPerFrame = (ofGetElapsedTimeMicros() - start - audioMicros) / 30000.0f;
        GLenum glError = glGetError();
        ofPixels pixels;
        target.readToPixels(pixels);
        double sum = 0;
        for (std::size_t i = 0; i + 2 < pixels.size(); i += pixels.getNumChannels()) sum += pixels[i] + pixels[i + 1] + pixels[i + 2];
        float mean = sum / (3.0 * pixels.getWidth() * pixels.getHeight());
        bool isBlack = mean < 0.5f;
        if (shots) {
            ofImage shot;
            shot.setFromPixels(pixels);
            shot.setImageType(OF_IMAGE_COLOR);
            shot.save(ofFilePath::join(shots, ofFilePath::getBaseName(entry.path) + ".png"));
            shot.resize(240, 135);
            thumbnails.push_back({entry.name, shot});
        }
        (isBlack ? black : ok)++;
        std::cout << "[scene-test] " << (isBlack ? "BLACK " : "OK    ") << entry.name << "  mean " << ofToString(mean, 1)
                  << "  " << ofToString(msPerFrame, 1) << " ms" << (glError ? "  GL error " + ofToString(glError) : "") << std::endl;
    }
    audio = liveAudio;

    // Sheets of 48: 8 across, 6 down, each with its name
    const int across = 8, down = 6, cellW = 240, cellH = 150;
    for (std::size_t first = 0; first < thumbnails.size(); first += across * down) {
        ofFbo sheet;
        sheet.allocate(across * cellW, down * cellH, GL_RGB);
        sheet.begin();
        ofClear(40, 255);
        for (std::size_t i = first; i < std::min(thumbnails.size(), first + across * down); ++i) {
            int cell = i - first;
            float x = (cell % across) * cellW, y = (cell / across) * cellH;
            ofSetColor(255);
            thumbnails[i].second.draw(x, y);
            ofDrawBitmapString(thumbnails[i].first.substr(0, 29), x + 2, y + 146);
        }
        sheet.end();
        ofPixels pixels;
        sheet.readToPixels(pixels);
        ofSaveImage(pixels, ofFilePath::join(shots, "sheet-" + ofToString(first / (across * down) + 1) + ".png"));
    }
    std::cout << "[scene-test] " << ok << " ok, " << black << " black after 30 frames, " << failed << " failed" << std::endl;
}
