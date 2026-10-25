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
    setNames = app->presets->listSets();
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
        if (ImGui::BeginTabItem("Sets")) {
            drawSets();
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

void LibraryApp::drawSets() {
    PresetManager &pm = *app->presets;

    // Which set
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    if (ImGui::BeginCombo("set", pm.setName.empty() ? "(none open)" : pm.setName.c_str())) {
        for (const string &name : setNames) {
            if (ImGui::Selectable(name.c_str(), name == pm.setName)) pm.openSet(name);
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    ImGui::InputTextWithHint("##newset", "new set name", newSetName, sizeof(newSetName));
    ImGui::SameLine();
    if (ImGui::Button("New set")) {
        pm.newSet(strlen(newSetName) ? string(newSetName) : "set " + ofGetTimestampString("%Y-%m-%d %H-%M"));
        newSetName[0] = 0;
        lastRefresh = -10;
    }
    if (pm.setName.empty()) {
        ImGui::TextDisabled("Open a set or make a new one.");
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete set")) {
        pm.removeSet(pm.setName);
        lastRefresh = -10;
        return;
    }
    ImGui::Separator();

    // Stepping through it (also MIDI notes 9 and 10, and the - and = keys)
    if (ImGui::Button("<< previous")) app->post([this]() { app->presets->previousInSet(); });
    ImGui::SameLine();
    if (ImGui::Button("next >>")) app->post([this]() { app->presets->nextInSet(); });
    ImGui::SameLine();
    if (pm.setPosition >= 0 && pm.setPosition < (int)pm.setEntries.size()) {
        ImGui::Text("%d/%d  %s", pm.setPosition + 1, (int)pm.setEntries.size(), pm.setEntries[pm.setPosition].preset.c_str());
    } else {
        ImGui::TextDisabled("not started");
    }

    // The entries: double-click to go there; a fade of "default" uses the preset fade
    bool changed = false;
    int moveUp = -1, moveDown = -1, removeAt = -1;
    if (ImGui::BeginTable("entries", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24);
        ImGui::TableSetupColumn("global preset", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("fade", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)pm.setEntries.size(); ++i) {
            PresetManager::SetEntry &entry = pm.setEntries[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i + 1);
            ImGui::TableNextColumn();
            if (ImGui::Selectable(entry.preset.c_str(), i == pm.setPosition, ImGuiSelectableFlags_AllowDoubleClick)
                && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                app->post([this, i]() { app->presets->goTo(i); });
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed |= ImGui::DragFloat("##fade", &entry.fade, 0.05f, -1, 30, entry.fade < 0 ? "default" : "%.1f s");
            ImGui::TableNextColumn();
            if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveUp = i;
            ImGui::SameLine(0, 2);
            if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveDown = i;
            ImGui::SameLine(0, 2);
            if (ImGui::SmallButton("x")) removeAt = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    auto &entries = pm.setEntries;
    if (moveUp > 0) { std::swap(entries[moveUp], entries[moveUp - 1]); changed = true; }
    if (moveDown >= 0 && moveDown + 1 < (int)entries.size()) { std::swap(entries[moveDown], entries[moveDown + 1]); changed = true; }
    if (removeAt >= 0) {
        entries.erase(entries.begin() + removeAt);
        if (pm.setPosition >= (int)entries.size()) pm.setPosition = (int)entries.size() - 1;
        changed = true;
    }

    // Adding: an existing global preset, or what's running now as a new one
    vector<string> &globals = presetNames[PRESET_GLOBAL];
    if (!globals.empty()) {
        globalToAdd = ofClamp(globalToAdd, 0, (int)globals.size() - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        if (ImGui::BeginCombo("##add", globals[globalToAdd].c_str())) {
            for (int i = 0; i < (int)globals.size(); ++i) {
                if (ImGui::Selectable(globals[i].c_str(), i == globalToAdd)) globalToAdd = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            entries.push_back({globals[globalToAdd], -1});
            changed = true;
        }
    }
    if (ImGui::Button("Add what's running now")) {
        // Saved as a new global preset named after the set, then added
        string name;
        for (int n = (int)entries.size() + 1; ; ++n) {
            name = pm.setName + " " + ofToString(n, 2, '0');
            if (std::find(globals.begin(), globals.end(), name) == globals.end()) break;
        }
        bool withLights = globalIncludesLights;
        app->post([this, name, withLights]() {
            app->presets->save(PRESET_GLOBAL, name, withLights);
            app->presets->setEntries.push_back({name, -1});
            app->presets->saveSet();
        });
        lastRefresh = -10;
    }
    ImGui::SameLine();
    ImGui::Checkbox("with lights", &globalIncludesLights);

    if (changed) pm.saveSet();
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

    // Shaders: every generator and Synesthesia scene found, static ones marked
    if (ImGui::CollapsingHeader("Shaders and Synesthesia scenes")) {
        string current = app->shaderEngine->getCurrentPath();
        shaderFilter.Draw("search##shader", -FLT_MIN);
        if (ImGui::BeginListBox("##shaders", ImVec2(-FLT_MIN, 12 * ImGui::GetTextLineHeightWithSpacing()))) {
            for (const ShaderEngine::Entry &entry : app->shaderEngine->getAllShaders()) {
                string label = entry.name + (entry.synesthesia ? "  [Synesthesia]" : "") + (entry.animated ? "" : "  (static)");
                if (!shaderFilter.PassFilter(label.c_str())) continue;
                // Names can repeat (a scene and its copy), so the path keeps each row apart
                if (ImGui::Selectable((label + "##" + entry.path).c_str(), entry.path == current, ImGuiSelectableFlags_AllowDoubleClick)
                    && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    string path = entry.path;
                    app->post([this, path]() { app->shaderEngine->loadPath(path); });
                }
                // Hovering a Synesthesia scene shows its preview
                if (!entry.thumbnail.empty() && ImGui::IsItemHovered()) {
                    if (const ofTexture *preview = thumbnail(entry.thumbnail)) {
                        ImGui::BeginTooltip();
                        ImGui::Image((ImTextureID)(uintptr_t)preview->getTextureData().textureID,
                                     ImVec2(preview->getWidth(), preview->getHeight()));
                        ImGui::EndTooltip();
                    }
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

const ofTexture *LibraryApp::thumbnail(const string &file) {
    auto found = thumbnails.find(file);
    if (found == thumbnails.end()) {
        ofTexture &texture = thumbnails[file];
        ofPixels pixels;
        if (ofLoadImage(pixels, file) && pixels.getWidth() > 0) {
            // About 320 wide is plenty, and keeps a few hundred of them small
            float scale = std::min(1.0f, 320.0f / pixels.getWidth());
            pixels.resize(pixels.getWidth() * scale, pixels.getHeight() * scale);
            texture.allocate(pixels, false);   // GL_TEXTURE_2D, as ImGui draws
            texture.loadData(pixels);
        }
        found = thumbnails.find(file);
    }
    return found->second.isAllocated() ? &found->second : nullptr;
}

void LibraryApp::keyPressed(int key) {
    // The library takes typing (preset names, search); other keys go to dots
    if (!ImGui::GetIO().WantTextInput) gui->keyPresses.push(key);
}
