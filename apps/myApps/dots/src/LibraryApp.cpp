//
//  LibraryApp.cpp
//  dots
//

#include "LibraryApp.h"
#include "ofApp.h"
#include "ofAppGLFWWindow.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

// Like the viewer, the library's close button only hides it
static void hideLibraryInsteadOfClosing(GLFWwindow *window) {
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwHideWindow(window);
}

static const char *moduleTitles[PRESET_N_MODULES] = {"Global", "Flames", "MilkDrop", "Shader", "Lights"};

void LibraryApp::setup() {
    ofSetWindowTitle("dots library");
    ofBackground(30);
    ofSetEscapeQuitsApp(false);
    imgui.setup(nullptr, true);

    window = static_cast<ofAppGLFWWindow *>(ofGetWindowPtr())->getGLFWWindow();
    glfwSetWindowCloseCallback(window, hideLibraryInsteadOfClosing);

    for (int m = 0; m < PRESET_N_MODULES; ++m) {
        selectedPreset[m] = -1;
        newName[m][0] = 0;
    }
}

void LibraryApp::scanMilkdropPresets() {
    // Every MilkDrop preset, relative to the presets folder
    string folder = app->projectM.getPresetsFolder();
    std::error_code ignored;
    for (auto &entry : std::filesystem::recursive_directory_iterator(folder, ignored)) {
        if (entry.path().extension() == ".milk") {
            milkdropPresets.push_back(ofFilePath::makeRelative(folder, entry.path().string()));
        }
    }
    std::sort(milkdropPresets.begin(), milkdropPresets.end());
}

void LibraryApp::update() {
    // This window sets up before ofApp, so wait for its engines and presets
    if (!app->presets) return;
    if (milkdropPresets.empty() && !app->projectM.getPresetsFolder().empty()) scanMilkdropPresets();
    // The GUI's "show library" checkbox and this window's close button stay in step
    bool visible = glfwGetWindowAttrib(window, GLFW_VISIBLE);
    if (gui->showLibrary != shown) {
        shown = gui->showLibrary;
        if (shown) glfwShowWindow(window); else glfwHideWindow(window);
    } else if (visible != shown) {
        shown = visible;
        gui->showLibrary = visible;
    }

    if (ofGetElapsedTimef() - lastRefresh > 1) {
        refreshLists();
        lastRefresh = ofGetElapsedTimef();
    }
}

void LibraryApp::refreshLists() {
    for (int m = 0; m < PRESET_N_MODULES; ++m) {
        presetNames[m] = app->presets->list(m);
    }
    lightSnapshots.clear();
    ofDirectory dir(ofToDataPath("lights", true));
    if (dir.exists()) {
        dir.allowExt("xml");
        dir.listDir();
        for (auto &file : dir.getFiles()) {
            string base = file.getBaseName();   // snapshot-N
            if (base.rfind("snapshot-", 0) == 0) lightSnapshots.push_back(ofToInt(base.substr(9)));
        }
        std::sort(lightSnapshots.begin(), lightSnapshots.end());
    }
}

