//
//  MediaInput.cpp
//  dots
//

#include "MediaInput.h"

void MediaInput::setup() {
    directory.setup();
}

void MediaInput::select(int newKind) {
    kind = ofClamp(newKind, NONE, N_KINDS - 1);
    if (kind == SYPHON && !syphonServer.empty() && !syphon.isSetup()) useSyphon(syphonServer, syphonApp);
}

bool MediaInput::isVideo(const string &path) {
    string extension = ofToLower(ofFilePath::getFileExt(path));
    return extension == "mov" || extension == "mp4" || extension == "m4v";
}

bool MediaInput::loadFile(const string &path) {
    file = path;
    media = ShaderMedia();   // so a new image is drawn
    fileIsVideo = isVideo(path);
    video.close();
    image.clear();
    if (fileIsVideo) {
        fileOk = video.load(path);
        if (fileOk) {
            video.setVolume(0);
            video.setLoopState(OF_LOOP_NORMAL);
            video.play();
        }
    } else {
        fileOk = image.load(path);
    }
    if (!fileOk) ofLogWarning("MediaInput") << "Can't open " << path;
    kind = FILE;
    return fileOk;
}

void MediaInput::useSyphon(const string &serverName, const string &appName) {
    syphonServer = serverName;
    syphonApp = appName;
    if (!syphon.isSetup()) syphon.setup();
    syphon.set(serverName, appName);
    kind = SYPHON;
}

void MediaInput::draw(float width, float height, bool flip, int type, std::function<void(float, float, float, float)> drawSource) {
    if (width < 1 || height < 1) {
        media = ShaderMedia();
        return;
    }
    if (!fbo.isAllocated() || fbo.getWidth() != (int)width || fbo.getHeight() != (int)height) {
        ofFboSettings settings;
        settings.width = width;
        settings.height = height;
        settings.internalformat = GL_RGBA;
        settings.textureTarget = GL_TEXTURE_2D;   // scenes sample it as a sampler2D
        settings.minFilter = GL_LINEAR;
        settings.maxFilter = GL_LINEAR;
        settings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
        settings.wrapModeVertical = GL_CLAMP_TO_EDGE;
        fbo.allocate(settings);
    }
    ofPushView();
    fbo.begin();
    ofClear(0, 255);
    ofPushStyle();
    ofDisableAlphaBlending();
    ofSetColor(255);
    if (flip) drawSource(0, height, width, -height);
    else drawSource(0, 0, width, height);
    ofPopStyle();
    fbo.end();
    ofPopView();
    media.texture = &fbo.getTexture();
    media.type = type;
}

void MediaInput::update(ofFbo &flames, ofFbo &milkdrop) {
    switch (kind) {
        case FLAMES:
        case MILKDROP: {
            ofFbo &source = kind == FLAMES ? flames : milkdrop;
            draw(source.getWidth(), source.getHeight(), false, 3, [&source](float x, float y, float w, float h) { source.draw(x, y, w, h); });
            break;
        }
        case SYPHON:
            if (!syphon.isSetup()) {
                media = ShaderMedia();
                break;
            }
            // The frame's size is only known once it's locked
            if (syphon.lockTexture()) {
                ofTexture &frame = syphon.getTexture();
                draw(frame.getWidth(), frame.getHeight(), false, 3, [&frame](float x, float y, float w, float h) { frame.draw(x, y, w, h); });
                syphon.unlockTexture();
            } else {
                media = ShaderMedia();
            }
            break;
        case FILE:
            if (!fileOk) {
                media = ShaderMedia();
            } else if (fileIsVideo) {
                video.update();
                draw(video.getWidth(), video.getHeight(), false, 2, [this](float x, float y, float w, float h) { video.draw(x, y, w, h); });
            } else {
                // Only redrawn when it changes size; a still image doesn't move
                if (!media.texture || fbo.getWidth() != (int)image.getWidth() || media.type != 1) {
                    draw(image.getWidth(), image.getHeight(), true, 1, [this](float x, float y, float w, float h) { image.draw(x, y, w, h); });
                }
            }
            break;
        default:
            media = ShaderMedia();
    }
}

string MediaInput::describe() const {
    switch (kind) {
        case FLAMES: return "flames";
        case MILKDROP: return "MilkDrop";
        case SYPHON: return syphonServer.empty() && syphonApp.empty() ? "Syphon (none chosen)" : "Syphon " + syphonApp + (syphonServer.empty() ? "" : " " + syphonServer);
        case FILE: return file.empty() ? "file (none chosen)" : ofFilePath::getFileName(file) + (fileOk ? "" : " (can't open)");
        default: return "none";
    }
}

vector<string> MediaInput::listFiles() {
    vector<string> folders;
    string synData = ofFilePath::join(ofFilePath::getUserHomeDir(), "Library/Application Support/com.gravitycurrent.synesthesia/Data/media.json");
    ofJson json = ofLoadJson(synData);
    if (json.is_object() && json.contains("sourceFolders") && json["sourceFolders"].is_array()) {
        for (auto &folder : json["sourceFolders"]) {
            if (folder.is_string()) folders.push_back(folder.get<string>());
        }
    }
    folders.push_back(ofToDataPath("media", true));

    vector<string> files;
    for (const string &folder : folders) {
        std::error_code ignored;
        if (!std::filesystem::is_directory(folder, ignored)) continue;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        for (auto it = std::filesystem::recursive_directory_iterator(folder, options, ignored);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ignored)) {
            if (it.depth() > 2) it.disable_recursion_pending();
            if (!it->is_regular_file(ignored)) continue;
            string path = it->path().string();
            string extension = ofToLower(ofFilePath::getFileExt(path));
            if (isVideo(path) || extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "gif") {
                files.push_back(path);
            }
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}
