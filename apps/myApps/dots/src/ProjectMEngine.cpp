//
//  ProjectMEngine.cpp
//  dots
//

#include "ProjectMEngine.h"

// About a third of a second of 44.1/48 kHz audio; older samples are dropped
// if the main thread falls behind
static const size_t MAX_PENDING_SAMPLES = 16384;

static const char *resetVertexShader = R"(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
void main() { gl_Position = modelViewProjectionMatrix * position; }
)";

static const char *resetFragmentShader = R"(#version 150
out vec4 outputColor;
void main() { outputColor = vec4(1.0); }
)";

ProjectMEngine::~ProjectMEngine() {
    if (playlist) projectm_playlist_destroy(playlist);
    if (projectM) projectm_destroy(projectM);
}

void ProjectMEngine::setup(int width, int height, const string &dataPath) {
    projectM = projectm_create();
    if (!projectM) {
        ofLogError("ProjectMEngine") << "projectm_create failed; MilkDrop is off";
        return;
    }

    projectm_set_window_size(projectM, width, height);
    projectm_set_fps(projectM, 30);
    projectm_set_mesh_size(projectM, 64, 48);
    projectm_set_soft_cut_duration(projectM, 3);
    projectm_set_preset_duration(projectM, 30);
    projectm_set_hard_cut_enabled(projectM, false);

    string textures = ofFilePath::join(dataPath, "textures");
    string texturesInner = ofFilePath::join(textures, "textures");
    const char *texturePaths[] = {textures.c_str(), texturesInner.c_str()};
    projectm_set_texture_search_paths(projectM, texturePaths, 2);

    playlist = projectm_playlist_create(projectM);
    projectm_playlist_set_shuffle(playlist, true);
    projectm_playlist_set_preset_switched_event_callback(playlist, &ProjectMEngine::presetSwitched, this);
    string presets = ofFilePath::join(dataPath, "presets");
    presetsFolder = presets;
    uint32_t added = projectm_playlist_add_path(playlist, presets.c_str(), true, false);
    ofLogNotice("ProjectMEngine") << added << " presets from " << presets;
    if (added > 0) nextPreset(true);

    resetShader.setupShaderFromSource(GL_VERTEX_SHADER, resetVertexShader);
    resetShader.setupShaderFromSource(GL_FRAGMENT_SHADER, resetFragmentShader);
    resetShader.bindDefaults();
    resetShader.linkProgram();

    resize(width, height);
}

void ProjectMEngine::resize(int width, int height) {
    fbo.allocate(width, height, GL_RGBA);
    fbo.begin();
    ofClear(0, 255);
    fbo.end();
    if (projectM) projectm_set_window_size(projectM, width, height);
}

void ProjectMEngine::addSamples(const float *samples, int count) {
    std::lock_guard<std::mutex> lock(audioMutex);
    pendingSamples.insert(pendingSamples.end(), samples, samples + count);
    if (pendingSamples.size() > MAX_PENDING_SAMPLES) {
        pendingSamples.erase(pendingSamples.begin(), pendingSamples.end() - MAX_PENDING_SAMPLES);
    }
}

void ProjectMEngine::render() {
    if (!projectM) return;

    {
        std::lock_guard<std::mutex> lock(audioMutex);
        renderSamples.swap(pendingSamples);
        pendingSamples.clear();
    }
    if (!renderSamples.empty()) {
        projectm_pcm_add_float(projectM, renderSamples.data(), renderSamples.size(), PROJECTM_MONO);
    }

    projectm_opengl_render_frame_fbo(projectM, fbo.getId());

    // Put back the GL state openFrameworks expects
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_DEPTH_TEST);
    resetShader.begin();
    resetShader.end();
}

void ProjectMEngine::nextPreset(bool hardCut) {
    if (playlist) projectm_playlist_play_next(playlist, hardCut);
}

void ProjectMEngine::previousPreset(bool hardCut) {
    if (playlist) projectm_playlist_play_previous(playlist, hardCut);
}

void ProjectMEngine::setPresetDuration(double seconds) {
    if (projectM) projectm_set_preset_duration(projectM, seconds);
}

void ProjectMEngine::setPresetLocked(bool locked) {
    if (projectM) projectm_set_preset_locked(projectM, locked);
}

void ProjectMEngine::setShuffle(bool shuffle) {
    if (playlist) projectm_playlist_set_shuffle(playlist, shuffle);
}

string ProjectMEngine::getPresetName() {
    std::lock_guard<std::mutex> lock(nameMutex);
    return presetName;
}

string ProjectMEngine::getPresetPath() {
    std::lock_guard<std::mutex> lock(nameMutex);
    return presetPath;
}

void ProjectMEngine::setSoftCutDuration(double seconds) {
    if (projectM) projectm_set_soft_cut_duration(projectM, seconds);
}

void ProjectMEngine::setBeatSensitivity(float sensitivity) {
    if (projectM) projectm_set_beat_sensitivity(projectM, sensitivity);
}

void ProjectMEngine::setHardCuts(bool enabled, float sensitivity, double seconds) {
    if (!projectM) return;
    projectm_set_hard_cut_enabled(projectM, enabled);
    projectm_set_hard_cut_sensitivity(projectM, sensitivity);
    projectm_set_hard_cut_duration(projectM, seconds);
}

void ProjectMEngine::setMeshSize(int across) {
    if (projectM) projectm_set_mesh_size(projectM, across, std::max(1, across * 3 / 4));
}

void ProjectMEngine::setAspectCorrection(bool enabled) {
    if (projectM) projectm_set_aspect_correction(projectM, enabled);
}

void ProjectMEngine::loadPresetFile(const string &path, bool smooth) {
    if (!projectM || !ofFile::doesFileExist(path, false)) return;
    projectm_load_preset_file(projectM, path.c_str(), smooth);
    std::lock_guard<std::mutex> lock(nameMutex);
    presetName = ofFilePath::getBaseName(path);
    presetPath = path;
}

void ProjectMEngine::presetSwitched(bool isHardCut, unsigned int index, void *userData) {
    ProjectMEngine *engine = static_cast<ProjectMEngine *>(userData);
    char *path = projectm_playlist_item(engine->playlist, index);
    if (!path) return;
    string fullPath = path;
    projectm_playlist_free_string(path);

    std::lock_guard<std::mutex> lock(engine->nameMutex);
    engine->presetName = ofFilePath::getBaseName(fullPath);
    engine->presetPath = fullPath;
}