void LibraryApp::draw() {
    if (!app->presets) return;
    imgui.begin();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(ofGetWidth(), ofGetHeight()));
    ImGui::Begin("dots library", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    if (ImGui::BeginTabBar("library")) {
        if (ImGui::BeginTabItem("Presets")) {
            drawPresets();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Files")) {
            drawFiles();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    imgui.end();
}

void LibraryApp::drawPresets() {
    float fade = gui->presetFadeSeconds;
    if (ImGui::SliderFloat("fade (s)", &fade, 0, 10, "%.1f")) gui->presetFadeSeconds = fade;
    ImGui::TextDisabled("Double-click a preset to recall it.");
    for (int m = 0; m < PRESET_N_MODULES; ++m) {
        drawModulePresets(m, moduleTitles[m]);
    }
}

void LibraryApp::drawModulePresets(int module, const string &title) {
    ImGui::PushID(module);
    if (ImGui::CollapsingHeader(title.c_str(), module == PRESET_GLOBAL ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        vector<string> &names = presetNames[module];
        int &selected = selectedPreset[module];
        if (selected >= (int)names.size()) selected = -1;

        if (ImGui::BeginListBox("##presets", ImVec2(-FLT_MIN, 6 * ImGui::GetTextLineHeightWithSpacing()))) {
            for (int i = 0; i < (int)names.size(); ++i) {
                if (ImGui::Selectable(names[i].c_str(), selected == i, ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected = i;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        string name = names[i];
                        app->post([this, module, name]() { app->presets->recall(module, name); });
                    }
                }
            }
            ImGui::EndListBox();
        }

        bool includeLights = globalIncludesLights;
        if (module == PRESET_GLOBAL) {
            ImGui::Checkbox("save with lights", &globalIncludesLights);
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::InputTextWithHint("##name", "new preset name", newName[module], sizeof(newName[module]));
        ImGui::SameLine();
        if (ImGui::Button("Save new")) {
            string name = newName[module];
            app->post([this, module, name, includeLights]() { app->presets->save(module, name.empty() ? ofGetTimestampString("%Y-%m-%d %H-%M-%S") : name, includeLights); });
            newName[module][0] = 0;
            lastRefresh = -10;
        }
        if (selected >= 0) {
            string name = names[selected];
            if (ImGui::Button("Recall")) {
                app->post([this, module, name]() { app->presets->recall(module, name); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Overwrite")) {
                app->post([this, module, name, includeLights]() { app->presets->save(module, name, includeLights); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                app->presets->remove(module, name);
                selected = -1;
                lastRefresh = -10;
            }
        }
    }
    ImGui::PopID();
}

void LibraryApp::drawFiles() {
    // Flames: tracks and their scenes
    if (ImGui::CollapsingHeader("Flames tracks")) {
        StateManager &sm = *app->stateManager;
        for (int t = 0; t < (int)sm.tracks.size(); ++t) {
            DotsTrack &track = sm.tracks[t];
            string label = "track " + ofToString(track.trackId) + "  (" + ofToString(track.scenes.size()) + " scenes)";
            ImGuiTreeNodeFlags flags = t == sm.trackIdx ? ImGuiTreeNodeFlags_Selected : 0;
            if (ImGui::TreeNodeEx(("##track" + ofToString(t)).c_str(), flags, "%s", label.c_str())) {
                for (int s = 0; s < (int)track.scenes.size(); ++s) {
                    bool current = t == sm.trackIdx && s == sm.sceneIdx;
                    if (ImGui::Selectable(("scene " + ofToString(s) + "##" + ofToString(t)).c_str(), current)) {
                        app->post([this, t, s]() {
                            if (app->stateManager->trackIdx != t) app->stateManager->selectTrack(t);
                            app->stateManager->selectScene(s);
                        });
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    // MilkDrop presets, searchable
    if (ImGui::CollapsingHeader("MilkDrop presets")) {
        string current = ofFilePath::makeRelative(app->projectM.getPresetsFolder(), app->projectM.getPresetPath());
        milkdropFilter.Draw("search##milk", -FLT_MIN);
        vector<int> shown;
        for (int i = 0; i < (int)milkdropPresets.size(); ++i) {
            if (milkdropFilter.PassFilter(milkdropPresets[i].c_str())) shown.push_back(i);
        }
        ImGui::TextDisabled("%d of %d; double-click to load", (int)shown.size(), (int)milkdropPresets.size());
        if (ImGui::BeginListBox("##milk", ImVec2(-FLT_MIN, 14 * ImGui::GetTextLineHeightWithSpacing()))) {
            ImGuiListClipper clipper;
            clipper.Begin(shown.size());
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const string &preset = milkdropPresets[shown[row]];
                    if (ImGui::Selectable(preset.c_str(), preset == current, ImGuiSelectableFlags_AllowDoubleClick)
                        && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        string path = ofFilePath::join(app->projectM.getPresetsFolder(), preset);
                        app->post([this, path]() { app->projectM.loadPresetFile(path, true); });
                    }
                }
            }
            ImGui::EndListBox();
        }
    }

    // Shaders: every generator found, static ones marked
    if (ImGui::CollapsingHeader("Shaders")) {
        string current = app->shaderEngine->getCurrentPath();
        shaderFilter.Draw("search##shader", -FLT_MIN);
        if (ImGui::BeginListBox("##shaders", ImVec2(-FLT_MIN, 12 * ImGui::GetTextLineHeightWithSpacing()))) {
            for (const ShaderEngine::Entry &entry : app->shaderEngine->getAllShaders()) {
                string label = ofFilePath::getBaseName(entry.path) + (entry.animated ? "" : "  (static)");
                if (!shaderFilter.PassFilter(label.c_str())) continue;
                if (ImGui::Selectable(label.c_str(), entry.path == current, ImGuiSelectableFlags_AllowDoubleClick)
                    && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    string path = entry.path;
                    app->post([this, path]() { app->shaderEngine->loadPath(path); });
                }
            }
            ImGui::EndListBox();
        }
    }

    // Light snapshots (the venue's lighting)
    if (ImGui::CollapsingHeader("Light snapshots")) {
        for (int slot : lightSnapshots) {
            if (ImGui::Selectable(("snapshot " + ofToString(slot)).c_str(), app->lighting->snapshot == slot, ImGuiSelectableFlags_AllowDoubleClick)
                && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                app->lighting->snapshot = slot;
                app->lighting->loadSnapshot(slot);
            }
        }
        if (lightSnapshots.empty()) ImGui::TextDisabled("none saved yet");
    }

    // The folders themselves
    if (ImGui::CollapsingHeader("Folders")) {
        auto reveal = [](const string &label, const string &path) {
            if (ImGui::Button(label.c_str())) {
                ofDirectory::createDirectory(path, false, true);
                ofSystem("open \"" + path + "\"");
            }
        };
        reveal("Presets", ofToDataPath("presets", true));
        reveal("Flames tracks", ofToDataPath("params", true));
        reveal("MilkDrop presets", app->projectM.getPresetsFolder());
        reveal("My shaders (bin/data/shaders)", ofToDataPath("shaders", true));
        reveal("System ISF shaders", "/Library/Graphics/ISF");
        reveal("Light snapshots", ofToDataPath("lights", true));
    }
}

void LibraryApp::keyPressed(int key) {
    // The library takes typing (preset names, search); other keys go to dots
    if (!ImGui::GetIO().WantTextInput) gui->keyPresses.push(key);
}
